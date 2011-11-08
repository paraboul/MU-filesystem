/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#ifndef _MU_H_
#define _MU_H_

#include "main.h"

#define MU_BUFFER_SIZE (400L*1024L*1024L)

typedef struct
{
    unsigned char *ptr;
    size_t size;
    size_t length;
} str_buffer_t;

struct mu_dl_args_s
{
    mu_file_t *file;
    size_t from;
    size_t size;
};

mu_session_t *mu_login(const char *login, const char *pass);
char *mu_get_file(const char *mucode, mu_session_t *mu);
ssize_t mu_get_range(mu_file_t *mu_file, size_t from, 
        size_t size, str_buffer_t *buffer);
size_t mu_get_file_size(const char *file);

#endif

