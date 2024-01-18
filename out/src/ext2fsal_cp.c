#include "ext2fsal.h"
#include "e2fs.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int32_t ext2_fsal_cp(const char *src,
                     const char *dst)
{
    /**
     * TODO: implement the ext2_cp command here ...
     * Arguments src and dst are the cp command arguments described in the handout.
     */
	
	//open src file for reading
	FILE *src_file = fopen(src, "rb");
	//ERROR CHECKING: check if file exists
    	if (src_file == NULL) {
        	return ENOENT;
    	}
	//make a copy of the src path
	int src_path_len = strlen(src);
	char src_1[src_path_len + 1];
        strncpy(src_1, src, src_path_len + 1);
	
	//get src filename by splitting up src path
	char *rest = src_1;
	char *name = strtok_r(rest, "/", &rest);
	char *src_filename;
	src_filename = name;
        while (name != NULL) {
		name = strtok_r(rest, "/", &rest);
                if (name != NULL) {
			src_filename = name;
		}
	}

	//create dst file
	//ERROR CHECKING: dst path must start with a /, otherwise it is not an absolute path
	if (dst[0] != '/') {
                return ENOENT;
        }
	//get inode of current directory
	struct ext2_inode *inode_table = get_inode_table(disk);
	struct ext2_inode *inode = &inode_table[EXT2_ROOT_INO - 1];
	
	//make a copy of the dst path
	int dst_path_len = strlen(dst) + 1;
	char dst_1[dst_path_len + 1];
        strncpy(dst_1, dst, dst_path_len + 1);
	char *dst_path_left = dst_1 + 1; //records the dst path left
	
	//splitting up dst path into its directories
	char dst_path[dst_path_len]; 
	strncpy(dst_path, dst_1, dst_path_len);
	char *rest2 = dst_path;
	char *dst_path_item = strtok_r(rest2, "/", &rest2); //current item in dst_path
	if (dst_path_item != NULL) {
		dst_path_left += strlen(dst_path_item); //update dst path left
	}

	//get current directory in dst_path_item
	pthread_mutex_lock(&lock);
	struct ext2_dir_entry *directory_entry = get_directory_entry(inode, dst_path_item);
	pthread_mutex_unlock(&lock);
	//loop until all directories in dst_path are traversed
	while (directory_entry != NULL) {
		//check if the current directory entry is a directory
		if (directory_entry->file_type != EXT2_FT_DIR) {
			if (strlen(dst_path_left) != 0) {
				return ENOENT; //ERROR CHECKING: current dst_path_item is not a directory
			}
			return EEXIST; //ERROR CHECKING: soft link or file with the same name already exists
		}
		//update dst_path_left to delete the /
                if (dst_path_left[0] == '/' && directory_entry->file_type == EXT2_FT_DIR) {
                        dst_path_left++;
                }
		//get next item in the dst_path
		dst_path_item = strtok_r(rest2, "/", &rest2);
		if (dst_path_item != NULL) {
                	dst_path_left += strlen(dst_path_item);
        	}
		inode = &inode_table[directory_entry->inode - 1];
		pthread_mutex_lock(&lock);
		directory_entry = get_directory_entry(inode, dst_path_item);
		pthread_mutex_unlock(&lock);
	}
	//if dst_path includes file name then use it, otherwise use src_filename
	if (dst_path_item != NULL) {
		src_filename = dst_path_item;
	}

	pthread_mutex_lock(&lock);
	struct ext2_dir_entry *dst_file = make_directory_entry(inode, 0, src_filename, EXT2_FT_REG_FILE); //create dst file
        struct ext2_inode *next_inode = &inode_table[dst_file->inode - 1];
	copy_data(src_file, next_inode); //copy data from src file to dst file
	pthread_mutex_unlock(&lock);

	fclose(src_file); //close src_file
    	return 0;
}
