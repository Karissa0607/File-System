#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "ext2fsal.h"

pthread_mutex_t lock;
unsigned char *disk;
void ext2_fsal_init(const char* image)
{
        /**
     	* TODO: Initialization tasks, e.g., initialize synchronization primitives used,
     	* or any other structures that may need to be initialized in your implementation,
     	* open the disk image by mmap-ing it, etc.
     	*/
	//Open disk image
	disk = NULL;
	int fd = open(image, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	//Initialize synchronization primitives 
	if (pthread_mutex_init(&lock, NULL) != 0) {
		exit(1);
	}
}

void ext2_fsal_destroy()
{
    /**
     * TODO: Cleanup tasks, e.g., destroy synchronization primitives, munmap the image, etc.
     */
	//munmap the image
	int retval = munmap(disk, 128 * 1024);
	if (retval == -1) { //munmap returns 0 on success, -1 on fail
		perror("munmap");
		exit(1);
	}
	pthread_mutex_destroy(&lock);
}
