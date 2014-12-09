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

void write_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size) {
	// from dos_cp.c file
	// writes the values into a directory entry
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
	//problem: do I overwrite other clusters or do I jump around? how does this work...? how do I lengthen the chain cluster without overwriting others? any thoughts?
}

uint16_t print_dirent(struct direntry *dirent, int indent) {
	// modified from dos_ls.c file
	uint16_t followclust = 0;
	
	int i;
	char name[9];
	char extension[4];
	uint32_t size;
	uint16_t file_cluster;
	name[8] = ' ';
	extension[3] ' ';
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


void traverse_root(uint8)t *image_buf, struct bpb33* bpb){
	// from dos_ls.c file
	uint16_t cluster = 0;
	
	struct direntry *dirent = (struct direntry *)cluster_to_addr(cluster, image_buf, bpb);
	
	int i = 0;
	for ( ; i < bpb->bpbRootDirEnts; i++) {
		uint16_t followclust = print_dirent(dirent, 0);
		if (is_valid_cluster(followclust, bpb)) {
			follow_dir(followclust, 1, image_buf, bpb, references);
		}
		dirent++;
	}
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
	// scan the FAT system and gather data about references
	traverse_root(image_buf, bpb);

    unmmap_file(image_buf, &fd);
    return 0;
}
