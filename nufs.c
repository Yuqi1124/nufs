// based on cs3650 starter code

#include <stdio.h>
#include <string.h>
//#include <bsd/string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <bsd/string.h>
#include <assert.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "pages.h" 
#include "inode.h"
#include "bitmap.h"
#include "directory.h"


void*
inode_bitmap_start() {
	return get_pages_bitmap();
}

void*
block_bitmap_start() {
	return (void*) get_pages_bitmap() + 8;
}

void*
inodes_get(int num) {
	return (void*) get_pages_bitmap() + 40 + num*sizeof(inode);
}

void*
blocks_get(int num) {
	return pages_get_page(1 + num);
}

int
get_empty_block() {
	void* bm = block_bitmap_start(); 
	for(int ii = 0; ii < 255; ii++) {
		if(bitmap_get(bm, ii) == 0) {
			bitmap_put(bm, ii, 1);
			return ii;
		}
	}
	return -1;
}

// implementation for: man 2 access
// Checks if a file exists.
int
nufs_access(const char *path, int mask)
{
    int rv = 0;
    //because getattr already checked if file exist or not,
    //so I think it is not necessary at least for this time
    printf("access(%s, %04o) -> %d\n", path, mask, rv);
    return rv;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int
nufs_getattr(const char *path, struct stat *st)
{
    int rv = 0;
    inode* rn = (inode*) inodes_get(0);
    int p = directory_lookup(rn, path);
    if (strcmp(path, "/") == 0) {
        st->st_mode = 040755; // directory
        st->st_size = 0;
	st->st_nlink = 1; //may add more late
        st->st_uid = getuid(); //may be not necessary
    }
    else if (strcmp(path, "/hello.txt") == 0) {
        st->st_mode = 0100644; // regular file
        st->st_size = 6;
	st->st_nlink = 1;
        st->st_uid = getuid();
    }
    
    else if (p != -1) {
        inode* in = (inode*) inodes_get(p);
	st->st_mode = in->mode; // regular file
        st->st_size = in->size;
	st->st_nlink = in->refs;
        st->st_uid = getuid();
    }
    else {
	rv = -ENOENT;
    }
    printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv, st->st_mode, st->st_size);
    return rv;
}

// implementation for: man 2 readdir
// lists the contents of a directory 
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    struct stat st;
    int rv;

    rv = nufs_getattr("/", &st);
    assert(rv == 0);

    filler(buf, ".", &st, 0);

    //we know our only dic is root dic, so don't need to do two map now
    inode* in = (inode*) inodes_get(0);
    int sz = in->size;
    dir* rt = (dir*) blocks_get(in->ptrs[0]); 
    int dir_num = sz / 64;

    for(int ii = 0; ii < dir_num; ii++) {
	rv = nufs_getattr(rt->name, &st);
	assert(rv == 0);
	char store[48];
	int jj;
	for(jj = 0; jj < strlen(rt->name) - 1; jj++) {
		store[jj] = rt->name[jj+1];
	}
	store[jj + 1] = '\0';
    	filler(buf, store, &st, 0);
	rt = (dir*) ((void*) rt + 64);
    }

    //in addition list hello...
    rv = nufs_getattr("/hello.txt", &st);
    assert(rv == 0);
    filler(buf, "hello.txt", &st, 0);
    
    printf("readdir(%s) -> %d\n", path, rv);
    return rv;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    struct stat st;
    int rv = nufs_getattr(path, &st);
    //check if path valid
    //get a new node
    int inum = -1;
    void* map_start = inode_bitmap_start();
    for(int ii = 0; ii < 64; ii++) {
	 if(bitmap_get(map_start, ii) == 0) {
		inum = ii;
		bitmap_put(map_start, ii, 1);
		break;
	 }
    }
    if (inum != -1) {
	//can make helper later
	inode* in = (inode*) inodes_get(inum);
	in->refs = 1;
	in->mode = mode;
	in->size = 0;

	int b = get_empty_block();
	if(b != -1) {
		in->ptrs[0] = b;
	}
	in->iptr = -1; //show not use

	//add entires to root dir (block 0)
	//get inode of 0
	inode* rn = (inode*) inodes_get(0);
	int dir_num = rn->size / 64;
	void* start = (void*)blocks_get(0) + dir_num*64;	
	dir* d = (dir*)start;

	strcpy(d->name, path);
	d->name[strlen(path)] = '\0';

	d->inum = inum;
	//update root inode's size
	rn->size = rn->size + 64;
	rv = 0;
    }
    printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
nufs_mkdir(const char *path, mode_t mode)
{
    int rv = nufs_mknod(path, mode | 040000, 0);
    printf("mkdir(%s) -> %d\n", path, rv);
    return rv;
}

int
nufs_unlink(const char *path)
{
    int rv = -1;

    inode* rn = (inode*) inodes_get(0);
    int inum = directory_delete(rn, path);
    if (inum != -1) {
	inode* nn = (inode*) inodes_get(inum);
	nn->refs = nn->refs - 1;
	if(nn->refs == 0) {
		void* map_start = inode_bitmap_start();
		bitmap_put(map_start, inum, 0);
		free_page(nn->ptrs[0]);
	}
	rv = 0;
    }
    else {
        printf("actively return file not found (unlink)\n");
	rv = -ENOENT;
    }
    
    printf("unlink(%s) -> %d\n", path, rv);
    return rv;
}

int
nufs_link(const char *from, const char *to)
{
    int rv = -1;
    printf("link(%s => %s) -> %d\n", from, to, rv);
	return rv;
}

int
nufs_rmdir(const char *path)
{
    int rv = -1;
    printf("rmdir(%s) -> %d\n", path, rv);
    return rv;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int
nufs_rename(const char *from, const char *to)
{
    int rv = 0;
    inode* rn = (inode*) inodes_get(0);
    int i = directory_rename(rn, from, to);
    if (i == -1) {
	rv = -ENOENT;
    }
    printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
}

int
nufs_chmod(const char *path, mode_t mode)
{
    int rv = -1;
    printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

int
nufs_truncate(const char *path, off_t size)
{
    int rv = 0;
    printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
    return rv;
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;
    printf("open(%s) -> %d\n", path, rv);
    return rv;
}
//buf: read into; offset: starting point, fi
// Actually read data
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct stat st;
    int rv = nufs_getattr(path, &st);
    inode* rn = (inode*) inodes_get(0);
    int p = directory_lookup(rn, path);
    inode* in = (inode*) inodes_get(p);
    if(size + offset > in->size) {
	rv = in->size;
    }
    else {
	rv = size;
    }
    char* block = (char*) ((char*) blocks_get(in->ptrs[0]) + offset);
    //strlcpy(buf, block ,size); this function caused error...
    memcpy(buf, block, rv);
    printf("buf: %s\n", buf);
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

// Actually write data
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct stat st;
    int rv = nufs_getattr(path, &st);
    inode* rn = (inode*) inodes_get(0);
    int p = directory_lookup(rn, path);
    inode* in = (inode*) inodes_get(p);
    rv = size;
    char* block = (char*) ((char*) blocks_get(in->ptrs[0]) + offset);
    //strlcpy(buf, block ,size);
    //stackoverflow issues, edge case can be consider
    memcpy(block, buf, rv);
    in->size += rv;
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

// Update the timestamps on a file or directory.
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    int rv = -1;
    printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n",
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
	return rv;
}

// Extended operations
int
nufs_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi,
           unsigned int flags, void* data)
{
    int rv = -1;
    printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
    return rv;
}

//initialize root dir: set inode for it
//place at inode bitmap position 0, so is inode 0
void
root_init(void* start) {
	void* go_up = (void*) start + 40;
	inode* inode_start = (inode*) go_up;
	inode_start->refs = 1;
	inode_start->mode = 040755; //directory
	//inode_start->size = 0;
	printf("in root init, SIZE is %d\n", inode_start->size);
	inode_start->ptrs[0] = 0; //point to block 0
	inode_start-> iptr = -1; //show not use yet
	void* blockmapst = block_bitmap_start();
	bitmap_put(blockmapst, 0, 1);
}


void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
    ops->link     = nufs_link;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open	  = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
    ops->ioctl    = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    int p = --argc;
    printf("TODO: mount %s as data file\n", argv[p]);
    //storage_init(argv[--argc]);
    //init data.nufs
    pages_init(argv[p]);
    void* start = pages_get_page(0);
    root_init(start);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}

