#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

void print_inode(struct ext2_inode *inode_table, int inode_num) {
    char type;
    if ((inode_table[inode_num - 1].i_mode & EXT2_S_IFDIR) != 0) {//7th inode is inode_table[6]
	type = 'd';
    } else if ((inode_table[inode_num-1].i_mode & EXT2_S_IFREG) != 0) {
	type = 'f';
    } else if ((inode_table[inode_num-1].i_mode & EXT2_S_IFLNK) != 0) {
	type = '1';
    } else {
	type = '0';
    }
    struct ext2_inode *inode = &inode_table[inode_num - 1]; 
    printf("[%d] type: %c size: %d links: %d blocks: %d\n", inode_num, type, inode->i_size, inode->i_links_count, inode->i_blocks);

    printf("[%d] Blocks: ", inode_num);
    for (int i = 0; i < inode->i_blocks/2; i++) {
	printf(" %d", inode->i_block[i]);
    }
    printf("\n");
}
int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", bg->bg_block_bitmap);
    printf("    inode bitmap: %d\n", bg->bg_inode_bitmap);
    printf("    inode table: %d\n", bg->bg_inode_table);
    printf("    free blocks: %d\n", bg->bg_free_blocks_count);
    printf("    free inodes: %d\n", bg->bg_free_inodes_count);
    printf("    used_dirs: %d\n", bg->bg_used_dirs_count);
    
    unsigned char *block_bitmap = (unsigned char *)(disk + 1024*bg->bg_block_bitmap);
    printf("Block bitmap: ");
    for (int byte = 0; byte < sb->s_blocks_count/8; byte++) {//8 bits in 1 byte
	for (int bit = 0; bit < 8; bit++) {
	    int in_use = (block_bitmap[byte] & (1 << bit)) >> bit;
	    printf("%d", in_use);
	}
	printf(" ");
    }
    printf("\n");

    unsigned char *inode_bitmap = (unsigned char *)(disk + 1024*bg->bg_inode_bitmap);
    printf("Inode bitmap: ");
    for (int byte = 0; byte < sb->s_inodes_count/8; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            int in_use = (inode_bitmap[byte] & (1 << bit)) >> bit;
            printf("%d", in_use);
        }
        printf(" ");
    }
    printf("\n");

    struct ext2_inode *inode_table = (struct ext2_inode *)(disk + 1024*bg->bg_inode_table);  
    printf("Inodes:\n");
    print_inode(inode_table, EXT2_ROOT_INO); //root/second inode
    //Print all used inodes
    for (int i = sb->s_first_ino; i < sb->s_inodes_count; i++) {
	int byte = i / 8;
	int bit = i % 8; 
	if (inode_bitmap[byte] & (1 << bit)) {
		print_inode(inode_table, i + 1); 
	}
    }
     
    return 0;
}
