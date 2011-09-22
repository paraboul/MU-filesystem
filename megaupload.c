/*
    (c) Anthony Catel <a.catel@weelya.com> - 2011
    GPL v2
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <fuse.h>
#include "megaupload.h"
#include "log.h"
#include "md5.h"

static const char *mu_login_url = "http://www.megaupload.com/mgr_login.php";
static const char *mu_grab_url = "http://www.megaupload.com/mgr_dl.php?d=%s&u=%s";

#define CURL_HEADER_GET(header, txt) \
static int _curl_##header##_get(void *ptr, size_t size, size_t num, str_buffer_t *buffer) \
{ \
    if (num > sizeof(#header)+1 && strncasecmp(ptr, #txt": ", sizeof(#header)+1) == 0) { \
        num = num - (sizeof(#header)+1); \
        if (buffer->size == 0 || buffer->ptr == NULL) { \
            buffer->size = 0; \
            buffer->length = 0; \
            buffer->ptr = malloc(size * num + 1); \
        } else { \
            buffer->ptr = realloc(buffer->ptr, buffer->size + (size * num) + 1); \
        } \
        memcpy(buffer->ptr + buffer->length, ptr + sizeof(#header)+1, size * num); \
 \
        buffer->size = buffer->size + (size * num) + 1; \
        buffer->length += size*num; \
 \
        buffer->ptr[buffer->length-2] = '\0'; \
        return num + sizeof(#header)+1; \
    } \
    return num;\
}

CURL_HEADER_GET(location, Location)
CURL_HEADER_GET(content_length, Content-Length)


static int _curl_read(void *ptr, size_t size, size_t num, str_buffer_t *buffer)
{
    if (buffer == NULL) {
        return num;
    }
    if (buffer->size == 0 || buffer->ptr == NULL) {
        buffer->size = 0;
        buffer->length = 0;
        buffer->ptr = malloc(size * num + 1);
        
        if (buffer->ptr == NULL)
            return 0;
            
    } else {
        buffer->ptr = realloc(buffer->ptr, buffer->size + (size * num) + 1);
        if (buffer->ptr == NULL)
            return 0;
    }

    memcpy(buffer->ptr + buffer->length, ptr, size * num);

    buffer->size = buffer->size + (size * num) + 1;
    buffer->length += size*num;

    buffer->ptr[buffer->length] = '\0';

    return num;
}

static int _curl_read_fixed(void *ptr, size_t size, size_t num,
            str_buffer_t *buffer)
{
    if (buffer == NULL || 
        buffer->length + (size*num) > buffer->size) {
        
        return num;
    }

    memcpy(buffer->ptr + buffer->length, ptr, size * num);
    
    buffer->length += size*num;

    return num;
}

mu_session_t *mu_login(const char *login, const char *pass)
{
    mu_session_t *mu;
    str_buffer_t buffer;
    CURL *curl;
    CURLcode response;
    md5_context ctx;
    unsigned char md5sum[16];
    char md5str[33];
    int i;

    mu = malloc(sizeof(mu_session_t));
    
    if (mu == NULL) {
        return NULL;
    }
    
    if ((curl = curl_easy_init()) == NULL) {
        free(mu);
        return NULL;
    }
    
    buffer.size     = 0;
    buffer.length   = 0;
    buffer.ptr      = NULL;    
    
    mu->uid[0] = '\0';
    
    md5_starts(&ctx);
    md5_update(&ctx, (unsigned char *)pass, strlen(pass));
    md5_finish(&ctx, md5sum);
    
    for (i = 0; i < 16; i++) {
        sprintf(md5str + (i*2), "%02x", md5sum[i]);
    }
    md5str[32] = '\0';
    
    mu->creds = malloc(strlen(login) + 64);    
    sprintf(mu->creds, "u=%s&b=0&p=%s", login, md5str);

    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, mu->creds);
    curl_easy_setopt(curl, CURLOPT_URL, mu_login_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    if ((response = curl_easy_perform(curl)) != 0) {
        
        curl_easy_cleanup(curl);
        
        free(mu->creds);
        free(mu);
        
        return NULL;
    }
    
    if (buffer.length > 32) {
        memcpy(mu->uid, &buffer.ptr[2], 32);
        mu->uid[32] = '\0';
            
        log_msg("Loged to MU with uid <%s>\n", mu->uid);
    } else {
        log_msg("Failed to log with <%s>\n", login);
        free(mu->creds);
        free(mu);
        mu = NULL;
    }
    
    free(buffer.ptr);
    
    curl_easy_cleanup(curl);
    
    return mu;
}

size_t mu_get_file_size(const char *file)
{
    str_buffer_t buffer, cl;
    CURL *curl;
    CURLcode response;
    size_t ret;
    
    if ((curl = curl_easy_init()) == NULL) {
        return 0;
    }
    
    buffer.size     = 0;
    buffer.length   = 0;
    buffer.ptr      = NULL;
    
    cl.size     = 0;
    cl.length   = 0;
    cl.ptr      = NULL; 

    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_URL, file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _curl_content_length_get);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &cl);

    if ((response = curl_easy_perform(curl)) != 0 || cl.ptr == NULL) {
        curl_easy_cleanup(curl);
        return 0;
    }

    ret = atoi((char *)cl.ptr);
    free(cl.ptr);
    
    curl_easy_cleanup(curl);
    
    return ret;
}

char *mu_get_file(const char *mucode, mu_session_t *mu)
{
    str_buffer_t buffer, location;
    CURL *curl;
    CURLcode response;
    char mu_url[512];
    
    if ((curl = curl_easy_init()) == NULL) {
        return NULL;
    }
    
    buffer.size     = 0;
    buffer.length   = 0;
    buffer.ptr      = NULL;
    
    location.size     = 0;
    location.length   = 0;
    location.ptr      = NULL;    
        
    sprintf(mu_url, mu_grab_url, mucode, mu->uid);

    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_URL, mu_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _curl_location_get);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &location);

    if ((response = curl_easy_perform(curl)) != 0) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    curl_easy_cleanup(curl);
    
    return (char *)location.ptr;

}

/*
    TODO :
    - use the same curl session
    - use a ring buffer and request more data (cache)
*/
ssize_t mu_get_range(const char *file, size_t from, 
        size_t size, size_t fsize, str_buffer_t *buffer)
{
    CURL *curl;
    CURLcode response;
    char range[64];
    
    if ((curl = curl_easy_init()) == NULL) {
        return -1;
    }
    
    buffer->size     = size;
    buffer->length   = 0;
    
    if (from+size > fsize) {
        sprintf(range, "%d-", from);
    } else {
        sprintf(range, "%d-%d", from, (from+size)-1);
    }

    curl_easy_setopt(curl, CURLOPT_URL, file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read_fixed);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);

    if ((response = curl_easy_perform(curl)) != 0) {
        curl_easy_cleanup(curl);
        return -1;
    }
    
    curl_easy_cleanup(curl);
    
    if (buffer->length != size) {
        log_msg("MU size doesnt match\n");
        return -1;
    }
    
    log_msg("Requested : %d - %d\n", size, buffer->length);
    
    return size;
}

#if 0
int main()
{
    mu_session_t *mu;
    mu = mu_login("LezardSpock", "****");
    printf("Loged with : %s\n", mu->uid);

    
    return 0;
}
#endif
