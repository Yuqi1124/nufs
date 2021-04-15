#include <stdio.h>
#include <string.h>
#include "pages.h"
#include "inode.h"
#include "directory.h"


//return -1 if find nothing, otherwise, inum of the file
int 
directory_lookup(inode* dd, const char* name) {
	//printf("get path: %s \n", name);
    	 //int ii = 0;
	 /*
   	 char store[48];
   	 while(name[ii] != '\0') {
		printf("infinite loop?\n");
		if(name[ii] != '/') {
			store[ii] = name[ii];
			ii++;
		}
   	 }
	store[ii+1] = '\0';
	*/
	if(dd->size > 4096) {
		printf("too big file\n");
	}	
	else {
		int dir_num = dd->size / 64; //how many dir
		
		//don't check mode because we don't have nest now
		int block = dd->ptrs[0]; //first block

		dir* start = (dir*) pages_get_page(block + 1); //start address

		char rname[48];
		strcpy(rname, name);
		rname[strlen(name)] = '\0';

		for(int ii = 0; ii < dir_num; ii++) {
			if(strcmp(start->name, rname) == 0) {
				return start->inum;
			}
			else {
				//supdate start
				start = (dir*) ((void*) start + 64);
			}
		}
		return -1;	
	}
}

// return -1 if didn't find the name
int directory_rename(inode* dd, const char* name, const char* after) {
	if(dd->size > 4096) {
		printf("too big file\n");
	}	
	else {
		int dir_num = dd->size / 64; //how many dir
		
		//don't check mode because we don't have nest now
		int block = dd->ptrs[0]; //first block

		dir* start = (dir*) pages_get_page(block + 1); //start address

		char rname[48];
		strcpy(rname, name);
		rname[strlen(name)] = '\0';

		for(int ii = 0; ii < dir_num; ii++) {
			if(strcmp(start->name, rname) == 0) {
				strcpy(start->name, after);
				start->name[strlen(after)] = '\0';
				return 1;
			}
			else {
				//supdate start
				start = (dir*) ((void*) start + 64);
			}
		}
		return -1;	
	}

}


int
directory_delete(inode* dd, const char* name) {
	if(dd->size > 4096) {
		printf("too big file, we are not dealling with this now\n");
		return -1;
	}	
	else {
		int dir_num = dd->size / 64; //how many dir
		
		//don't check mode because we don't have nest now
		int block = dd->ptrs[0]; //first block

		dir* start = (dir*) pages_get_page(block + 1); //start address
		int index;

		char rname[48];
		strcpy(rname, name);
		rname[strlen(name)] = '\0';
		int de_num = -1;

		for(int ii = 0; ii < dir_num; ii++) {
			if(strcmp(start->name, rname) == 0) {
				de_num = start->inum;
				index = ii;
				break;
			}
			else {
				//supdate start
				start = (dir*) ((void*) start + 64);
			}
		}
		if(de_num == -1) {
			return -1;
		}
		else {
			dir* next = (dir*) ((void*) start + 64);
			for(int ii = index + 1; ii < dir_num; ii++) {
				strcpy(start->name, next->name);
				start->name[strlen(next->name)] = '\0';
				//start->name = next->name;
				start->inum = next->inum;
				start = next;
				next = (dir*) ((void*) start + 64); 
			}
			dd->size = dd->size - 64;
			return de_num;
		}	
	}

}


