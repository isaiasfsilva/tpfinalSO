#include <inttypes.h>
#include <stdio.h>
#include "fs.h"
#include <errno.h>
#include <string.h>

void tMkdir(char dir[]) {
	printf("criando %s\n", dir);
	struct superblock *sb = fs_open("disk.img");
	fs_mkdir(sb, dir);
	fs_close(sb);
}



void tFormata() {
	struct superblock *sb = fs_format("disk.img", 128);
	fs_close(sb);
}

void tGetBlock() {
	struct superblock *sb = fs_open("disk.img");
	printf("%ld\n", sb->blks);
	printf("%ld\n", fs_get_block(sb));
	fs_close(sb);
}

void tPutBlock() {
	struct superblock *sb = fs_open("disk.img");
	fs_put_block(sb, 4);	 
	fs_close(sb);
}

void tProcuraDiretorio() {
	struct superblock *sb = fs_open("disk.img");
	parDiretorioNome* teste;
	const char *strz = "/tomate/oi/peido/josias";
	
	teste = procuraDiretorio(sb, strz);
	if(teste == NULL)
		perror("ERRO");
	printf("%s\n%lu\n", teste->arq, teste->dirInode);
	fs_close(sb);
}

void tListDir() {
	struct superblock *sb = fs_open("disk.img");
	printf("%s\n\n", fs_list_dir(sb, "/home/"));	  // 

	fs_close(sb);
}


int main() {
tFormata();
tMkdir("/home");

int i;
char dir[50]="/home/usr";
for(i=0;i<50;i++){
	char dirtemp[50]="";
	char buffer[60];
	strcpy(dirtemp,dir);
	sprintf(buffer, "%d", i);
	strcat(dirtemp,buffer);
	tMkdir(dirtemp);
	tListDir();

}

}