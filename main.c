#include <inttypes.h>
#include <stdio.h>
#include "fs.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void tMkdir(char dir[]) {
	struct superblock *sb = fs_open("disk.img");
	fs_mkdir(sb, "/tomate/oi/peido14");
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
	const char *strz = "/filead/";
	
	teste = procuraDiretorio(sb, strz, 0);
	if(teste == NULL)
		perror("ERRO");
	printf("%s\n%lu\n", teste->arq, teste->dirInode);
	fs_close(sb);
}

tAltosMkdir() {
	int i;
	char filename[550];
	printf("Formating... ");
	//struct superblock *sb = fs_open("disk.img");
	struct superblock *sb = fs_format("disk.img", 128);
	printf("Done\n");
	for(i=0;i<=100;i++) {
		printf("Creating file%d... ", i);
		sprintf(filename, "file%d", i);
		fs_mkdir(sb, filename);
		printf("Done\n");
	}
	fs_close(sb);
}

void tListDir() {
	struct superblock *sb = fs_open("disk.img");
	char *as = fs_list_dir(sb, "/");
	printf("%s\n\n", as);	  // 
	free(as);
	fs_close(sb);
}

void tWritefile() {
	struct superblock *sb = fs_format("disk.img", 128);
	char aaa[300];
	strcpy(aaa, "Oi, eu me chamo Roberto, eh bom conversar com voce! Oi, eu me chamo Roberto, eh bom conversar com voce! Oi, eu me chamo Roberto, eh bom conversar com voce! Oi, eu me chamo Roberto, eh bom conversar com voce! Oi, eu me chamo Roberto, eh bom conversar com voce!");
	fs_write_file(sb, "/amaterasu.txt", aaa, 260);

	fs_close(sb);
}

void tReadfile() {
	struct superblock *sb = fs_open("disk.img");
	char bbb[300];
	int i;
	for(i=0;i<300;i++) {
		bbb[i] = 0;
	}
	fs_read_file(sb, "/amaterasu.txt", bbb, 300);

	printf("%s\n", bbb);
	fs_close(sb);
}


void tRmdir() {
	
	struct superblock *sb = fs_open("disk.img");
	char filename[550];
	//struct superblock *sb = fs_open("disk.img");
 	int i;

	for(i=0;i<=100;i++) {
		printf("Removendo file%d\n",i);
		char *as = fs_list_dir(sb, "/");
	printf("%s\n\n", as);	  // 
	free(as);

		sprintf(filename, "file%d", i);
		fs_rmdir(sb, filename);

			char *ass = fs_list_dir(sb, "/");
	printf("%s\n\n", ass);	  // 
	free(ass);

		printf("Done\n\n\n");
	}



	
	fs_close(sb);
}

int main() {

tFormata();
tAltosMkdir();
tRmdir();
	return 0;
}
