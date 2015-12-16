struct superblock * fs_format(const char *fname, uint64_t blocksize){



}

/* Open the filesystem in =fname and return its superblock.  Returns NULL on
 * error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605fs, then errno is set to EBADF. */
struct superblock * fs_open(const char *fname){


}

/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
int fs_close(struct superblock *sb){


}

/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb){



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