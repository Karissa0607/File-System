#include <pthread.h>
#include "ext2fsal.h"
#include "e2fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int32_t ext2_fsal_mkdir(const char *path)
{
    	/**
     	* TODO: implement the ext2_mkdir command here ...
     	* the argument path is the path to the directory that is to be created.
     	*/
	//Beginning of path analysis:
	//Root directory can not be made
	if (strcmp(path, "/") == 0) {
		return EEXIST; //Root directory already exists
	}
	//Path must start with a /. If not it is not an absolute path
	if (path[0] != '/') {
		return ENOENT;
	}
	if (path == NULL) {
		return ENOENT;
	}
	
	//Get the needed basic ext2 structures
	struct ext2_group_desc *bg = get_group_desc(disk);
    	struct ext2_inode *inode_table = get_inode_table(disk);
	
	//Make a copy of the path
	int path_len = strlen(path);
	char path_1[path_len + 1];
	strncpy(path_1, path, path_len + 1);

	//Splitting up path into its directories
	char *rest = path_1;
	char *name = strtok_r(rest, "/", &rest);

	//Current inode is the root inode
	int inode_num = EXT2_ROOT_INO;
	struct ext2_inode *inode = &inode_table[EXT2_ROOT_INO - 1];
	
	//Find the directory entry in the root inode that matches with the name of the directory in the path
	pthread_mutex_lock(&lock);
	struct ext2_dir_entry *directory_entry = get_directory_entry(inode, name);
	pthread_mutex_unlock(&lock);

	//Loop to find the last directory in the path
	while (name != NULL) {
		char *next_name = strtok_r(rest, "/", &rest);
		if (directory_entry == NULL && next_name != NULL) {
			return ENOENT; //Path does not exist
		} else if (directory_entry == NULL && next_name == NULL) {
			//No directory entry was found and there is no other file/directory in the path after this one
			//Create directory entry: 
			pthread_mutex_lock(&lock);
			struct ext2_dir_entry *curr_directory_entry = make_directory_entry(inode, 0, name, EXT2_FT_DIR);
			bg->bg_used_dirs_count++;
			struct ext2_inode *curr_inode = &inode_table[curr_directory_entry->inode - 1];
			make_directory_entry(curr_inode, curr_directory_entry->inode, ".", EXT2_FT_DIR); //Link directory to itself
			make_directory_entry(curr_inode, inode_num, "..", EXT2_FT_DIR); //Link directory to its parent
			pthread_mutex_unlock(&lock);
			return 0; //Success
		} else if (directory_entry != NULL && next_name == NULL) {
			return EEXIST; //Directory already exists
		} else if (directory_entry != NULL && directory_entry->file_type != EXT2_FT_DIR) {
			return ENOENT; //Absolute path contains a type that is not a directory
		}
		//Update information
		pthread_mutex_lock(&lock);
		inode_num = directory_entry->inode;
		name = next_name;
		inode = &inode_table[directory_entry->inode - 1];
		directory_entry = get_directory_entry(inode, name);
		pthread_mutex_unlock(&lock);
	}
	return ENOENT; //Path doesn't exist
}
