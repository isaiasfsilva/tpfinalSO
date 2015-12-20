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
parDiretorioNome* procuraDiretorio(struct superblock *sb, const char* fname, char removeFlag) {
	parDiretorioNome *pda;
	char *fnameCopy, *strAux, *strAux2;
	uint64_t dirCount, i;
	uint64_t dirCorrente, fileInodeAux;

	
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
				if(!removeFlag) { 
					if(!strcmp(strAux, fileInfo->name)) {
						errno = EEXIST;
						break;
					}
				} else {
					if(!strcmp(strAux, fileInfo->name)) {
						achou = 1;
						errno = 0;
						fileInodeAux = dirInode->links[j%INODE_LINKS_LIMIT];
						break;
					}
				}
			}
		}

		free(dirInode);
		free(fileInode);
		free(dirInfo);
		free(fileInfo);
		if((i!=dirCount && !achou) || (i==dirCount && removeFlag && !achou))
			errno = ENOENT;
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
	pda->fileInode = (removeFlag)?fileInodeAux:0;
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
		uint64_t i;
		struct freepage* fpg;
		fpg = (struct freepage*) malloc(sb->blksz);

		for(i=3; i< sb->blks;i++) {
			fpg->next = (i+1==sb->blks)?0:i+1;
			fpg->count = 0;
			escreverNoDisco(sb, (void *) fpg, i, sb->blksz);
		}
		free(fpg);
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
	size_t blocosNecessarios, blocosDados;
	blocosDados = (cnt/sb->blksz) + ((cnt%sb->blksz!=0)?1:0); // Conteudo do arquivo
	blocosNecessarios = 2; // Inode principal + metadados
	blocosNecessarios += ((blocosDados/INODE_LINKS_LIMIT) + ((blocosDados%INODE_LINKS_LIMIT)?1:0)) -1; // inodes child
	blocosNecessarios += blocosDados; // Conteudo do arquivo
	
	if(sb->freeblks<blocosNecessarios) {
		errno = ENOSPC;
		return -1;
	}

	parDiretorioNome* pda;
	struct inode* inodeDir, *inodeNewFile;
	struct nodeinfo *nodeinfoDir, *nodeinfoNewFile;
	uint64_t lastMeta, nodeBlock, infoBlock, metaInfo;

	pda = procuraDiretorio(sb, fname, 0);
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
		if(sb->freeblks<blocosNecessarios+1) { // Será necessário mais um bloco para o novo inode.
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
	inodeNewFile = (struct inode*) malloc(sb->blksz);
	nodeinfoNewFile = (struct nodeinfo*) malloc(sb->blksz);
	// Cria o inode info
	nodeinfoNewFile->size = cnt;
	strcpy(nodeinfoNewFile->name, pda->arq);

	// Pedir os blocos que faltam
	nodeBlock = fs_get_block(sb);
	infoBlock = fs_get_block(sb);
	// Criar as informações do inode
	inodeNewFile->mode = IMREG;
	inodeNewFile->parent = pda->dirInode; // duvida
	inodeNewFile->meta = infoBlock;
	inodeNewFile->next = 0;
	// Atualizar o diretório pai
	inodeDir->links[nodeinfoDir->size%INODE_LINKS_LIMIT] = nodeBlock;
	nodeinfoDir->size++;
	// Salvar os dados em disco
	escreverNoDisco(sb, (void *) inodeDir, lastMeta, sb->blksz);
	escreverNoDisco(sb, (void *) nodeinfoDir, metaInfo, sb->blksz);
	escreverNoDisco(sb, (void *) inodeNewFile, nodeBlock, sb->blksz);
	escreverNoDisco(sb, (void *) nodeinfoNewFile, infoBlock, sb->blksz);
	// Dar free
	free(pda->arq);
	free(pda);
	free(inodeDir);
	free(nodeinfoDir);
	free(inodeNewFile);
	free(nodeinfoNewFile);
	 // Escrever o conteúdo do arquivo
		uint64_t i;
		uint64_t block, fileBlock, lastBlock;
		struct inode* inodeAtual;
		inodeAtual = (struct inode*) malloc(sb->blksz);
		lerDoDisco(sb, (void *) inodeAtual, nodeBlock, sb->blksz);
		block = nodeBlock;
		for(i=0; i < blocosDados; i++) {
			if(i!=0 && i%INODE_LINKS_LIMIT==0) {
				struct inode *nodeChild;
				nodeChild = (struct inode *) malloc(sb->blksz);
				nodeChild->mode = IMCHILD;
				nodeChild->parent = nodeBlock;
				nodeChild->meta = inodeAtual->meta==IMCHILD?block:nodeBlock;
				nodeChild->next = 0;
				lastBlock = block;
				block =fs_get_block(sb);
				inodeAtual->next = block;

				escreverNoDisco(sb, (void *) inodeAtual, lastBlock, sb->blksz);
				escreverNoDisco(sb, (void *) nodeChild, block, sb->blksz);
				free(nodeChild);
				lerDoDisco(sb, (void *) inodeAtual, block, sb->blksz);
			}

			fileBlock = fs_get_block(sb);
			inodeAtual->links[i%INODE_LINKS_LIMIT] = fileBlock;
			if(i!=blocosDados-1 || cnt%sb->blksz==0)
				escreverNoDisco(sb, (void *) (buf +(i*sb->blksz)), fileBlock, sb->blksz);
			else
				escreverNoDisco(sb, (void *) (buf +(i*sb->blksz)), fileBlock, cnt%sb->blksz);
		}

		escreverNoDisco(sb, (void *) inodeAtual, block, sb->blksz);
		
		free(inodeAtual);
	

	return 0;
}

ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf, size_t bufsz){
	size_t bytesleft,bytecnt, i, blockFiles;
	parDiretorioNome *pda;
	struct inode *fileInode, *dirInode;
	struct nodeinfo *fileInodeInfo, *dirInodeInfo;
	pda = procuraDiretorio(sb,fname,1);

	if(pda == NULL)
		return -1;

	fileInode = (struct inode*) malloc(sb->blksz);
	fileInodeInfo = (struct nodeinfo*) malloc(sb->blksz);

	lerDoDisco(sb, (void *) fileInode, pda->fileInode, sb->blksz);
	lerDoDisco(sb, (void *) fileInodeInfo, fileInode->meta, sb->blksz);
	// TODO - Modificar retorno  da pda para parametro = 1
	if (fileInode->mode != IMREG){ //se o inode não corresponder um arquivo
		errno = EISDIR;
		free(fileInode);
		free(fileInodeInfo);
		free(pda->arq);
		free(pda);
		return -1;
		//retornar errno
	}

	if(bufsz > fileInodeInfo->size) //se o buffer for maior que o arquivo, ler todos os blocos do arquivo
		bufsz = fileInodeInfo->size;
	
	blockFiles = (bufsz/sb->blksz) + ((bufsz%sb->blksz==0)?0:1);
	for(i=0; i<blockFiles; i++) {
		if(i!=0 && i%INODE_LINKS_LIMIT==0){
			lerDoDisco(sb, (void *) fileInode, fileInode->next, sb->blksz);
		}
		if(i+1!=blockFiles || bufsz%sb->blksz==0)
			lerDoDisco(sb, (void *) (buf +(i*sb->blksz)), fileInode->links[i%INODE_LINKS_LIMIT], sb->blksz);
		else
			lerDoDisco(sb, (void *) (buf +(i*sb->blksz)), fileInode->links[i%INODE_LINKS_LIMIT], bufsz%sb->blksz);
	}
	free(pda->arq);
	free(pda);
	free(fileInode);
	free(fileInodeInfo);
	return (ssize_t) bufsz;
}

int fs_unlink(struct superblock *sb, const char *fname){
	parDiretorioNome *pda;
	pda = procuraDiretorio(sb,fname,1); //terceiro parametro, caso n exista da erro

	if(pda == NULL)
		return -1;

	// Deletando o inode do arquivo e seus dados
	struct inode *fileInode;
	struct nodeinfo *fileMeta; // Os inodes dos arquivos
	fileInode = (struct inode *) malloc(sb->blksz);
	fileMeta = (struct nodeinfo *) malloc(sb->blksz);
	lerDoDisco(sb, (void *) fileInode, pda->fileInode, sb->blksz);
	lerDoDisco(sb, (void *) fileMeta, fileInode->meta, sb->blksz);
	
	uint64_t totalBlocks = fileMeta->size/sb->blksz;
	uint64_t lastInodeBlocks = (totalBlocks%INODE_LINKS_LIMIT!=0)?totalBlocks%INODE_LINKS_LIMIT:INODE_LINKS_LIMIT;

	fs_put_block(sb, fileInode->meta);
	uint64_t nextFileBlock = pda->fileInode;
	while(fileInode->next!=0) {
		uint64_t actualFileBlock = nextFileBlock;
		uint64_t i;
		if(fileInode->next!=0)
			for(i=0;i<INODE_LINKS_LIMIT;i++)
				fs_put_block(sb, fileInode->links[i]);
		else
			for(i=0;i<lastInodeBlocks;i++)
				fs_put_block(sb, fileInode->links[i]);
		
		if(fileInode->next!=0) 
			nextFileBlock = fileInode->next;
		lerDoDisco(sb, (void *) fileInode, fileInode->next, sb->blksz);
		fs_put_block(sb, actualFileBlock);
	}
	free(fileInode);
	free(fileMeta);

	// Atualizar o diretório pai
	struct inode *dirInode;
	struct nodeinfo *dirMeta; // Os inodes dos arquivos
	dirInode = (struct inode *) malloc(sb->blksz);
	dirMeta = (struct nodeinfo *) malloc(sb->blksz);
	lerDoDisco(sb, (void *) dirInode, pda->dirInode, sb->blksz);
	lerDoDisco(sb, (void *) dirMeta, dirInode->meta, sb->blksz);
	// Achar onde está o arquivo
	uint64_t filePosition;
	uint64_t i, actualFileBlock = pda->dirInode;
	for(i=0; i< dirMeta->size; i++) {
		if(i!=0 && i%INODE_LINKS_LIMIT==0) {
			actualFileBlock = dirInode->next;
			lerDoDisco(sb, (void *) dirInode, dirInode->next, sb->blksz);
		}
		if(dirInode->links[i%INODE_LINKS_LIMIT] == pda->fileInode) {
			filePosition = i%INODE_LINKS_LIMIT;
		}
	}

	dirMeta->size--;
	struct inode *dirInodeAux = (struct inode*) malloc(sb->blksz);
	lerDoDisco(sb, (void *) dirInodeAux, actualFileBlock, sb->blksz);
	
	uint64_t blockAux = pda->dirInode;
	while(dirInodeAux->next!=0){
		blockAux = dirInodeAux->next;
		lerDoDisco(sb, (void *) dirInodeAux, dirInodeAux->next, sb->blksz);
	}
	
	dirInode->links[filePosition] = dirInodeAux->links[dirMeta->size%INODE_LINKS_LIMIT];

	escreverNoDisco(sb, (void *) dirInode, actualFileBlock, sb->blksz);
	escreverNoDisco(sb, (void *) dirMeta, dirInode->meta, sb->blksz);
	if(dirMeta->size%INODE_LINKS_LIMIT==0 && dirInodeAux->mode == IMCHILD) {
		blockAux = dirInodeAux->meta;
		lerDoDisco(sb, (void *) dirInodeAux, dirInodeAux->meta, sb->blksz);	
		fs_put_block(sb, dirInodeAux->next);
		dirInodeAux->next = 0;
	}
	escreverNoDisco(sb, (void *) dirInodeAux, blockAux, sb->blksz);
	free(dirInode);
	free(dirMeta);
	free(dirInodeAux);
	return 0;
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

	pda = procuraDiretorio(sb, dname, 0);
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
	inodeNewDir->parent = pda->dirInode;
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

	parDiretorioNome* pda;
	struct inode* inodeDir, *fileInode, *auxInodeDir;
	struct nodeinfo* nodeinfoDir, *fileInfo;
	char *fnameCopy, *strRetorno;

	uint64_t lastMetaInodeDir, lastMetaInfoInodeDir,lastMetaInodeDir2,llastMetaInodeDir;

	pda = procuraDiretorio(sb, dname,1);
	
	if(pda==NULL)
		return -1;

 
	inodeDir = (struct inode*) malloc(sb->blksz);
	nodeinfoDir = (struct nodeinfo*) malloc(sb->blksz);	
	fileInode = (struct inode*) malloc(sb->blksz);
	fileInfo = (struct nodeinfo*) malloc(sb->blksz);
	auxInodeDir=(struct inode*) malloc(sb->blksz);

	lastMetaInodeDir2=pda->dirInode;
	lastMetaInodeDir=pda->dirInode;
	llastMetaInodeDir=pda->dirInode;

	lerDoDisco(sb, (void *) inodeDir, pda->dirInode, sb->blksz);
	lerDoDisco(sb, (void *) nodeinfoDir, inodeDir->meta, sb->blksz);

	lastMetaInfoInodeDir= inodeDir->meta;
    
    
    int j=-1;
    do{ // Encontra qual diretório que vou remover
    	j++;
		if( j!=0 && j%INODE_LINKS_LIMIT==0)  {
			llastMetaInodeDir=lastMetaInodeDir2;
			lastMetaInodeDir=lastMetaInodeDir2;
			lastMetaInodeDir2=inodeDir->next;
			lerDoDisco(sb, (void *) inodeDir, inodeDir->next, sb->blksz);
		}

		lerDoDisco(sb, (void *) fileInode, inodeDir->links[j%INODE_LINKS_LIMIT], sb->blksz);
		lerDoDisco(sb, (void *) fileInfo, fileInode->meta, sb->blksz);
		//printf("Endereço do links[j]%d\n", inodeDir->links[j%INODE_LINKS_LIMIT]);
    }while(j < nodeinfoDir->size && strcmp(fileInfo->name,pda->arq)!=0);


	if(strcmp(fileInfo->name,pda->arq)==0){			
		if(fileInfo->size>0){ //Se diretório não estaá vazio
			errno = ENOTEMPTY;
			return -1;
		}
		fs_put_block(sb,inodeDir->links[j%INODE_LINKS_LIMIT]); //Recoloca Inode e Info de volta como livres
		fs_put_block(sb,fileInode->meta);

		memcpy(auxInodeDir, inodeDir, sb->blksz);


		while(inodeDir->next!=0){ //Acha o ultimo carinha!
			printf("é o ultimo carinha\n");
			llastMetaInodeDir=lastMetaInodeDir;
			lastMetaInodeDir = inodeDir->next;
			lerDoDisco(sb, (void *) inodeDir, inodeDir->next, sb->blksz);
			
		} 

		

		nodeinfoDir->size--; 



		auxInodeDir->links[j%INODE_LINKS_LIMIT] = inodeDir->links[(nodeinfoDir->size)%INODE_LINKS_LIMIT]; // Link[i] = links[i+1]; Observe que já deu --
	    

	    if((nodeinfoDir->size)%INODE_LINKS_LIMIT==0){
	    	printf("iiiixi... size==1\n");
	    	
	    	

	    	lerDoDisco(sb, (void *) inodeDir,llastMetaInodeDir, sb->blksz);
	    	fs_put_block(sb,inodeDir->next);//Recoloca na freelist
	    	inodeDir->next=0;
	    	escreverNoDisco(sb, (void *) inodeDir, llastMetaInodeDir, sb->blksz);
	    }
			



	escreverNoDisco(sb, (void *) auxInodeDir, lastMetaInodeDir2, sb->blksz);
	escreverNoDisco(sb, (void *) nodeinfoDir, lastMetaInfoInodeDir, sb->blksz);


		
		
	}else{ //DIRETORIO INEXISTENTE!!!!!
		errno=ENOENT;
		return -1;
	}

	free(inodeDir);
	free(nodeinfoDir);
	free(fileInode);
	free(fileInfo);
	free(auxInodeDir);
	
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

	pda = procuraDiretorio(sb, fnameCopy, 0);
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
		
		if(j!=0 && j%INODE_LINKS_LIMIT==0)  {
			lerDoDisco(sb, (void *) inodeDir, inodeDir->next, sb->blksz);
		}

		lerDoDisco(sb, (void *) fileInode, inodeDir->links[j%INODE_LINKS_LIMIT], sb->blksz);
		lerDoDisco(sb, (void *) fileInfo, fileInode->meta, sb->blksz);

		strcat(strRetorno, fileInfo->name);

		if(fileInode->mode==IMDIR){			
			strcat(strRetorno, "/");
		}

		strcat(strRetorno, " "); //add espaço na frente do nome pra separar
		

	}

 	free(pda->arq);
	free(pda);
	free(inodeDir);
	free(nodeinfoDir);
	free(fileInfo);
	free(fileInode);
	free(fnameCopy);
	return strRetorno;
	
}