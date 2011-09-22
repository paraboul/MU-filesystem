/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#define FUSE_USE_VERSION  26

#define _XOPEN_SOURCE 500

#include <limits.h>
#include <stdio.h>

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "log.h"
#include <curl/curl.h>
#include "megaupload.h"

static const char *entry_file = "/home/para/dev/fuse/list.txt";

static int mufs_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));
  if(strcmp(path, "/") == 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/Lost.Girl.avi") == 0) {
      log_msg("Get attr %s\n", path);
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 1386524358;
  }
  else {
    log_msg("Get attr not found %s\n", path);
      res = -ENOENT;
    }
  return res;
}

static int mufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    FILE *lst;
    (void) offset;
    (void) fi;
    char url[512];
    char title[512];
    
    if(strcmp(path, "/") != 0)
      return -ENOENT;
    
    lst = fopen(entry_file, "r");
    
    log_msg("readdir : %s\n", path);
    
    if (lst == NULL)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    while (fscanf(lst, "%s %s\n", title, url) != EOF) {
        filler(buf, title, NULL, 0);
    }

    return 0;
}

static int mufs_open(const char *path, struct fuse_file_info *fi)
{
    int fd;

    #if 0
    fd = open("/home/para/dev/fuse/futurama.mkv", fi->flags);
    
    if (fd < 0) {
        log_msg("cant open file\n");
        return -EACCES;
    }
    
    fi->fh = fd;
    
    log_msg("open file %s\n", path);
    /*if(strcmp(path, mufs_path) != 0)
        return -ENOENT;

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;*/
    #endif

    return 0;
}

int mufs_release(const char *path, struct fuse_file_info *fi)
{

    log_msg("\nmufs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);

    //return close(fi->fh);
    return 0;
}

static int mufs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    str_buffer_t buffer;
    log_msg("Read file : %s - size : %d - offset : %d\n", path, size, offset);
    /* TODO : directly pass buf to mu_get_range to avoid copy */
    mu_get_range("http://www926.megaupload.com/files/e68ab188370102dd619ac6b9b9ad3731/Lost.Girl.S02E02.VOSTFR.720p.WEB-DL.AAC2.0.H.264-GKS.mkv", offset, size, &buffer);
    
    memcpy(buf, (char *)buffer.ptr, buffer.length);
    
    free(buffer.ptr);
    
    return buffer.length;
    //return pread(fi->fh, buf, size, offset);
}

static struct fuse_operations mufs_oper = {
  .getattr   = mufs_getattr,
  .readdir = mufs_readdir,
  .open   = mufs_open,
  .read   = mufs_read,
  .release = mufs_release
};

int main(int argc, char *argv[])
{
    struct bb_state *bb_data;
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    bb_data = calloc(sizeof(struct bb_state), 1);
    
    if (bb_data == NULL) {
	    perror("main calloc");
	    abort();
    }
    
    bb_data->logfile = log_open();

    return fuse_main(argc, argv, &mufs_oper, bb_data);
}
