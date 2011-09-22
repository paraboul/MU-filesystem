/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#ifndef _MU_H_
#define _MU_H_

typedef struct
{
    char *creds;
    char uid[64];
} mu_session_t;

typedef struct
{
    unsigned char *ptr;
    size_t size;
    size_t length;
} str_buffer_t;

mu_session_t *mu_login(const char *login, const char *pass);
char *mu_get_file(const char *mucode, mu_session_t *mu);
ssize_t mu_get_range(const char *file, size_t from, 
        size_t size, size_t fsize, str_buffer_t *buffer);
size_t mu_get_file_size(const char *file);

#endif
