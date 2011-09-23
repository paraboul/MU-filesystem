/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#ifndef _MUMAIN_H_
#define _MUMAIN_H_

#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>
#include "hash.h"
#include "megaupload.h"

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
} mu_file_t;

#define MU_DATA ((mu_state_t *)fuse_get_context()->private_data)

#endif
