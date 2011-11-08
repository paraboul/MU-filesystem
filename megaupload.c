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
#include <pthread.h>
#include "megaupload.h"
#include "log.h"
#include "md5.h"
#include "main.h"

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

static int _curl_read_thread(void *ptr, size_t size, size_t num,
            mu_file_t *mu_file)
{
    size_t end, end_r;
    
    if (mu_file->requested.kill) {
        mu_file->requested.kill = 0;
        printf("Thread is about to be killed\n");
        return -1;
    }

    if (mu_file->buffer.length + (size*num) > mu_file->buffer.allocated) {
        mu_file->buffer.offset += mu_file->buffer.length + (size * num);
        mu_file->buffer.length = 0;
        printf("Overflow\n");
    } 
    
    memcpy(mu_file->buffer.data + mu_file->buffer.length, ptr, size*num);
    
    mu_file->buffer.length += size*num;
    end = mu_file->buffer.offset + mu_file->buffer.length;
    end_r = mu_file->requested.offset + mu_file->requested.size;
    
    if (mu_file->requested.size != 0 && 
            mu_file->requested.offset >= mu_file->requested.offset &&
            end >= end_r) {
        
        mu_file->requested.size = 0;
        printf("Unlock\n");
        pthread_mutex_unlock(&mu_file->dl_thread.ready);
    }
    
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
            
        printf("Loged to MU with uid <%s>\n", mu->uid);
    } else {
        printf("Failed to log with <%s>\n", login);
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

void *mu_thread_download(void *arg)
{
    CURL *curl;
    CURLcode response;
    char range[64];
    size_t from, size;
    mu_file_t *mu_file;
    struct mu_dl_args_s *args = (struct mu_dl_args_s *)arg;
    
    mu_file = args->file;
    from    = args->from;
    size    = args->size;
    
    printf("Thread for (%d-%d)\n", from, from+size);
    
    if ((curl = curl_easy_init()) == NULL) {
        printf("Failed to init curl\n");
        mu_file->requested.kill = 0;
        mu_file->dl_thread.isrunning = 0;

        return NULL;
    }
    
    if (mu_file->buffer.data == NULL) {
        mu_file->buffer.data = malloc(MU_BUFFER_SIZE);
        if (mu_file->buffer.data == NULL) {
            printf("Cannot allocate memory\n");
            mu_file->requested.kill = 0;
            mu_file->dl_thread.isrunning = 0;

            return NULL;
        }
        mu_file->buffer.allocated = MU_BUFFER_SIZE;
    }
    
    mu_file->buffer.length = 0;
    mu_file->buffer.offset = from;
    
    sprintf(range, "%d-", from);
    
    curl_easy_setopt(curl, CURLOPT_URL, mu_file->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read_thread);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, mu_file);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);

    if ((response = curl_easy_perform(curl)) != 0) {
        printf("Thread terminated (%d-%d)\n", from, from+size);
        curl_easy_cleanup(curl);
        free(args);
        
        mu_file->requested.kill = 0;
        mu_file->dl_thread.isrunning = 0;

        return NULL;
    }
    
    printf("Thread ended  (%d-%d)\n", from, from+size);
    
    curl_easy_cleanup(curl);

    free(args);
    
    mu_file->requested.kill = 0;
    mu_file->dl_thread.isrunning = 0;

    return NULL;
}

ssize_t mu_get_range(mu_file_t *mu_file, size_t from, 
        size_t size, str_buffer_t *buffer)
{
    struct mu_dl_args_s *args;
    static int npass = 0;
    int npass_ = 0;
    
    npass++;
    npass_ = npass;
    
    if (from+size > mu_file->size) {
        size = mu_file->size - from;
    }
    
    if (mu_file->requested.expected != from) {
        printf("[%d] Pre jump (%d but %d)\n", npass_, mu_file->requested.expected, from);
        mu_file->requested.kill = 1;
        printf("[%d] Wait for old to terminate...\n", npass_);
        pthread_join(mu_file->dl_thread.thread, NULL);
        mu_file->requested.kill = 0;
        printf("[%d] Success terminated %d\n", npass_, mu_file->requested.kill);
        pthread_mutex_destroy(&mu_file->dl_thread.ready);
    }
    
    printf("[%d] Requested : %d of size %d (%d-%d) (%d)\n", npass_, from, size, from, from+size, mu_file->requested.kill);
    
    mu_file->requested.offset = from;

    if (!mu_file->dl_thread.isrunning) {
        printf("[%d] About to summon thread %d\n", npass_, mu_file->requested.kill);
        args = malloc(sizeof(struct mu_dl_args_s));
        args->file = mu_file;
        args->from = from;
        args->size = size;
        
        pthread_mutex_init(&mu_file->dl_thread.ready, NULL);
        pthread_mutex_lock(&mu_file->dl_thread.ready);

        mu_file->requested.size = size;
        
        mu_file->dl_thread.isrunning = 1;

        pthread_create(&mu_file->dl_thread.thread, NULL, 
                            mu_thread_download, args);
    }
    
    mu_file->requested.size = size;
    mu_file->requested.expected = from+size;
    
    printf("[%d] Wait for data...\n", npass_);
    pthread_mutex_lock(&mu_file->dl_thread.ready);
    printf("[%d] Done.\n", npass_);
    
    memcpy(buffer->ptr, 
        mu_file->buffer.data + (from - mu_file->buffer.offset), size);

    buffer->length = size;
    
    return 0;
}

/*
    TODO :
    - use the same curl session
    - use a ring buffer and request more data (cache)
*/
#if 0
ssize_t mu_get_range(mu_file_t *mu_file, size_t from, 
        size_t size, str_buffer_t *buffer)
{
    CURL *curl;
    CURLcode response;
    char range[64];
    
    if ((curl = curl_easy_init()) == NULL) {
        printf("Failed to init curl\n");
        return -1;
    }
    
    if (mu_file->buffer.data == NULL) {
        mu_file->buffer.data = malloc(8L * 1024L * 1024L);
        if (mu_file->buffer.data == NULL) {
            printf("Cannot allocate memory\n");
            return -1;
        }
        mu_file->buffer.allocated = 8L * 1024L * 1024L;
        mu_file->buffer.length = 0;
        mu_file->buffer.offset = 0;
    }
    
    if (size + from <= mu_file->buffer.length + mu_file->buffer.offset) {
        
    }
    
    buffer->size     = size;
    buffer->length   = 0;
    
    if (from+size > mu_file->size) {
        sprintf(range, "%d-", from);
    } else {
        sprintf(range, "%d-%d", from, (from+size)-1);
    }
    
    printf("Request range : %s\n", range);

    curl_easy_setopt(curl, CURLOPT_URL, mu_file->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_read_fixed);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);

    if ((response = curl_easy_perform(curl)) != 0) {
        printf("Failed to perform curl %s (%s)\n", mu_file->url, curl_easy_strerror(response));
        curl_easy_cleanup(curl);
        return -1;
    }
    
    curl_easy_cleanup(curl);
    
    if (buffer->length != size) {
        printf("MU size doesnt match\n");
        return -1;
    }
    
    printf("Requested : %d - %d\n", size, buffer->length);
    
    return size;
}
#endif

#if 0
int main()
{
    mu_session_t *mu;
    mu = mu_login("LezardSpock", "****");
    printf("Loged with : %s\n", mu->uid);

    
    return 0;
}
#endif
