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
#include <ctype.h>


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


// from dos_cp.c file
// writes the values into a directory entry
void write_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size) {

	char *p, *p2;
	char *uppername;
	int len, i;
	
	// clean out anything old that used to be here
	memset(dirent, 0, sizeof(struct direntry));
	
	// extract just the filename part
	uppername = strdup(filename);
	p2 = uppername;
	for (i = 0; i < strlen(filename); i++) {
		if (p2[i] == '/' || p2[i] == '\\') {
			uppername = p2+i+1;
		}
	}
	// convert filename to uppercase
	for (i = 0; i < strlen(uppername); i++) {
		uppername[i] = toupper(uppername[i]);
	}
	
	// set the file name and extension
	memset(dirent->deName, ' ', 8);
	p = strchr(uppername, '.');
	memcpy(dirent->deExtension, "___", 3);
	if (p == NULL) {
		fprintf(stderr, "No file name extension given - defaulting to . ___\n");
	}
	else {
		*p = '\0';
		p++;
		len = strlen(p);
		if (len > 3) len = 3;
		memcpy(dirent->deExtension, p, len);
	}
	
	if (strlen(uppername) > 8) {
		uppername[8] = '\0';
	}
	memcpy(dirent->deName, uppername, strlen(uppername));
	free(p2);
	
	// set the attributes and file size
	dirent->deAttributes = ATTR_NORMAL;
	putushort(dirent->deStartCluster, start_cluster);
	putulong(dirent->deFileSize, size);
}

// from dos_cp.file
// finds a free slot in the directory, and writes the directory entry
void create_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size, uint8_t *image_buf, struct bpb33 *bpb) {

	while (1) {
		if (dirent->deName[0] == SLOT_EMPTY) {
			// we found an empty slot at the end of the directory
			write_dirent(dirent,filename, start_cluster, size);
			dirent++;
			// make sure the next dirent is set to be empty, just in case it wasn't before
			memset((uint8_t *)dirent, 0, sizeof(struct direntry));
			dirent->deName[0] = SLOT_EMPTY;
			return;
		}
		if (dirent->deName[0] == SLOT_DELETED) {
			// we found a deleted entry - we can just overwrite it
			write_dirent(dirent, filename, start_cluster, size);
			return;
		}
		dirent++;
	}
}


// from dos_ls.c file
uint16_t print_dirent(struct direntry *dirent, int indent) {
	uint16_t followclust = 0;
	
	int i;
	char name[9];
	char extension[4];
	uint32_t size;
	uint16_t file_cluster;
	name[8] = ' ';
	extension[3] = ' ';
	memcpy(name, &(dirent->deName[0]), 8);
	memcpy(extension, dirent->deExtension, 3);
	
	if (name[0] == SLOT_EMPTY) {
		return followclust;
	}
	
	// skip over deleted entries
	if (((uint8_t)name[0]) == SLOT_DELETED) {
		return followclust;
	}
	
	if (((uint8_t)name[0]) == 0x2E) {
		// dot entry ("." or "..")
		// skip it
		return followclust;
	}
	
	// names are space padded - remove the spaces
	for (i = 8; i > 0; i --) {
		if (name[i] == ' ')
			name[i] = '\0';
		else
			break;
	}
	
	// remove the spaces from extensions
	for (i = 3; i > 0; i--) {
		if (extension[i] == ' ')
			extension[i] = '\0';
		else
			break;
	}
	
	if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN) {
		// ignore any long file name extension entries
		//
		// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0) {
		printf("Volume: %s\n", name);
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
		if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN) {
	   		print_indent(indent);
    	    printf("%s/ (directory)\n", name);
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else {
    	/*
    	 * a "regular" file entry
    	 * print attributes, size, starting cluster, etc.
    	 */
    	 int ro = (dirent->deAttributes & ATTR_READONLY) == ATTR_READONLY;
    	 int hidden = (dirent->deAttributes & ATTR_HIDDEN) == ATTR_HIDDEN;
    	 int sys = (dirent->deAttributes & ATTR_SYSTEM) == ATTR_SYSTEM;
    	 int arch = (dirent->deAttributes & ATTR_ARCHIVE) == ATTR_ARCHIVE;
    	 
    	 size = getulong(dirent->deFileSize);
    	 print_indent(indent);
    	 printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", name, extension, size, getushort(dirent->deStartCluster), ro?'r':' ', hidden?'h':' ', sys?'s':' ', arch?'a': ' ');
    }
    
    return followclust;
}



// checks if the current entry is a file
int is_file(struct direntry *dirent, int indent) {
	int is_file = 0;
	char name[9];
	name[8] = ' ';
	memcpy(name, &(dirent->deName[0]), 8);
	
	if (name[0] == SLOT_EMPTY) {
		return is_file;
	}
	// skip over deleted entries
	if (((uint8_t)name[0]) == SLOT_DELETED) {
		return is_file;
	}
	if (((uint8_t)name[0]) == 0x2E) {
		// dot entry ("." or "..")
		// skip it
		return is_file;
	}
	if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN) {
		// ignore any long file name extension entries
		//
		// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0) {
		
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) {
       
    }
    else {
    	/*
    	 * a "regular" file entry
    	 * print attributes, size, starting cluster, etc.
    	 */
    	 is_file = 1;
    }
    return is_file;
}

//gets the current actual length of the cluster chain so that it can be compared to the size value in the directory entry
int getclusterlen(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, int *refarr){
	int clusterlen = 1;
	uint16_t fatent = get_fat_entry(startCluster, image_buf, bpb);
	while (!is_end_of_file(fatent)){
		refarr[fatent]++;
		fatent = get_fat_entry(fatent, image_buf, bpb);
		clusterlen++;
	}
	return clusterlen;
}


// fix for when the FAT cluster chain is 'L'ONGER than the given correct size in the dir entry 
void fix_lcluster(uint16_t startCluster, uint8_t *image_buf, struct bpb33 *bpb, uint32_t clusterlen, int *refarr){
	int clusternum = 1;
	
	uint16_t prevfat = startCluster;
	uint16_t fatent = get_fat_entry(startCluster, image_buf, bpb);
	
	// travel according to the links in the FAT
	while (clusternum < clusterlen){
		prevfat = fatent;
		fatent = get_fat_entry(fatent, image_buf, bpb);
		clusternum++;
	}
		
	//freeing any clusters that exist past the real last cluster
	fatent =  get_fat_entry(fatent, image_buf, bpb);
	while (!is_end_of_file(fatent)){
		uint16_t freetemp = fatent;
		refarr[fatent]--;
		fatent = get_fat_entry(fatent, image_buf, bpb);
		set_fat_entry(freetemp, CLUST_FREE, image_buf, bpb);
	}
	
	// free the old EOF
	set_fat_entry(fatent, CLUST_FREE, image_buf, bpb);
	
	//set the "real" last cluster to EOF
	set_fat_entry (prevfat, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
	
	//update the refcounter array of corresponding indexes to be ones if they were references clusters
	 
}

//Fix for when the FAT chain cluster is 'S'HORTER than the given correct size in dir entry
void fix_scluster(struct direntry *dirent, int clusterlen){
	//write the actual "correct" size into the dir entry
	putulong(dirent->deFileSize,(uint32_t) (clusterlen * 512));
}


// check if a file's size has changed and is inconsistent with the FAT, calls the right function to fix the problem
void check_cluster_size(struct direntry *dirent, uint8_t *image_buf, struct bpb33 *bpb, int *refarr) {
	uint32_t size = getulong(dirent->deFileSize); 
	uint16_t startCluster = getushort(dirent->deStartCluster);
	
	refarr[startCluster]++;
	
	// check if actual = expected size
	int clusterlen = getclusterlen(startCluster, image_buf, bpb, refarr);
	uint32_t expectedClusterLen = (size % 512) ? (size / 512 + 1) : (size / 512);
	
	if (expectedClusterLen == 0) {
		expectedClusterLen = 1;
	}
	
	
	if (clusterlen != expectedClusterLen) {
		printf("FAT INCONSISTENT: expected a chain length of %u clusters, actual chain length is %d clusters.\n", expectedClusterLen, clusterlen);
		if (clusterlen > expectedClusterLen) {
			// fixes the FAT chain if the expected chain is longer than the actual chain
			fix_lcluster(startCluster, image_buf, bpb, expectedClusterLen, refarr);
		}
		else {
			// fixes the dirent if the actual chain is less than the expected chain
			fix_scluster(dirent, clusterlen);
	
		}
		printf("Fixed the problem!\n");
	}
}

// from dos_ls.c file 
//traverses through a directory entry 
void follow_dir(uint16_t cluster, int indent, uint8_t *image_buf, struct bpb33* bpb, int *refarr){
    
    while (is_valid_cluster(cluster, bpb)) {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
		for ( ; i < numDirEntries; i++) {
			uint16_t followclust = print_dirent(dirent, indent);	
			
			
			// check the size and fix problems for each file; don't fix anything if it isn't a file 		
			if (is_file(dirent, 0) == 1) {
				check_cluster_size(dirent, image_buf, bpb, refarr);
			}
			
        	if (followclust){
        		follow_dir(followclust, indent+1, image_buf, bpb, refarr);
        	}
        	dirent++;
		}
		cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}


// modified from dos_ls.c file
//traversing through all the directory entries (starting at the root directory entry)
void traverse_root(uint8_t *image_buf, struct bpb33* bpb, int *refarr){
	uint16_t cluster = 0;
	
	struct direntry *dirent = (struct direntry *)cluster_to_addr(cluster, image_buf, bpb);
	
	int i = 0;
	for ( ; i < bpb->bpbRootDirEnts; i++) {
		
		
		uint16_t followclust = print_dirent(dirent, 0);
		
		
		if (is_file(dirent, 0) == 1) {
		// check the size of the file
			check_cluster_size(dirent, image_buf, bpb, refarr);
		}
		
		if (is_valid_cluster(followclust, bpb)) {
			follow_dir(followclust, 1, image_buf, bpb, refarr);
		}
		dirent++;
	}
}



void unorphaner(int *refarr, int arrsize, uint8_t *image_buf, struct bpb33 *bpb) {
	int n = 1; 
	struct direntry *dirent = (struct direntry *)cluster_to_addr(0, image_buf, bpb);
	// checks for orphan cluster and fixes them
	for (int i = 2; i < arrsize; i++) { // start from cluster 2
		printf("%d ", refarr[i]);
	 	if (refarr[i] == 0) {
	 		// check if entries marked free are actually free
	 		uint8_t fatent = get_fat_entry(i, image_buf, bpb);
	 		if (is_valid_cluster(fatent, bpb) || is_end_of_file(fatent)) {
	 			
	 			// make the file name
	 			char buf[4];
	 			snprintf(buf, 4, "found%d.dat", i);
	 			char *filename = strcat(strcat("found", buf), ".dat");
	 			n++;
	 			
	 			// make a new directory entry if the cluster is an orphan
	 			create_dirent(dirent, filename, i, getclusterlen(i, image_buf, bpb, refarr), image_buf, bpb);
	 		}
	 	}
	 }

}


int main(int argc, char** argv) {
    uint8_t *image_buf;
    struct bpb33* bpb;
    int fd;
   
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd); 
    bpb = check_bootsector(image_buf); 
    
    
    int arrsize = (bpb->bpbSectors) - 31;
    int *refarr = malloc(sizeof(int) *arrsize) ; //an array that keeps track of the reference counts
	 memset(refarr, 0, arrsize); //set all the values in the reference counter array to zero
	 // scan the FAT system and gather data about references
	 traverse_root(image_buf, bpb, refarr);


	unorphaner(refarr, arrsize, image_buf, bpb);

    unmmap_file(image_buf, &fd);
    return 0;
}
