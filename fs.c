#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

#define INODE_LINKS_LIMIT ((sb->blksz-32)/sizeof(uint64_t))
#define NODEINFO_NAME_LIMIT (sb->blksz - 65)

unsigned long fsize(int fd) { // Obtem o tamanho do disco
    unsigned long len = (unsigned long) lseek(fd, 0, SEEK_END);
    return len;
}

// Posiciona o cursos no local onde deve escrever e escreve no disco
void escreverNoDisco(struct superblock* sb, void * data, uint64_t pos, size_t tam) {
	lseek(sb->fd, (pos*sb->blksz), SEEK_SET);
	write(sb->fd, data, tam);
}
// Posiciona o cursos no local onde deve ler e lê do disco
void lerDoDisco(struct superblock* sb, void * data, uint64_t pos, size_t tam) {
	lseek(sb->fd, (pos*sb->blksz), SEEK_SET);
	read(sb->fd, data, tam);
}
// Navega entre os diretórios para encontrar o último inode
parDiretorioNome* procuraDiretorio(struct superblock *sb, const char* fname) {
	parDiretorioNome *pda;
	char *fnameCopy, *strAux, *strAux2;
	int dirCount, i;
	uint64_t dirCorrente;

	
	fnameCopy = (char *) malloc((strlen(fname) + 1) * sizeof(char));
	strcpy(fnameCopy, fname); // Copia o caminho para uma string que pode ser modificada
	
	dirCount = 0; // Conta o número de diretorios que devem ser navegados antes de criar o diretorio
	strAux = strtok(fnameCopy,"/");
	while(strAux != NULL) {  // Faz a contagem
		dirCount++;
		strAux = strtok(NULL,"/");
	}
	
	strcpy(fnameCopy, fname);
	strAux = strtok(fnameCopy,"/");
	i=1;
	dirCorrente = sb->root;
	errno = 0;
	while(i<=dirCount) { // Todos os diretorios serao resolvidos dentro desse while
		struct inode *dirInode, *fileInode;
		struct nodeinfo *dirInfo, *fileInfo;
		uint64_t j;
		char achou = 0;
		dirInode = (struct inode*) malloc(sb->blksz);
		fileInode = (struct inode*) malloc(sb->blksz);
		dirInfo = (struct nodeinfo*) malloc(sb->blksz);
		fileInfo = (struct nodeinfo*) malloc(sb->blksz);

		lerDoDisco(sb, (void *) dirInode, dirCorrente, sb->blksz);
		lerDoDisco(sb, (void *) dirInfo, dirInode->meta, sb->blksz);
		for(j=0; j < dirInfo->size; j++) {
			if(j != 0 && j%INODE_LINKS_LIMIT == 0) {
				lerDoDisco(sb, (void *) dirInode, dirInode->next, sb->blksz);
			}
			lerDoDisco(sb, (void *) fileInode, dirInode->links[j%INODE_LINKS_LIMIT], sb->blksz);
			lerDoDisco(sb, (void *) fileInfo, fileInode->meta, sb->blksz);
			if(i!=dirCount) { // Checa se o arquivo já existe
				if(!strcmp(strAux, fileInfo->name)) {
					if(fileInode->mode == IMDIR) {
						dirCorrente = dirInode->links[j%INODE_LINKS_LIMIT];
						achou = 1;
						break;
					} else {
						errno = ENOTDIR;
						break;
					}
				}
			} else {
				if(!strcmp(strAux, fileInfo->name)) {
					errno = EEXIST;
					break;
				}
			}
		}

		free(dirInode);
		free(fileInode);
		free(dirInfo);
		free(fileInfo);
		if(i!=dirCount && !achou) {
			errno = ENOENT;
		}
		if(errno)
			break;
		if(i!=dirCount)
			strAux = strtok(NULL,"/");
		i++;
	}
	if(errno) {
		free(fnameCopy);
		return NULL;
	}
	if(strlen(strAux) > sb->blksz - 65) { // Checa se o nome é maior do que o fs aceita
		errno = ENAMETOOLONG;
		free(fnameCopy);
		return NULL;
	}
	
	pda = (parDiretorioNome *) malloc(sizeof(parDiretorioNome)); // Aloca estrutura de retorno
	pda->dirInode = dirCorrente;
	pda->arq = (char *) malloc((strlen(strAux) + 1)*sizeof(char));
	strcpy(pda->arq, strAux);
	free(fnameCopy);

	return pda;
}
// Abre o disco e coloca o descritor em sb->fd
struct superblock* openDisk(const char *fname) {
	struct superblock* sb;
	sb = (struct superblock *) malloc(sizeof(struct superblock)); // Criando estrutura do superbloco

	sb->fd = open(fname, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); // Abre o arquivo para manipulação. Nesse ponto, só é necessário que o programa tenha permissão de escrita.
	if(flock(sb->fd, LOCK_EX|LOCK_NB)==-1){
		errno = EBUSY;
		free(sb);
		return NULL;
	}
	return sb;
}

struct superblock * fs_format(const char *fname, uint64_t blocksize){
	if (blocksize < MIN_BLOCK_SIZE) { // Verifica se o valor do blocksize é maior que o minimo.
        errno = EINVAL;
        return NULL;
    }
	struct superblock* sb = openDisk(fname);
	if(sb == NULL)
		return;

	sb->magic = 0xdcc605f5; // Atribuindo o valor daconstante
	sb->blksz = blocksize; // Atribui o tamanho do bloco
	sb->blks = fsize(sb->fd)/sb->blksz; // Calcula o número de blocos

	if(sb->blks < MIN_BLOCK_COUNT) { // Checa se o número de blocos é maior que o mínimo
		errno = ENOSPC;
		free(sb);
		return NULL;
	}
	sb->freeblks = sb->blks - 3;
	sb->freelist = 3;
	sb->root= 1;
	escreverNoDisco(sb, (void *) sb, 0, sizeof(struct superblock));

	// Root
	{
		struct inode *in;
		struct nodeinfo *nodeIn;	
		in = (struct inode*) malloc(sb->blksz);
		nodeIn = (struct nodeinfo*) malloc(sb->blksz);

		strcpy(nodeIn->name, "/");
		nodeIn->size = 0;

		in->mode = IMDIR;
		in->parent = 1;
		in->meta = 2;
		in->next = 0;

		escreverNoDisco(sb, (void *) in, 1, sb->blksz);
		escreverNoDisco(sb, (void *) nodeIn, 2, sb->blksz);
		free(in);
		free(nodeIn);
	}
	{
	// freeblocks		
		int i;
		struct freepage* fpg;
		fpg = (struct freepage*) malloc(sb->blksz);

		for(i=3; i< sb->blks;i++) {
			fpg->next = (i+1==sb->blks)?0:i+1;
			fpg->count = 0;
			escreverNoDisco(sb, (void *) fpg, i, sb->blksz);
		}
	}
	/*flock(sb->fd, LOCK_UN);
	close(sb->fd);*/
	return sb;
}

struct superblock * fs_open(const char *fname){
	int auxFd; // Descritor auxiliar para gravar em sb->fd
	struct superblock* sb = openDisk(fname);
	if(sb == NULL)
		return;
	
	auxFd = sb->fd;

	read(auxFd, (void *) sb, sizeof(struct superblock)); // Carrega o superbloco
	sb->fd = auxFd;

	if(sb->magic != 0xdcc605f5) {
		errno = EBADF;
		flock(sb->fd, LOCK_UN);
		close(sb->fd);
		free(sb);
		return 0;
	}

	return sb;
}

int fs_close(struct superblock *sb){
	if(sb == NULL)
		return -1;

	if(sb->magic != 0xdcc605f5) {
		errno = EBADF;
		return -1;
	}
	
	escreverNoDisco(sb, (void *) sb, 0, sizeof(struct superblock));

	flock(sb->fd, LOCK_UN);
	close(sb->fd);
	free(sb);
	return 0;
}

uint64_t fs_get_block(struct superblock *sb){
	uint64_t blocoRetorno;
	struct freepage *fp;

	if(sb== NULL)
		return (uint64_t)-1;
	if(sb->freeblks == 0)
		return 0;

	fp = (struct freepage*) malloc(sb->blksz);
	lerDoDisco(sb, (void *) fp, sb->freelist, sb->blksz);
	if(fp->count>0) {
		blocoRetorno = fp->links[fp->count-1];
		fp->count--;
		escreverNoDisco(sb, (void *) fp, sb->freelist, sb->blksz);
	} else {
		blocoRetorno = sb->freelist;
		sb->freelist = fp->next;
	}
	sb->freeblks--;
	free(fp);
	return blocoRetorno;
}

int fs_put_block(struct superblock *sb, uint64_t block){
	struct freepage *fp;
	if(sb == NULL)
		return -1;
	
	fp = (struct freepage*) malloc(sb->blksz);
	fp->next = sb->freelist;
	fp->count = 0;
	sb->freelist = block;
	sb->freeblks++;
	escreverNoDisco(sb, (void *) fp, block, sb->blksz);
	free(fp);
	return 0;
}

int fs_write_file(struct superblock *sb, const char *fname, char *buf,size_t cnt){



}

ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf, size_t bufsz){



}

int fs_unlink(struct superblock *sb, const char *fname){


}

int fs_mkdir(struct superblock *sb, const char *dname){
	if(sb->freeblks<=1) {
		errno = ENOSPC;
		return -1;
	}
	parDiretorioNome* pda;
	struct inode* inodeDir, *inodeNewDir;
	struct nodeinfo* nodeinfoDir, *nodeinfoNewDir;
	uint64_t lastMeta, nodeBlock, infoBlock, metaInfo;

	pda = procuraDiretorio(sb, dname);
	if(pda==NULL)
		return -1;

	inodeDir = (struct inode*) malloc(sb->blksz);
	nodeinfoDir = (struct nodeinfo*) malloc(sb->blksz);

	lerDoDisco(sb, (void *) inodeDir, pda->dirInode, sb->blksz);
	lerDoDisco(sb, (void *) nodeinfoDir, inodeDir->meta, sb->blksz);
	metaInfo = inodeDir->meta;
	lastMeta = pda->dirInode;
	while(inodeDir->next!=0){
		lastMeta = inodeDir->next;
		lerDoDisco(sb, (void *) inodeDir, inodeDir->next, sb->blksz);
	}

	if(nodeinfoDir->size!= 0 && nodeinfoDir->size%INODE_LINKS_LIMIT==0) {
		printf("I WANNA HEAL\n");
		if(sb->freeblks<=2) { // Será necessário mais um bloco para o novo inode.
			errno = ENOSPC;
			free(pda->arq);
			free(pda);
			free(inodeDir);
			free(nodeinfoDir);
			return -1;
		}		
		struct inode* dirChild = (struct inode*) malloc(sb->blksz);
		uint64_t newBlock = fs_get_block(sb);
		// Cria o novo child
		dirChild->mode = IMCHILD;
		dirChild->parent = (inodeDir->mode==IMCHILD)?inodeDir->parent:lastMeta;
		dirChild->meta = lastMeta;
		dirChild->next = 0;
		inodeDir->next = newBlock;
		// Salva os dados no disco
		escreverNoDisco(sb, (void *) dirChild, newBlock, sb->blksz);
		escreverNoDisco(sb, (void *) inodeDir, lastMeta, sb->blksz);
		free(dirChild);
		lerDoDisco(sb, (void *) inodeDir, newBlock, sb->blksz);
		lastMeta = newBlock;
	}
	// Aloca a memória do novo arquivo
	inodeNewDir = (struct inode*) malloc(sb->blksz);
	nodeinfoNewDir = (struct nodeinfo*) malloc(sb->blksz);
	// Cria o inode info
	nodeinfoNewDir->size = 0;
	strcpy(nodeinfoNewDir->name, pda->arq);

	// Pedir os blocos que faltam
	nodeBlock = fs_get_block(sb);
	infoBlock = fs_get_block(sb);
	// Criar as informações do inode
	inodeNewDir->mode = IMDIR;
	inodeNewDir->parent = pda->dirInode; // duvida
	inodeNewDir->meta = infoBlock;
	inodeNewDir->next = 0;
	// Atualizar o diretório pai
	inodeDir->links[nodeinfoDir->size%INODE_LINKS_LIMIT] = nodeBlock;
	nodeinfoDir->size++;
	// Salvar os dados em disco
	escreverNoDisco(sb, (void *) inodeDir, lastMeta, sb->blksz);
	escreverNoDisco(sb, (void *) nodeinfoDir, metaInfo, sb->blksz);
	escreverNoDisco(sb, (void *) inodeNewDir, nodeBlock, sb->blksz);
	escreverNoDisco(sb, (void *) nodeinfoNewDir, infoBlock, sb->blksz);
	// Dar free
	free(pda->arq);
	free(pda);
	free(inodeDir);
	free(nodeinfoDir);
	free(inodeNewDir);
	free(nodeinfoNewDir);
	
	return 0;
}

int fs_rmdir(struct superblock *sb, const char *dname){



}


//por enqautno eu acho que só lista diretorios. o link para o inode dos arquivos também está no links do inode do diretorio??? [Se Sim, então vai listar tudo]
char * fs_list_dir(struct superblock *sb, const char *dname){
	parDiretorioNome* pda;
	struct inode* inodeDir, *fileInode;
	struct nodeinfo* nodeinfoDir, *fileInfo;
	char *fnameCopy, *strRetorno;

	fnameCopy = (char *) malloc((strlen(dname) + 1) * sizeof(char));
	strcpy(fnameCopy, dname); // Copia o caminho para uma string que pode ser modificada

	strcat(fnameCopy," "); //Ele só add um espaco no final para a funcao [procuraDiretorio] retornar bonitinho *.*

	pda = procuraDiretorio(sb, fnameCopy);
	if(pda==NULL)
		return NULL;

	inodeDir = (struct inode*) malloc(sb->blksz);
	nodeinfoDir = (struct nodeinfo*) malloc(sb->blksz);
	fileInfo = (struct nodeinfo*) malloc(sb->blksz);
	fileInode = (struct inode*) malloc(sb->blksz);

	lerDoDisco(sb, (void *) inodeDir, pda->dirInode, sb->blksz);
	lerDoDisco(sb, (void *) nodeinfoDir, inodeDir->meta, sb->blksz);

	strRetorno = (char *)malloc((nodeinfoDir->size * 255) * sizeof(char)); // Quantidade de itens * o máximo de cada nome
	strcpy(strRetorno,"");

	int j;
	for(j=0; j < nodeinfoDir->size; j++) {
		
		if(j != 0 && j%(sb->blksz-32) == 0) {
			lerDoDisco(sb, (void *) inodeDir, inodeDir->next, sb->blksz);
		}

		lerDoDisco(sb, (void *) fileInode, inodeDir->links[j%(sb->blksz-32)], sb->blksz);
		lerDoDisco(sb, (void *) fileInfo, fileInode->meta, sb->blksz);

		strcat(strRetorno, fileInfo->name);

		if(fileInode->mode==IMDIR){			
			strcat(strRetorno, "/");
		}

		strcat(strRetorno, " "); //add espaço na frente do nome pra separar
		

	}

    //COMO DAR FREE NA VARIAVEL strRetorno???????
 
	return strRetorno;
	
}