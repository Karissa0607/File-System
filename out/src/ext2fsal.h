#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include "e2fs.h"
extern pthread_mutex_t lock;
extern unsigned char *disk;
// Initializes the ext2 file system
// Called only once during the server initialization
//
// image is a pointer to a zero terminated string that is a full path to valid ext2 disk image
void ext2_fsal_init(const char *image);

// Destroys the ext2 file system
void ext2_fsal_destroy();

// src is a pointer to a zero terminated string
// dst is a pointer to a zero terminated string
//
// returns 0 if the operation completed succefully. 
// Otherwise, an error may be returned (see handout).
int32_t ext2_fsal_cp(const char *src,
                     const char *dst);

// src is a pointer to a zero terminated string
// dst is a pointer to a zero terminated string
//
// returns 0 if the operation completed succefully. 
// Otherwise, an error may be returned (see handout).
int32_t ext2_fsal_ln_hl(const char *src,
                        const char *dst);

// src is a pointer to a zero terminated string
// dst is a pointer to a zero terminated string
//
// returns 0 if the operation completed succefully. 
// Otherwise, an error may be returned (see handout).
int32_t ext2_fsal_ln_sl(const char *src,
                        const char *dst);

// path is a pointer to a zero terminated string
//
// returns 0 if the operation completed succefully. 
// Otherwise, an error may be returned (see handout).
int32_t ext2_fsal_rm(const char *path);

// path is a pointer to a zero terminated string
//
// returns 0 if the operation completed succefully. 
// Otherwise, an error may be returned (see handout).
int32_t ext2_fsal_mkdir(const char *path);
