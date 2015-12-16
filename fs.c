#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

/* Build a new filesystem image in =fname (the file =fname should be present
 * in the OS's filesystem).  The new filesystem should use =blocksize as its
 * block size; the number of blocks in the filesystem will be automatically
 * computed from the file size.  The filesystem will be initialized with an
 * empty root directory.  This function returns NULL on error and sets errno
 * to the appropriate error code.  If the block size is smaller than
 * MIN_BLOCK_SIZE bytes, then the format fails and the function sets errno to
 * EINVAL.  If there is insufficient space to store MIN_BLOCK_COUNT blocks in
 * =fname, then the function fails and sets errno to ENOSPC. */

unsigned long fsize(int fd) {
    unsigned long len = (unsigned long) lseek(fd, 0, SEEK_END);
    return len;
}

void escreverNoDisco(struct superblock* sb, void * data, int64_t pos, size_t tam) {
	lseek(sb->fd, (pos*sb->blksz), SEEK_SET);
	write(sb->fd, data, tam);
}

void lerDoDisco(struct superblock* sb, void * data, int64_t pos, size_t tam) {
	lseek(sb->fd, (pos*sb->blksz), SEEK_SET);
	read(sb->fd, data, tam);
}

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

/*struct inode* createInode(char child, uint64_t parent, uint64_t size, char *name) {
	if(mode == "IMCHILD") {

	}
	struct inode *in;
	in = (struct inode*) malloc(sizeof(struct inode));


	return in;
}*/



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
	sb->freeblks = sb->blks - 4;
	sb->freelist = 3;
	sb->root= 1;
	escreverNoDisco(sb, (void *) sb, 0, sizeof(struct superblock));

	// Root
	{
		struct inode *in;
		struct nodeinfo *nodeIn;	
		in = (struct inode*) malloc(sizeof(struct inode));
		nodeIn = (struct nodeinfo*) malloc(sizeof(struct nodeinfo));

		strcpy(nodeIn->name, "/");
		nodeIn->size = 0;

		in->mode = IMDIR;
		in->parent = 1;
		in->meta = 2;
		in->next = 0;

		escreverNoDisco(sb, (void *) in, 1, sizeof(struct inode));
		escreverNoDisco(sb, (void *) nodeIn, 2, sizeof(struct nodeinfo));
		free(in);
		free(nodeIn);
	}
	{
	// freeblocks		
		int i;
		struct freepage* fpg;
		fpg = (struct freepage*) malloc(sizeof(struct freepage));

		for(i=3; i< sb->blks;i++) {
			fpg->next = (i+1==sb->blks)?0:i+1;
			fpg->count = 0;
			escreverNoDisco(sb, (void *) fpg, i, sizeof(struct freepage));
		}
	}
	/*flock(sb->fd, LOCK_UN);
	close(sb->fd);*/
	return sb;
}

/* Open the filesystem in =fname and return its superblock.  Returns NULL on
 * error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605fs, then errno is set to EBADF. */
struct superblock * fs_open(const char *fname){
	int auxFd;
	struct superblock* sb = openDisk(fname);
	if(sb == NULL)
		return;
	
	auxFd = sb->fd;

	read(auxFd, (void *) sb, sizeof(struct superblock));
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

/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
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

/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb){
	int64_t blocoRetorno;
	struct freepage *fp;

	if(sb== NULL)
		return (uint64_t)-1;
	if(sb->freeblks == 0)
		return 0;

	fp = (struct freepage*) malloc(sizeof(struct freepage));
	lerDoDisco(sb, (void *) fp, sb->freelist, sizeof(struct freepage));
	if(fp->count>0) {
		blocoRetorno = fp->links[count-1];
		fp->count--;
		sb->freeblks--;
		escreverNoDisco(sb, (void *) fp, sb->freelist, sizeof(struct freepage));
	} else {
		
	}
	free(fp);
	return blocoRetorno;
}

/* Put =block back into the filesystem as a free block.  Returns zero on
 * success or a negative value on error.  If there is an error, errno is set
 * accordingly. */
int fs_put_block(struct superblock *sb, uint64_t block){


}

int fs_write_file(struct superblock *sb, const char *fname, char *buf,size_t cnt){



}

ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf, size_t bufsz){



}

int fs_unlink(struct superblock *sb, const char *fname){


}

int fs_mkdir(struct superblock *sb, const char *dname){


}

int fs_rmdir(struct superblock *sb, const char *dname){



}

char * fs_list_dir(struct superblock *sb, const char *dname){


	
}