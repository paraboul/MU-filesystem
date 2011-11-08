/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#ifndef _MUMAIN_H_
#define _MUMAIN_H_

#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>
#include <pthread.h>
#include "hash.h"

typedef struct
{
    char *creds;
    char uid[64];
} mu_session_t;


typedef struct
{
    FILE *logfile;
    HTBL *flist;
    mu_session_t *session;
} mu_state_t;

typedef struct
{
    char *title;
    char *tag;
    char *url;
    size_t size;
    
    struct {
        unsigned char *data;
        size_t length;
        size_t offset;
        size_t allocated;
    } buffer;
    
    struct {
        size_t offset;
        size_t size;
        size_t expected;
        size_t kill;
    } requested;
    
    struct {
        pthread_t thread;
        pthread_mutex_t ready;
        pthread_mutex_t read;
        pthread_mutex_t threadrun;
        int isrunning;
    } dl_thread;
    
} mu_file_t;

#define MU_DATA ((mu_state_t *)fuse_get_context()->private_data)

#endif
