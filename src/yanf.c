/**
 * yanf - Yet Another Non-volatile memory Filesystem
 *
 * Used for experimenting with and developing the liblightnvm interface.
 *
 * Usage:
 *
 * yanf -f -o direct_io,big_writes,max_write=131072,max_read=131072 /tmp/yanf
 *
 */
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <liblightnvm.h>

#ifdef YANF_DEBUG
	#define YANF_DEBUG(x, ...) printf("%s:%s-%d: " x "\n", __FILE__, \
		__func__, __LINE__, ##__VA_ARGS__);fflush(stdout);
#else
	#define YANF_DEBUG(x, ...)
#endif

struct entry {
	int used;
	char path[PATH_MAX];
	struct stat stat;
};

static struct entry *fs;
static int fs_ntotal;
static int fs_nused;
static int fs_inc = 30;

void _fs_print(void)
{
	printf("fs(%p), fs_inc(%d), fs_ntotal(%d), fs_nused(%d)\n",
		fs, fs_inc, fs_ntotal, fs_nused);
}

int _fs_grow(void)
{
	int grown_ntotal;
	struct entry *grown;

	grown_ntotal = fs_ntotal + fs_inc;
	grown = realloc(fs, grown_ntotal * sizeof(*grown));
	if (!grown)
		return -ENOMEM;

	// Initialize the new memory (don't touch the old)
	memset(grown + fs_ntotal, 0, fs_inc * sizeof(*grown));

	fs = grown;			// Update fs
	fs_ntotal = grown_ntotal;

	return 0;
}

int _fs_new(void)
{
	int i;

	for (i = 0; i < fs_ntotal; i++) {
		if (!fs[i].used) {
			fs_nused++;

			memset(&fs[i], 0, sizeof(fs[i]));
			fs[i].used = 1;
			return i;
		}
	}

	return -ENOMEM;
}

void _fs_destroy(int id)
{
	fs[id].used = 0;
	fs_nused--;
}

int _fs_find(const char *path)
{
	int i;

	for (i = 0; i < fs_ntotal; i++) {
		if (fs[i].used && (strcmp(fs[i].path, path) == 0))
			return i;
	}

	return -ENOENT;
}

int _fs_exists(const char *path)
{
	return _fs_find(path) >= 0;
}

static int yanf_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	YANF_DEBUG("_create(%s, ?, ?)\n", path);
	int entry;

	if (_fs_exists(path))
		return -EEXIST;

	entry = _fs_new();			// Get an entry
	if (entry < 0) {
		_fs_grow();
		entry = _fs_new();
		if (entry < 0)
			return -ENOMEM;
	}

	memcpy(fs[entry].path, path, PATH_MAX);	// Set it up
	fs[entry].stat.st_mode = mode;
	fs[entry].stat.st_mode = mode;
	fs[entry].stat.st_nlink = 1;
	fs[entry].stat.st_size = 0;

	time(&fs[entry].stat.st_atime);
	fs[entry].stat.st_mtime = fs[entry].stat.st_atime;
	fs[entry].stat.st_ctime = fs[entry].stat.st_atime;

	return 0;
}

static int yanf_getattr(const char *path, struct stat *stbuf)
{
	YANF_DEBUG("_getattr(%s, ?)\n", path);
	int entry;

	memset(stbuf, 0, sizeof(*stbuf));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	entry = _fs_find(path);
	if (entry < 0)
		return -ENOENT;

	memcpy(stbuf, &fs[entry].stat, sizeof(*stbuf));

	return 0;
}

static int yanf_truncate(const char *path, off_t size)
{
	YANF_DEBUG("_truncate(%s, %ld)\n", path, size);

	if (!_fs_exists(path))
		return -ENOENT;

	return 0;
}

static int yanf_open(const char *path, struct fuse_file_info *fi)
{
	YANF_DEBUG("_open(%s, ?)\n", path);

	if (!_fs_exists(path))
		return -ENOENT;

	// check fi->flags

	return 0;
}

static int yanf_read(const char *path, char *buf, size_t size, off_t offset,
			struct fuse_file_info *fi)
{
	YANF_DEBUG("_read(%s, ?, %lu, %ld, ?)\n", path, size, offset);

	if (!_fs_exists(path))
		return -ENOENT;

	// Read 'size' bytes from 'path' into 'buf' starting at 'offset'
	// Return the amount which was successfully copied into 'buf'

	memset(buf, 0, size);	// pseudo operation

	return size;
}

static int yanf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	YANF_DEBUG("_readdir(%s, ?, ?, %ld, ?)\n", path, offset);
	int i;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for (i = 0; i < fs_ntotal; i++) {
		if (fs[i].used)
			filler(buf, fs[i].path+1, NULL, 0);
	}

	return 0;
}

static int yanf_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	YANF_DEBUG("_write(%s, ?, %lu, %ld, ?)\n", path, size, offset);
	int entry;

	entry = _fs_find(path);
	if (entry < 0)
		return -ENOENT;

	fs[entry].stat.st_size += size;

	return size;
}

static int yanf_utimens(const char *path, const struct timespec tv[2])
{
	YANF_DEBUG("_utimens(%s)\n", path);

	return 0;
}

static int yanf_unlink(const char *path)
{
	int entry = _fs_find(path);

	if (entry < 0)
		return -ENOENT;

	_fs_destroy(entry);

	return 0;
}

static struct fuse_operations yanf_oper = {
	.getattr	= yanf_getattr,
	.truncate	= yanf_truncate,
	.open		= yanf_open,
	.readdir	= yanf_readdir,
	.read		= yanf_read,
	.write		= yanf_write,
	.create		= yanf_create,
	.utimens	= yanf_utimens,
	.unlink		= yanf_unlink,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &yanf_oper, NULL);
}

