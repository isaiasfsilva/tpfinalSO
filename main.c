#include <inttypes.h>
#include <stdio.h>
#include "fs.h"
#include <errno.h>

int main() {
	struct superblock *sb = fs_format("disk.img", 128);
	if(sb!=NULL) {
		printf("%d\n", sb->fd);
		printf(":D\n");
	} else {
		printf("D':\n");
	}
	fs_close(sb);
	sb = fs_open("disk.img");
	if(sb!=NULL) {
		printf("%d\n", sb->fd);
		printf(":D\n");
	} else {
		printf("D':\n");
	}
	return 0;
}