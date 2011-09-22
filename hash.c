/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hash.h"


static unsigned int hash_string(const char *str)
{
        int hash = 5381;
        const char *s;
	
        for (s = str; *s != '\0'; s++) {
                hash = ((hash << 5) + hash) + tolower(*s);
        }
	
        return (hash & 0x7FFFFFFF)%(HACH_TABLE_MAX-1);
}

HTBL *hashtbl_init()
{
	HTBL_ITEM **htbl_item;
	HTBL *htbl;
	
	htbl = malloc(sizeof(*htbl));
	
	if (htbl == NULL) {
	    return NULL;
	}
	
	htbl_item = (HTBL_ITEM **) malloc( sizeof(*htbl_item) * (HACH_TABLE_MAX + 1) );
	
	if (htbl_item == NULL) {
	    free(htbl);
	    return NULL;
	}
	
	memset(htbl_item, 0, sizeof(*htbl_item) * (HACH_TABLE_MAX + 1));
	
	htbl->first = NULL;
	htbl->table = htbl_item;
	
	return htbl;
}

void hashtbl_free(HTBL *htbl)
{
	size_t i;
	HTBL_ITEM *hTmp;
	HTBL_ITEM *hNext;
	
	for (i = 0; i < (HACH_TABLE_MAX + 1); i++) {
		hTmp = htbl->table[i];
		while (hTmp != 0) {
			hNext = hTmp->next;
			free(hTmp->key);
			hTmp->key = NULL;
			free(hTmp);
			hTmp = hNext;
		}
	}
	
	free(htbl->table);
	free(htbl);	
}

void hashtbl_append(HTBL *htbl, const char *key, void *structaddr)
{
	unsigned int key_hash, key_len;
	HTBL_ITEM *hTmp, *hDbl;

	if (key == NULL) {
		return;
	}
	key_len = strlen(key);
	key_hash = hash_string(key);
	
	hTmp = (HTBL_ITEM *)malloc(sizeof(*hTmp));
	
	if (hTmp == NULL) {
	    return;
	}
	
	hTmp->next = NULL;
	hTmp->lnext = htbl->first;
	hTmp->lprev = NULL;

	hTmp->key = malloc(sizeof(char) * (key_len+1));
	
	if (hTmp->key == NULL) {
	    free(hTmp);
	    return;
	}
	
	hTmp->addrs = (void *)structaddr;
	
	memcpy(hTmp->key, key, key_len+1);
	
	if (htbl->table[key_hash] != NULL) {
		hDbl = htbl->table[key_hash];
		
		while (hDbl != NULL) {
			if (strcasecmp(hDbl->key, key) == 0) {
				free(hTmp->key);
				free(hTmp);
				hDbl->addrs = (void *)structaddr;
				
				return;
			} else {
				hDbl = hDbl->next;
			}
		}
		hTmp->next = htbl->table[key_hash];
	}
	
	if (htbl->first != NULL) {
		htbl->first->lprev = hTmp;
	}
	
	htbl->first = hTmp;
	
	htbl->table[key_hash] = hTmp;
}


void hashtbl_erase(HTBL *htbl, const char *key)
{
	unsigned int key_hash;
	HTBL_ITEM *hTmp, *hPrev;
	
	if (key == NULL) {
		return;
	}
	
	key_hash = hash_string(key);
	
	hTmp = htbl->table[key_hash];
	hPrev = NULL;
	
	while (hTmp != NULL) {
		if (strcasecmp(hTmp->key, key) == 0) {
			if (hPrev != NULL) {
				hPrev->next = hTmp->next;
			} else {
				htbl->table[key_hash] = hTmp->next;
			}
			
			if (hTmp->lprev == NULL) {
				htbl->first = hTmp->lnext;
			} else {
				hTmp->lprev->lnext = hTmp->lnext;
			}
			if (hTmp->lnext != NULL) {
				hTmp->lnext->lprev = hTmp->lprev;
			}
			
			free(hTmp->key);
			free(hTmp);
			return;
		}
		hPrev = hTmp;
		hTmp = hTmp->next;
	}
}

void *hashtbl_seek(HTBL *htbl, const char *key)
{
	unsigned int key_hash;
	HTBL_ITEM *hTmp;
	
	if (key == NULL) {
		return NULL;
	}
	
	key_hash = hash_string(key);
	
	hTmp = htbl->table[key_hash];
	
	while (hTmp != NULL) {
		if (strcasecmp(hTmp->key, key) == 0) {
			return (void *)(hTmp->addrs);
		}
		hTmp = hTmp->next;
	}
	
	return NULL;
}

