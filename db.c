#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sqlite3.h>

#include "db.h"
#include "hash.h"

static const char *db_file = "/home/para/dev/fuse/mufs.db";
static const char *init_query = "CREATE TABLE mufiles(title VARCHAR(255) PRIMARY KEY, tag VARCHAR(12), size INT)";

static int sqlite_callback(void *not_used, int argc, 
        char **argv, char **col_name)
{
    int i;
    
    for(i = 0; i < argc; i++) {
        printf("%s = %s\n", col_name[i], argv[i] ? argv[i] : "NULL");
    }
    
    return 0;
}

sqlite3 *mu_init_db()
{
    sqlite3 *db;
    int rc;
    char *sqlerr = NULL;
    
    if ((rc = sqlite3_open(db_file, &db)))
        return NULL;
        
    sqlite3_exec(db, init_query, sqlite_callback, 0, &sqlerr);    

    return db;
}

