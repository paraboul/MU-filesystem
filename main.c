/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
    
    build : gcc -Wall `pkg-config fuse --cflags --libs` -o mufs main.c log.c hash.c megaupload.c md5.c -lcurl
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
#include <signal.h>
#include <pthread.h>

#include <curl/curl.h>
#include <sqlite3.h>

#include "log.h"
#include "megaupload.h"
#include "db.h"
#include "main.h"

static const char *entry_file = "/home/para/dev/fuse/list.txt";

static int mufs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    mu_file_t *mu_file;
    mu_state_t *state = MU_DATA;
    
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        
    } else if ((mu_file = hashtbl_seek(state->flist, &path[1])) != NULL) {

        if (mu_file->size == 0) {
            printf("Init file info %s\n", path);
            
            if (mu_file->url == NULL)
                mu_file->url = mu_get_file(mu_file->tag, state->session);
                
            mu_file->size   = mu_get_file_size(mu_file->url);
            
            mu_insert_file(mu_file->title, mu_file->size);
        }
        
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = mu_file->size;
        
    } else {
        res = -ENOENT;
    }
    return res;
}

static int mufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    
    FILE *lst;
    char url[512];
    char title[512];
    
    mu_state_t *state = MU_DATA;
    
    if(strcmp(path, "/") != 0)
      return -ENOENT;
    
    lst = fopen(entry_file, "r");

    if (lst == NULL)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    while (fscanf(lst, "%s %s\n", title, url) != EOF) {
        mu_file_t *mu_file;
        
        if ((mu_file = hashtbl_seek(state->flist, title)) == NULL) {
            
            if ((mu_file = mu_db_get_infos(title)) == NULL) {

                mu_file = malloc(sizeof(mu_file_t));

                mu_file->size = 0;
                mu_file->title = strdup(title);

            } /* else : grab from BDD */
            
            mu_file->tag = strdup(strchr(url, '=')+1);
            mu_file->url = NULL;      
            mu_file->buffer.data = NULL;
            
            mu_file->requested.offset       = 0;
            mu_file->requested.size         = 0;
            mu_file->requested.expected     = 0;
            mu_file->requested.kill         = 0;
            mu_file->dl_thread.isrunning    = 0;
            
            pthread_mutex_init(&mu_file->dl_thread.read, NULL);
            pthread_mutex_init(&mu_file->dl_thread.threadrun, NULL);
                            
            hashtbl_append(state->flist, title, mu_file);
        }
        
        filler(buf, title, NULL, 0);
    }
    
    fclose(lst);

    return 0;
}

static int mufs_open(const char *path, struct fuse_file_info *fi)
{
    mu_state_t *state = MU_DATA;
    mu_file_t *mu_file;
    
    if ((mu_file = hashtbl_seek(state->flist, &path[1])) == NULL) {
        return -ENOENT;
    }
    
    if (mu_file->url == NULL) {
        mu_file->url = mu_get_file(mu_file->tag, state->session);
    }

    return 0;
}

int mufs_release(const char *path, struct fuse_file_info *fi)
{

    
    //return close(fi->fh);
    return 0;
}

static int mufs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    mu_file_t *mu_file;
    mu_state_t *state = MU_DATA;
    str_buffer_t buffer;
    
    mu_file = hashtbl_seek(state->flist, &path[1]);
    
    printf("Read file : %s - size : %d - offset : %d\n", path, size, offset);
    
    buffer.ptr = (unsigned char *)buf;

    pthread_mutex_lock(&mu_file->dl_thread.read);
    printf("Ask for %d\n", size);
    mu_get_range(mu_file, offset, size, &buffer);
    pthread_mutex_unlock(&mu_file->dl_thread.read);
    printf("Got : %d\n", buffer.length);
    
    return buffer.length;
}

static mu_state_t *mu_init_state()
{
    mu_state_t *mu_state;
    
    signal(SIGPIPE, SIG_IGN);
        
    if ((mu_state = malloc(sizeof(mu_state_t))) == NULL)
        return NULL;
        
    mu_state->logfile   = log_open();
    mu_state->flist     = hashtbl_init();
    
    if (mu_state->flist == NULL)
        return NULL;
    
    if (!mu_init_db())
        return NULL;
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    if ((mu_state->session = mu_login("LezardSpock", "****")) == NULL)
        return NULL;
    
    return mu_state;
}

static struct fuse_operations mufs_oper = {
    .getattr      = mufs_getattr,
    .readdir      = mufs_readdir,
    .open         = mufs_open,
    .read         = mufs_read,
    .release      = mufs_release
};

int main(int argc, char *argv[])
{
    mu_state_t *mu_state;
    
    if ((mu_state = mu_init_state()) == NULL) {
        return 0;
    }
    
    return fuse_main(argc, argv, &mufs_oper, mu_state);
}

