#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sqlite3.h>

#include "db.h"
#include "hash.h"
#include "main.h"

static const char *db_file = "/home/para/dev/fuse/mufs.db";
static const char *init_query = "CREATE TABLE mufiles(title VARCHAR(255) PRIMARY KEY, tag VARCHAR(12), size INT)";
static const char *insert_query = "INSERT INTO mufiles(title, size) VALUES(?, ?)";

static int sqlite_callback(void *not_used, int argc, 
        char **argv, char **col_name)
{
    int i;
    
    for(i = 0; i < argc; i++) {
        printf("%s = %s\n", col_name[i], argv[i] ? argv[i] : "NULL");
    }
    
    return 0;
}

int mu_insert_file(const char *title, size_t size)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    
    printf("Insert... %s %d\n", title, size);
    
    if (sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
        return 0;
        
    if (sqlite3_prepare_v2(db, insert_query, -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 1, title, strlen(title), SQLITE_STATIC) != SQLITE_OK ||
            sqlite3_bind_int(stmt, 2, size) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {

            printf("Unable to exec query %s\n", sqlite3_errmsg(db));
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
                
        return 0;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    printf("Query done\n");
    
    return 1;
}

mu_file_t *mu_db_get_infos(char *title)
{
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    mu_file_t *mu_file;

    const char *query = "SELECT size from mufiles WHERE title=? LIMIT 1";

    if (sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READONLY, NULL))
        return NULL;
        
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 1, title, strlen(title), SQLITE_STATIC) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_ROW) {
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        printf("No result %s\n", sqlite3_errmsg(db));
        
        return NULL;
    }
    
    mu_file = malloc(sizeof(mu_file_t));
    
    mu_file->title = strdup(title);
    mu_file->tag = NULL;
    mu_file->url = NULL;
    mu_file->size = sqlite3_column_int(stmt, 0);
    
    printf("Found : %s\n", mu_file->title);
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return mu_file;
}

int mu_init_db()
{
    sqlite3 *db;
    int rc;
    char *sqlerr = NULL;

    if ((rc = sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)))
        return 0;
        
    sqlite3_exec(db, init_query, sqlite_callback, NULL, &sqlerr);    

    sqlite3_close(db);

    return 1;
}

