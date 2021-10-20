#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "./lab_png.h"
#include "./crc.c"
#include "./zutil.c"
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>


#define PNG_TOP_LENGTH 33
#define PNG_BOT_LENGTH 12
#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240  /* 1024*10 = 10K */

/* This is a flattened structure, buf points to 
   the memory address immediately after 
   the last member field (i.e. seq) in the structure.
   Here is the memory layout. 
   Note that the memory is a chunk of continuous bytes.

   On a 64-bit machine, the memory layout is as follows:
   +================+
   | buf            | 8 bytes
   +----------------+
   | size           | 8 bytes
   +----------------+
   | max_size       | 8 bytes
   +----------------+
   | seq            | 4 bytes
   +----------------+
   | padding        | 4 bytes
   +----------------+
   | buf[0]         | 1 byte
   +----------------+
   | buf[1]         | 1 byte
   +----------------+
   | ...            | 1 byte
   +----------------+
   | buf[max_size-1]| 1 byte
   +================+
*/
typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }
    
    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */
    
    return 0;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}

int main( int argc, char** argv ) 
{
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF *p_shm_recv_buf;
    int shmid;
    int shm_size = sizeof_shm_recv_buf(BUF_SIZE);
    char fname[256];
    pid_t pid =getpid();
    pid_t cpid = 0;
    
    printf("shm_size = %d.\n", shm_size);
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_shm_recv_buf = shmat(shmid, NULL, 0);
    shm_recv_buf_init(p_shm_recv_buf, BUF_SIZE);


    if (argc == 1) {
        strcpy(url, IMG_URL); 
    } else {
        strcpy(url, argv[1]);
    }
    printf("%s: URL is %s\n", argv[0], url);

    cpid = fork();

    if ( cpid == 0 ) {          /* child proc download */
    
        curl_global_init(CURL_GLOBAL_DEFAULT);

        /* init a curl session */
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return 1;
        }

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)p_shm_recv_buf);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)p_shm_recv_buf);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    
        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("%lu bytes received in memory %p, seq=%d.\n",  \
                   p_shm_recv_buf->size, p_shm_recv_buf->buf, p_shm_recv_buf->seq);
        
        }
        /* cleaning up */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        shmdt(p_shm_recv_buf);
    } else if ( cpid > 0 ) {    /* parent proc */
        int state;
        waitpid(cpid, &state, 0);
        printf("Received ./output_%d_%d.png", p_shm_recv_buf->seq, pid);
        //write_file(fname, p_shm_recv_buf->buf, p_shm_recv_buf->size);
        shmdt(p_shm_recv_buf);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("fork");
        abort();
    }


    return 0;
}

// void dataToChunk(chunk_p chunk, U8* data, size_t size){
//     chunk->p_data = malloc(sizeof(U8)*size);
//     memcpy(chunk->p_data, data + 33, size);
//     chunk->length = size;
// }


// int main(int argc, char** argv){
//     FILE *f = fopen("uweng.png", "rb");
//     if(f == NULL){
//         printf("File not found\n");
//         return -1;
//     }

//     chunk_p tempChunk = malloc(sizeof(struct chunk));
//     U8* inData = (U8*)malloc(100);
//     fread(inData, sizeof(U8)*100, 1, f);
//     for(int i = 0; i < 100; i++){
//         printf("%d: %x\n", i, inData[i]);
//     }
//     dataToChunk(tempChunk, inData, 100-33);
//     for(int i = 0; i < 100-33; i++){
//         printf("%d: %x\n", i, tempChunk->p_data[i]);
//     }
    


//     return 0;
// }