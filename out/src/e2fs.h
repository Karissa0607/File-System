#ifndef CSC369_E2FS_H
#define CSC369_E2FS_H

#include "ext2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ext2fsal.h"
/**
 * TODO: add in here prototypes for any helpers you might need.
 * Implement the helpers in e2fs.c
 */

struct ext2_super_block *get_super_block(unsigned char *disk);
struct ext2_group_desc *get_group_desc(unsigned char *disk);
unsigned char *get_block_bitmap(unsigned char *disk);
unsigned char *get_inode_bitmap(unsigned char *disk);
struct ext2_inode *get_inode_table(unsigned char *disk);
struct ext2_dir_entry *get_directory_entry(struct ext2_inode *inode, char *name);
int find_free_bit(unsigned char *bitmap, int size);
unsigned short get_inode_type(unsigned char type);
int allocate_inode(unsigned char type);
int allocate_block();
struct ext2_dir_entry *make_directory_entry(struct ext2_inode *inode, unsigned int link, char *name, unsigned char type);
void delete_inode(unsigned int num);
void delete_block(unsigned int num);
void delete_blocks(struct ext2_inode *inode);
void remove_inode(unsigned int num);
void copy_data(FILE *src_file, struct ext2_inode *inode);
#endif
