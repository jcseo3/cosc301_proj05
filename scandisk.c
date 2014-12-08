/*
Names: Junghyun Seo , Cindy Han
Proj05
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"


void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent*4; i++)
	printf(" ");
}



void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}

//gets the current actual length of the cluster chain so that it can be compared to the size value in the directory entry
int getclusterlen(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb){
	int clusterlen = 1;
	uint16_t fatent = get_fat_entry(startCluster, image_buf, bpb);

	while (!is_end_of_file(fatent)){
		clusterlen++;
		fatent = get_fat_entry(startCluster, image_buf, bpb);
	}
	return clusterlen;
}

//Fix for when the FAT cluster chain is LONGER than the given correct size in the dir entry 
void fix_lFAT(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, uint32_t clusterlen){
	//can we always assume there will be at least one cluster? for now, I assumed that we also assume there is always one
	int clusternum = 1;
	uint16_t fatent = get_fat_entry(startCluster, image_buf, bpb);
	
	while (clusternum < clusterlen){
		fatent = get_fat_entry(startCluster, image_buf, bpb);
		clusternum++;
	}
		
	//set the "real" last cluster to EOF
	set_fat_entry (fatent, EOF, image_buf, bpb);
	
	//freeing any clusters that exist past the real last cluster
	while (!is_end_of_file(fatent)){
		uint16_t freetemp = fatent;
		fatent = get_fat_entry(startCluster, image_buf, bpb);
		set_fat_entry (freetemp, EOF, image_buf, bpb); // I'm don't think setting it to EOF is freeing it, but Idk how to free it.. 		
	}	
}

//Fix for when the FAT chain cluster is SHORTER than the given correct size in dir entry
void fix_sFAT(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, struct direntry *dirent){
	//get the size that is recorded in the directory entry
	uint32_t size = getulong(dirent->deFileSize);
	int correctclusternum = size / 512
	if ((size % 512) != 0){
		correctclusternum+=1;
	} 
	//traverse the clusters and undo the EOF on the current EOF and keep traversing until end of numofclusters and make that last FAT EOF 
	//problem: do I overwrite other clusters or do I jump around? how does this work...? how do I lengthen the chain cluster without overwriting others?
}


int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

<<<<<<< HEAD
    image_buf = mmap_file(argv[1], &fd); //image_buf is a pointer to the memory-mapped disk image;can pass pointer along other funcs to read and manipulate FS
    bpb = check_bootsector(image_buf); // checks to makes sure bootsector is valid
    
    uint8_t rootdiraddr = *root_dir_addr(image_buf, bpb);
=======
    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);

    // your code should start here...
	


>>>>>>> a7a076d79322c8974e2bdb114b43f15e2be9d723

	//your code should start here...


    unmmap_file(image_buf, &fd);
    return 0;
}
