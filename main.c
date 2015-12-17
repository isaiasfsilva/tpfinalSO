#include <inttypes.h>
#include <stdio.h>
#include "fs.h"
#include <errno.h>
#include <string.h>

int main() {
	struct superblock *sb = fs_open("disk.img");
	parDiretorioNome* teste;
	const char *strz = "/oi";
	
	teste = procuraDiretorio(sb, strz);
	if(teste == NULL)
		perror("ERRO");
	printf("%s\n%lu\n", teste->arq, teste->dirInode);
	fs_close(sb);
	/*struct superblock *sb = fs_format("disk.img", 128);

	fs_close(sb);
	return 0;*/	
/*	struct superblock *sb = fs_open("disk.img");
	printf("%ld\n", sb->blks);
	printf("%ld\n", fs_get_block(sb));
	fs_close(sb);
	return 0;*/

	/*struct superblock *sb = fs_open("disk.img");
	fs_put_block(sb, 4);	 
	fs_close(sb);*/

	/*struct superblock *sb = fs_open("disk.img");
	
	procuraDiretorio(sb, "dir1/dir2/dir3/arq.txt");

	fs_close(sb);*/
}