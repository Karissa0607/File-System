#include "ext2fsal.h"
#include "e2fs.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <errno.h>

/*
 * Returns the directory entry of the file within the path
 */ 
struct ext2_dir_entry *get_file_directory(char *path) {
	//Make a copy of the path
	int path_len = strlen(path) + 1;
	char org_name[path_len];
    	strncpy(org_name, path, path_len);

	//Find the last name in the path
	char *name = strtok(org_name, "/");

	//Get the parent inode which is the root inode
	struct ext2_inode *inode_table = get_inode_table(disk);
	struct ext2_inode *parent_inode = &inode_table[EXT2_ROOT_INO - 1];

	//Get the parent directory of the parent inode
	struct ext2_dir_entry *parent_dir_entry = get_directory_entry(parent_inode, ".");

	if (parent_dir_entry == NULL) {
		pthread_mutex_unlock(&lock);
		exit(ENOENT);
	}

	struct ext2_inode *inode = &inode_table[parent_dir_entry->inode - 1];
	//The current directory entry in the path
	struct ext2_dir_entry *dir_entry = get_directory_entry(inode, name);

	//Loop through the names in the path to find the parent directory entry
	while (name != NULL) {
		char *next_name = strtok(NULL, "/");
		if (dir_entry == NULL) {
			//No directory entry has this name
			pthread_mutex_unlock(&lock);
			exit(ENOENT);
		} else if (next_name == NULL) {
			//The end of the path has been reached
			return parent_dir_entry;
		} else if (dir_entry->file_type != EXT2_FT_DIR) {
			//The directory entry is not a directory
			pthread_mutex_unlock(&lock);
			exit(ENOENT);
		}
		name = next_name;
		parent_dir_entry = dir_entry;
		inode = &inode_table[parent_dir_entry->inode - 1];
        	dir_entry = get_directory_entry(inode, name);
	}
	return parent_dir_entry;
}

/*
 * Removes the directory entry of the file that has the name which is in the inode_num directory entry
 */
int remove_file(unsigned int inode_num, char *name) {
	struct ext2_inode *inode_table = get_inode_table(disk);
	struct ext2_inode *inode = &inode_table[inode_num - 1];
	int name_len = strlen(name);
	if (name_len > EXT2_NAME_LEN) {
		exit(1);
	}
	pthread_mutex_lock(&lock);
	for (int i = 0; i < inode->i_blocks/2; i++) {
		int length = 0;
		struct ext2_dir_entry *prev_dir_entry = NULL;
		while (length < EXT2_BLOCK_SIZE) {
			struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * inode->i_block[i] + length);
			if (entry->inode != 0) {//Directory entry is in use
				if ((name_len == (int) entry->name_len) && (strncmp(name, entry->name, name_len) == 0)) { //File names match
					if (entry->file_type == EXT2_FT_DIR) {//This is a directory not a file
						pthread_mutex_unlock(&lock);
						return EISDIR;
					}
					//Delete the file entry
					//Remove inode 
					remove_inode(entry->inode);
					if (prev_dir_entry == NULL) {
						//First entry in block. Set inode to 0 as it is no longer being used
						entry->inode = 0;
					} else {
						//The rec len of the prev entry is added to the rec_len of the removed entry
						prev_dir_entry->rec_len += entry->rec_len;
					}
					pthread_mutex_unlock(&lock);
					return 0;
				}
				length += entry->rec_len;
			} else if (entry->rec_len > 0) {
				length += entry->rec_len; //Skip to next entry as the directory entry is not in use
			} else {
				//Skip to end of block as the directory entry is zeroed out
				length = EXT2_BLOCK_SIZE;
			}
			prev_dir_entry = entry;
		}
	}
	//No matching directory was found
	pthread_mutex_unlock(&lock);
	return ENOENT;
}
int32_t ext2_fsal_rm(const char *path)
{
    /**
     * TODO: implement the ext2_rm command here ...
     * the argument 'path' is the path to the file to be removed.
     */
	int path_len = strlen(path);
     	if (path[path_len - 1] == '/') {
                return EISDIR; //Path must end with a file
        }
	//Path must start with a /. If not it is not an absolute path
        if (path[0] != '/') {
                return ENOENT;
        }
	//The path does not exist
        if (path == NULL) {
                return ENOENT;
        }
	
	//Make a copy of the path
	char path_1[path_len + 1];
	strncpy(path_1, path, path_len + 1);
	
	//Get the directory entry of the file that is indicated by the path
	pthread_mutex_lock(&lock);
	struct ext2_dir_entry *file_directory = get_file_directory(path_1);
	pthread_mutex_unlock(&lock);

	//If it does not exist the path is invalid
	if (file_directory == NULL) {
                return ENOENT;
        }

	//Find the name in the path of the file that is getting removed
	char *rest = path_1;
	char *name = strtok_r(rest, "/", &rest);

	char *final_name;
	if (name == NULL) { //No filename specified, so it refers to current directory
		final_name = ".";
	} else {
		final_name = name;
		while (name != NULL) {
			name = strtok_r(rest, "/", &rest);
			if (name != NULL) {
				final_name = name;
			}
		}
	}

    	return remove_file(file_directory->inode, final_name);
}

