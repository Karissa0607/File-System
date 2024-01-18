#include "e2fs.h"
#include "ext2.h"
#include "ext2fsal.h"
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

 /**
  * TODO: Add any helper implementations here
  */
/*
 * Returns the super block located at block 1
 */
struct ext2_super_block *get_super_block(unsigned char *disk) {
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	return sb;
}

/*
 * Returns the blocks group descriptor located at block 2
 */
struct ext2_group_desc *get_group_desc(unsigned char *disk) {
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE*2);
	return bg;
}

/*
 * Returns the block bitmap located at block 3
 */
unsigned char *get_block_bitmap(unsigned char *disk) {
	struct ext2_group_desc *bg = get_group_desc(disk);
	unsigned char *block_bitmap =  (unsigned char *)(disk + EXT2_BLOCK_SIZE*bg->bg_block_bitmap);
	return block_bitmap;
}

/*
 * Returns the inode bitmap located at block 4
 */
unsigned char *get_inode_bitmap(unsigned char *disk) {
	struct ext2_group_desc *bg = get_group_desc(disk);
	unsigned char *inode_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE*bg->bg_inode_bitmap);
	return inode_bitmap;
}

/*
 * Returns the inode table located at block 5
 */
struct ext2_inode *get_inode_table(unsigned char *disk) {
	struct ext2_group_desc *bg = get_group_desc(disk);
        struct ext2_inode *inode_table = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*bg->bg_inode_table);
        return inode_table;
}

/*
 * Returns the directory entry that matches the name that is inside the directory that is at the inode provided
 */
struct ext2_dir_entry *get_directory_entry(struct ext2_inode *inode, char *name) {
	//If name is null then there is no directory entry
	int name_len;
	if (name == NULL) {
		return NULL;
	} else {
		//if the length of the name is greater than the constraints that EXT2 file system places on the name length then exit with error code 1
		name_len = strlen(name);
		if (name_len > EXT2_NAME_LEN) {
			pthread_mutex_unlock(&lock); 
			exit(1);
		}
	}

	//Iterate through the given inode blocks to find the directory entry that has the same name that was provided
	for (int i = 0; i < inode->i_blocks/2; i++) {
		int length = 0;
		while (length < EXT2_BLOCK_SIZE) {
			struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * inode->i_block[i] + length);
			if (name_len == ((int) entry->name_len) && strncmp(name, entry->name, name_len) == 0) {
				return entry;
			}
			length += entry->rec_len;
		}
	}
	return NULL; //No entry was found so return null
}

/*
 * This will change the first bit that is available to 1 and returns the number of the inode or the block
 */
int find_free_bit(unsigned char *bitmap, int size) {
	//Loop through the bits of the bytes in the bitmap to find an available bit
	for (int byte = 0; byte < size/8; byte++) {//8 bits in 1 byte
        	for (int bit = 0; bit < 8; bit++) {
            		int in_use = (bitmap[byte] & (1 << bit)) >> bit;
            		if (!in_use) {
				//Change bit to 1 to indicate its now being used
				bitmap[byte] |= 1 << bit;
				return byte*8 + bit + 1;
			}
       		}
   	}
	pthread_mutex_unlock(&lock);
	exit(ENOMEM);
	
}

/*
 * Returns the inode type that corresponds with the directory entry type
 */
unsigned short get_inode_type(unsigned char type) {
	if (type == EXT2_FT_DIR) {
		return EXT2_S_IFDIR;
	} else if (type == EXT2_FT_REG_FILE) {
		return EXT2_S_IFREG;
	} else {
		return EXT2_S_IFLNK;
	}
}

/*
 * Assigns an inode with the type
 * Returns the inode number that was the first available
 */
int assign_inode(unsigned char type) {	
	unsigned char *inode_bitmap = get_inode_bitmap(disk);
	struct ext2_super_block *sb = get_super_block(disk);
        struct ext2_group_desc *bg = get_group_desc(disk);
	struct ext2_inode *inode_table = get_inode_table(disk);

	//Find the first free inode
	int inode = find_free_bit(inode_bitmap, sb->s_inodes_count);
	struct ext2_inode *inode_section = &inode_table[inode - 1];
	memset(inode_section, 0, sizeof(struct ext2_inode));

	//Updates the block group descriptor and super block information
	bg->bg_free_inodes_count--;
        sb->s_free_inodes_count--;

	//Initializes the inode
	unsigned int times = (unsigned int) time(NULL);
        if (((time_t) times) == ((time_t) -1)) exit(1);	
	inode_section->i_mode |= get_inode_type(type);
	inode_section->i_ctime = times;
	inode_section->i_atime = times;
	inode_section->i_mtime = times;

	return inode;
}

/*
 * Assigns a block
 * Returns the block number that was the first available
 */
int assign_block() {
	unsigned char *block_bitmap = get_block_bitmap(disk);
	struct ext2_super_block *sb = get_super_block(disk);
        struct ext2_group_desc *bg = get_group_desc(disk);

	//Find the first free block
	int block = find_free_bit(block_bitmap, sb->s_blocks_count);

	unsigned char *block_section = disk + EXT2_BLOCK_SIZE * block;
	memset(block_section, 0, EXT2_BLOCK_SIZE);

	//Updates the block group descriptor and super block information
	bg->bg_free_blocks_count--;
	sb->s_free_blocks_count--;

	return block;
}

struct ext2_dir_entry *make_directory_entry(struct ext2_inode *inode, unsigned int link, char *name, unsigned char type) {
	//Ensure that the directory entry with this name does not already exist
	if (get_directory_entry(inode, name) != NULL) {
		pthread_mutex_unlock(&lock);
		exit(EEXIST);
	}

	//Ensure that the name length is not greater than the constraints that EXT2 fs has on the name len of directory entries
	int name_len = strlen(name);
	if (name_len > EXT2_NAME_LEN) {
		pthread_mutex_unlock(&lock);
		exit(1);
	}

	//The rec_len of a directory entry must be a multiple of 4
	int new_rec_len = sizeof(struct ext2_dir_entry) + name_len;
	if (new_rec_len % 4 != 0) {
		//Add padding
		int padding = (4 - (new_rec_len % 4));
		new_rec_len += padding;
	}

	//Assign an inode for the directory entry if one was not already provided
	if (link == 0) {
		link = assign_inode(type);
	}
	struct ext2_inode *inode_table = get_inode_table(disk);
	int i;
	for (i = 0; i < 12; i++) {
		if ((inode->i_block)[i] == 0) {//Block not in use
			int block_num = assign_block(); //Assign a block for the directory entry
			//Fix inode info
			(inode->i_block)[i] = block_num;
			inode->i_size += EXT2_BLOCK_SIZE;
			inode->i_blocks += EXT2_BLOCK_SIZE / 512;
			
			//Get the directory entry
			struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + block_num * EXT2_BLOCK_SIZE);

			//Increment the link count of the link inode
			(&inode_table[link - 1])->i_links_count++;

			//Initialize directory entry information
			entry->rec_len = EXT2_BLOCK_SIZE;
			entry->name_len = name_len;
			entry->file_type = type;
			entry->inode = link;
			strcpy(entry->name, name);
			
			return entry;
		} else {
			//Block is in use
			//Find the last directory entry and try to put the directory entry here
			unsigned char *start = disk + ((inode->i_block)[i] * EXT2_BLOCK_SIZE);
            		unsigned char *end = start + EXT2_BLOCK_SIZE;
            
            		unsigned char *pointer = start;
            		struct ext2_dir_entry *curr_entry = (struct ext2_dir_entry *)start;
            		pointer += curr_entry->rec_len;

			//Loop to find the last directory entry in the block which is the prev_entry
            		struct ext2_dir_entry *prev_entry = curr_entry;
            		while(pointer != end){
                		prev_entry = (struct ext2_dir_entry *)pointer;
                		pointer += prev_entry->rec_len;
            		}
		        //Get the actual rec len of the directory entry	
			int actual_rec_len = sizeof(struct ext2_dir_entry) + prev_entry->name_len;
                        if (actual_rec_len % 4 != 0) {//rec_len must be a multiple of 4
                                //Add padding
                        	int padding = (4 - (actual_rec_len % 4));
                                actual_rec_len += padding;
                        }

			if((prev_entry->rec_len - actual_rec_len) >= new_rec_len){

                		//The new directory entry goes after the last found directory entry in the block
                		int rec_len = prev_entry->rec_len - actual_rec_len;

                		//Fix the rec_len of the last directory entry
                		prev_entry->rec_len = actual_rec_len;
                		struct ext2_dir_entry *new_entry  = (struct ext2_dir_entry *)(end - rec_len);
			
				//Increment the link count of the link inode
                        	(&inode_table[link - 1])->i_links_count++;

				//Initialize directory entry information
                        	new_entry->rec_len = rec_len;
                        	new_entry->name_len = name_len;
                        	new_entry->file_type = type;
                        	new_entry->inode = link;
                        	strcpy(new_entry->name, name);
                		return new_entry;
            		}
		}
	}
	//All 12 direct blocks are full
	return NULL; 
}

/*
 * Updates the inode to not used so 0
 */
void delete_inode(unsigned int num) {
	unsigned char *inode_bitmap = get_inode_bitmap(disk);
	struct ext2_group_desc *bg = get_group_desc(disk);
       	struct ext2_super_block *sb = get_super_block(disk);
	//Change the bit in the inode_bitmap to 0
	int byte = (num - 1) / 8;
	int bit = (num - 1) % 8;
	inode_bitmap[byte] = inode_bitmap[byte] & (~(1<< bit));
	bg->bg_free_inodes_count++;
	sb->s_free_inodes_count++;	
}

/*
 * Updates the block to not used so 0
 */
void delete_block(unsigned int num) {
	unsigned char *block_bitmap = get_block_bitmap(disk);
	struct ext2_group_desc *bg = get_group_desc(disk);
        struct ext2_super_block *sb = get_super_block(disk);
	//change the bit in the block_bitmap to 0
	int byte = (num - 1) / 8;
	int bit = (num - 1) % 8;
	block_bitmap[byte] = block_bitmap[byte] & (~(1<<bit));
	bg->bg_free_blocks_count++;
        sb->s_free_blocks_count++;
}

/*
 * Deletes the blocks in the inode
 */
void delete_blocks(struct ext2_inode *inode) {
	//Delete the direct blocks
	for (int i = 0; i < inode->i_blocks/2; i++) {
		delete_block(inode->i_block[i]);
	}
	//Delete the indirect block. There is only one since the disk is only 128 blocks
	unsigned int num = inode->i_block[12]; 
	if (num != 0) {
		//Obtain the indirect block
		unsigned int *block = (unsigned int *)(disk + num * EXT2_BLOCK_SIZE);
		int max = EXT2_BLOCK_SIZE / sizeof(unsigned int); 
		//Loop through all direct blocks in the indirect block
		for (int i = 0; i < max; i++) {
			//This is the direct block num
			unsigned int num1 = block[i];
			if (num1 != 0) {
				delete_block(num1); //delete direct block
			}
		} 
		delete_block(num); //delete the indirect block
	}


}
void remove_inode(unsigned int num) {
	struct ext2_inode *inode_table = get_inode_table(disk);
        struct ext2_inode *inode = &inode_table[num - 1];
	if (inode->i_links_count == 0) {
		exit(1); //Inode with no links can't be removed
	}
	//Decrease link count
	inode->i_links_count--;
	if (inode->i_links_count == 0) {
		unsigned int times = (unsigned int) time(NULL);
		if (((time_t) times) == ((time_t) -1)) {
			exit(1);
		}
		inode->i_dtime = times;
		delete_inode(num);
		delete_blocks(inode);
	}
}

/*
 * Used by the cp command to copy data from src_file to dst_file.
 */
void copy_data(FILE *src_file, struct ext2_inode *inode) {
	//initilaize important variables
	unsigned char buff[EXT2_BLOCK_SIZE];
	int block_ptr = 0;
	unsigned int bytes_read;

	//read data from src_file and copy it into direct blocks
	while (block_ptr < 12) {
		bytes_read = fread(buff, 1, EXT2_BLOCK_SIZE, src_file); 
		if (bytes_read <= 0) { //done if all data is read
			return;
		}
		//assign a direct block and copy data into it
		int block = assign_block();
		unsigned int *curr_block = (unsigned int *) (disk + (block * EXT2_BLOCK_SIZE));
		memcpy(curr_block, buff, bytes_read);

		//fix inode info
		inode->i_size += bytes_read;
		inode->i_blocks += EXT2_BLOCK_SIZE/512;
		inode->i_block[block_ptr] = block;

		block_ptr++; //move to next block
	}
	//done if all data is read
	bytes_read = fread(buff, 1, EXT2_BLOCK_SIZE, src_file);
	if (bytes_read <= 0) {
		return;
	}

	//for remaing data assign an indirect block
	int indirect_block = assign_block();
	unsigned int *indirect_block_pos = (unsigned int *)(disk + indirect_block * EXT2_BLOCK_SIZE); //position of indirect_block
	inode->i_blocks += EXT2_BLOCK_SIZE / 512;
	inode->i_block[12] = indirect_block;

	int blocks_used = 0; //keep track of blocks used
	int max = EXT2_BLOCK_SIZE / sizeof(unsigned int); //blocks_used cannot exceed max
	
	//loop until blocks_used reaches max
	while (blocks_used <= max) {
		if (bytes_read <= 0) {
			return;
		}
		//assign a direct block and copy data into it
		int block = assign_block();
		unsigned char *curr_block = disk + (block * EXT2_BLOCK_SIZE);
		memcpy(curr_block, buff, bytes_read);
		
		//fix inode info
		inode->i_size += bytes_read;
		inode->i_blocks += EXT2_BLOCK_SIZE / 512;
		*indirect_block_pos = block;
		
		//update other info
		indirect_block++; //move to next block
		blocks_used++;
		bytes_read = fread(buff, 1, EXT2_BLOCK_SIZE, src_file);
	}
	return;
}
