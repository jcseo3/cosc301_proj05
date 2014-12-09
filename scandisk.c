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

//get the size of the file that is recorded in the dir entry and convert into the number of clusters there should be in the chain
//used in main function
uint32_t getmetanumcluster(struct direntry *dirent){
	uint32_t size = getulong(dirent->deFileSize);
	int numcluster = size / 512
	if ((size % 512) !=0){
		numcluster+=1;
	}
	return numcluster;
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

//Fix for when the FAT cluster chain is 'L'ONGER than the given correct size in the dir entry 
void fix_lcluster(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, uint32_t clusterlen){
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
		set_fat_entry (freetemp, 0, image_buf, bpb); // not sure if I'm freeing correctly (CLUST_FREE value is 0)
	}	
}

//Fix for when the FAT chain cluster is 'S'HORTER than the given correct size in dir entry
void fix_scluster(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, struct direntry *dirent){
	//get the real length of the cluster chain from the FAT 
	int clusterlen = getclusterlen (startCluster, image_buf, bpb);
	//write the actual "correct" size into the dir entry
	putulong(dirent->deFileSize, (clusterlen * 512)); //check syntax
}

int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd); //image_buf is a pointer to the memory-mapped disk image;can pass pointer along other funcs to read and manipulate FS
    bpb = check_bootsector(image_buf); // checks to makes sure bootsector is valid
    
    // your code should start here...
	 uint8_t rootaddr = root_dir_addr(image_buf, bpb); 
	 
	 //recursively traverse the dir entry
	 
	 //compare the metadata and the actual length/size of the chain cluster and use functions to fix the inconsistencies accordingly
	 
	 //remember to update the reference counter array



    unmmap_file(image_buf, &fd);
    return 0;
}
