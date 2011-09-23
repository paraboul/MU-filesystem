/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#ifndef _MUDB_H_
#define _MUDB_H_

#include <unistd.h>
#include <sqlite3.h>

#include "main.h"

int mu_init_db();
int mu_insert_file(const char *title, size_t size);

mu_file_t *mu_db_get_infos(char *title);

#endif
