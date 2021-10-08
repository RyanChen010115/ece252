#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include "./lab_png.h"
#include "./crc.c"
#include "./zutil.c"
#include <pthread.h>
#include <semaphore.h>


#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define STRIP_HEIGHT 6

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })




typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;
sem_t sem;
sem_t catsem;
int activeThreads = 0;
char url[256];
int numThreads;
int lim = 50;

U32 swap(U32 value)
{
    value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
    return (value << 16) | (value >> 16);
}

void getDataIHDR(FILE *f, data_IHDR_p res, U32* IHDRwidth, U32* IHDRheight){
    U32 *width_ptr = malloc(sizeof(U32));
    U32 *length_ptr = malloc(sizeof(U32));
    U8 *temp_ptr = malloc(sizeof(U8));
    fread(width_ptr, sizeof(U32), 1, f);
    res->width = ((*width_ptr>>24)&0xff) | ((*width_ptr<<8)&0xff0000) | ((*width_ptr>>8)&0xff00) | ((*width_ptr<<24)&0xff000000); 
    *IHDRwidth = *width_ptr;
    fread(length_ptr, sizeof(U32), 1, f);
    res->height = ((*length_ptr>>24)&0xff) | ((*length_ptr<<8)&0xff0000) | ((*length_ptr>>8)&0xff00) | ((*length_ptr<<24)&0xff000000);
    *IHDRheight += *length_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->bit_depth = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->color_type = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->compression = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->filter = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->interlace = *temp_ptr;
    free(width_ptr);
    free(length_ptr);
    free(temp_ptr);
    return;
}

U32 getIDAT(FILE *f, chunk_p res){
    U32 *length_ptr = malloc(sizeof(U32));
    U32 *crc_ptr = malloc(sizeof(U32));
    fread(length_ptr, sizeof(U32), 1, f);
    res->length = ((*length_ptr>>24)&0xff) | ((*length_ptr<<8)&0xff0000) | ((*length_ptr>>8)&0xff00) | ((*length_ptr<<24)&0xff000000);
    fread(res->type, sizeof(res->type), 1, f);
    res->p_data = (U8*)malloc(res->length);

    fread(res->p_data, sizeof(U8)*res->length, 1, f);
    fread(crc_ptr, sizeof(U32), 1, f);
    res->crc = *crc_ptr;
    free(length_ptr);
    free(crc_ptr);
    return *length_ptr;
}

U32 getIHDRcrc(data_IHDR_p IDHRdata, U32* IHDRtype, U32* width, U32* height){
    FILE *write = fopen("./IHDR.png", "wb+");
    fwrite(IHDRtype, sizeof(U32), 1, write);
    fwrite(width, sizeof(U32), 1, write);
    fwrite(height, sizeof(U32), 1, write);
    *height = swap(*height);
    fwrite(&IDHRdata->bit_depth, sizeof(U8), 1, write);
    fwrite(&IDHRdata->color_type, sizeof(U8), 1, write);
    fwrite(&IDHRdata->compression, sizeof(U8), 1, write);
    fwrite(&IDHRdata->filter, sizeof(U8), 1, write);
    fwrite(&IDHRdata->interlace, sizeof(U8), 1, write);
    fclose(write);
    FILE *read = fopen("./IHDR.png", "rb");
    U8 IHDRcrcdata[17];
    fread(IHDRcrcdata, sizeof(IHDRcrcdata), 1, read);
    U32 crc_val = crc(IHDRcrcdata, 17);
    fclose(read);
    return crc_val;
}

U32 getIDATcrc(chunk_p IDATchunk, U64 length){
    U8 *IDATall = malloc(sizeof(U8) * length + 4);
    IDATall[0] = IDATchunk->type[0];
    IDATall[1] = IDATchunk->type[1];
    IDATall[2] = IDATchunk->type[2];
    IDATall[3] = IDATchunk->type[3];
    for(int i = 4; i < length + 4; i++){
        IDATall[i] = IDATchunk->p_data[i-4];
    }
    U32 crc_val = crc(IDATall, length + 4);
    return crc_val;
}


int catpng(int argc){
    const int NUM_FILES = argc - 1;

    U32 totalHeight = 0;
    U32 totalLength = 0;
    U32 totalLengthN = 0;
    U64 totalDecompLength = 0;
    

    unsigned char header[8];
    U32* IHDRlength = malloc(sizeof(U32));
    U32* IHDRtype = malloc(sizeof(U32));
    U32* IHDRCRC = malloc(sizeof(U32));
    U32* IHDRwidth = malloc(sizeof(U32));
    U32* IHDRheight = malloc(sizeof(U32));
    data_IHDR_p IHDRdata = malloc(sizeof(struct data_IHDR));

    chunk_p IDATchunk = malloc(sizeof(struct chunk));

    chunk_p chunkPTR[NUM_FILES];
    U64 lenArr[NUM_FILES];
    U32 IEND[3];

    for(int i = 1; i < argc; i++){
        char fname[256];
        sprintf(fname, "./output_%d.png", i-1);
        FILE *f = fopen(fname, "rb");
        if(f == NULL){
            printf("File not found\n");
            return -1;
        }

        //Read IDHR
        fread(header, sizeof(header), 1, f);
        fread(IHDRlength, sizeof(U32), 1, f);
        fread(IHDRtype, sizeof(U32), 1, f);
        getDataIHDR(f, IHDRdata, IHDRwidth, IHDRheight);
        fread(IHDRCRC, sizeof(U32), 1, f);
        totalHeight += IHDRdata->height;

        //Read IDAT
        totalLength += getIDAT(f, IDATchunk);
        totalLengthN += IDATchunk->length;

        //Uncompress data
        chunk_p tempChunk = malloc(sizeof(struct chunk));
        tempChunk->crc = IDATchunk->crc;
        tempChunk->length = IDATchunk->length;
        tempChunk->type[0] = IDATchunk->type[0];
        tempChunk->type[1] = IDATchunk->type[1];
        tempChunk->type[2] = IDATchunk->type[2];
        tempChunk->type[3] = IDATchunk->type[3];
        U64 decompLength = (U64)((IHDRdata->height)*(IHDRdata->width * 4 + 1));
        tempChunk->p_data = (U8*)malloc(decompLength);
        mem_inf(tempChunk->p_data, &decompLength, IDATchunk->p_data, (U64)IDATchunk->length);
        totalDecompLength += decompLength;
        lenArr[i - 1] = decompLength;
        chunkPTR[i - 1] = tempChunk;
        
        //Read IEND
        fread(IEND, sizeof(IEND), 1, f);
        free(IDATchunk->p_data);
        fclose(f);
    }


    U8* uIDATdata = malloc(sizeof(U8)*totalDecompLength);
    int k = 0;
    for(int i = 0; i < NUM_FILES; i++){
        for(int j = 0; j < lenArr[i]; j++){
            uIDATdata[k] = chunkPTR[i]->p_data[j];
            k++;
        }
    }

    U8* fIDATdata = malloc(sizeof(U8)*totalDecompLength);
    U64 IDATcomplength = 0;
    mem_def(fIDATdata, &IDATcomplength, uIDATdata, totalDecompLength, Z_BEST_COMPRESSION);

    U32 tempHeight = (argc - 1) * STRIP_HEIGHT;
    U32 IHDRcrc = getIHDRcrc(IHDRdata, IHDRtype, IHDRwidth, &tempHeight);

    chunk_p fIDATchunk = malloc(sizeof(struct chunk));

    fIDATchunk->length = IDATcomplength;
    fIDATchunk->type[0] = chunkPTR[0]->type[0];
    fIDATchunk->type[1] = chunkPTR[0]->type[1];
    fIDATchunk->type[2] = chunkPTR[0]->type[2];
    fIDATchunk->type[3] = chunkPTR[0]->type[3];
    fIDATchunk->p_data = fIDATdata;
    U32 IDATcrc = getIDATcrc(fIDATchunk, IDATcomplength);

    //writing to file
    FILE *all = fopen("all.png", "wb+");

    //write header
    fwrite(header, sizeof(header), 1, all);

    //write IHDR
    fwrite(IHDRlength, sizeof(U32), 1, all);
    fwrite(IHDRtype, sizeof(U32), 1, all);
    fwrite(IHDRwidth, sizeof(U32), 1, all);
    fwrite(&tempHeight, sizeof(U32), 1, all);
    fwrite(&IHDRdata->bit_depth, sizeof(U8), 1, all);
    fwrite(&IHDRdata->color_type, sizeof(U8), 1, all);
    fwrite(&IHDRdata->compression, sizeof(U8), 1, all);
    fwrite(&IHDRdata->filter, sizeof(U8), 1, all);
    fwrite(&IHDRdata->interlace, sizeof(U8), 1, all);
    IHDRcrc = swap(IHDRcrc);
    fwrite(&IHDRcrc, sizeof(U32), 1, all);

    //write IDAT
    U32 IDATlength = swap(fIDATchunk->length);
    IDATcrc = swap(IDATcrc);
    fwrite(&IDATlength, sizeof(U32), 1, all);
    fwrite(fIDATchunk->type, sizeof(fIDATchunk->type), 1, all);
    fwrite(fIDATchunk->p_data, IDATcomplength, 1, all);
    fwrite(&IDATcrc, sizeof(U32), 1, all);

    //write IEND
    fwrite(IEND, sizeof(IEND), 1, all);
    fclose(all);
    for(int i = 0; i < NUM_FILES; i++){
        free(chunkPTR[i]);
    }
    free(IHDRwidth);
    free(IHDRheight);
    free(fIDATchunk);
    free(fIDATdata);
    free(uIDATdata);
    free(IHDRCRC);
    free(IHDRlength);
    free(IHDRtype);
    free(IHDRdata);
    return 0;
}


typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);

int imageRecv[50] = {0};
int imageRecvCount = 0;
char *imageName[50] = {""};

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


size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

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

void * getImages(void *link){
    char *url = (char*)link;
    CURL *curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        return NULL;
    }
    while(imageRecvCount < lim){
        RECV_BUF recv_buf;
        recv_buf_init(&recv_buf, BUF_SIZE);
        CURLcode res;
        char fname[256];

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            return NULL;
        } 

        sem_wait(&sem);
        if(imageRecv[recv_buf.seq] == 0){
            if (imageRecvCount > lim){
                sem_post(&sem);
                break;
            }
            imageRecv[recv_buf.seq] = 1;
            imageRecvCount++;
            sprintf(fname, "./output_%d.png", recv_buf.seq);
            write_file(fname, recv_buf.buf, recv_buf.size);
            imageName[recv_buf.seq] = fname;   
        }
        sem_post(&sem); 
        recv_buf_cleanup(&recv_buf);
        curl_easy_reset(curl_handle);
    }
    curl_easy_cleanup(curl_handle);
    activeThreads--;
    if (activeThreads == 0){
        sem_post(&catsem);
    }
    pthread_exit(0);
}


void getImagesSingleThread(void *link){
    char *url = (char*)link;
    CURL *curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        return;
    }
    while(imageRecvCount < lim){
        RECV_BUF recv_buf;
        recv_buf_init(&recv_buf, BUF_SIZE);
        CURLcode res;
        char fname[256];

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            return;
        } 

        sem_wait(&sem);
        if(imageRecv[recv_buf.seq] == 0){
            if (imageRecvCount > lim){
                sem_post(&sem);
                break;
            }
            imageRecv[recv_buf.seq] = 1;
            imageRecvCount++;
            sprintf(fname, "./output_%d.png", recv_buf.seq);
            write_file(fname, recv_buf.buf, recv_buf.size);
            imageName[recv_buf.seq] = fname;   
        }
        sem_post(&sem); 
        recv_buf_cleanup(&recv_buf);
        curl_easy_reset(curl_handle);
    }
    curl_easy_cleanup(curl_handle);
    activeThreads--;
    if (activeThreads == 0){
        sem_post(&catsem);
    }
}

void assignVal(int num, char** argv){
    if(strcmp(argv[num],"-t") == 0){
        numThreads = atoi(argv[num+1]);
    }
    else if(strcmp(argv[num],"-n") == 0){
        if (atoi(argv[num+1])>3){
            printf("not a valid image number, keeping default of 1\n");
        }
        else{
            url[44] = *argv[num+1];
        }
    }
    else{
        printf("format invalid, using default settings\n");
        return;
    }
}

int main( int argc, char** argv ) 
{
    strcpy(url, IMG_URL); 
    numThreads = 1;
    if (argc >= 3){
        assignVal(1,argv);
    }
    if (argc == 5){
        assignVal(3,argv);
    }
    printf("%s\n",url);
    sem_init(&sem,1,1);

    printf("%s: URL is %s\n", argv[0], url);
    printf("Using %d concurrent threads\n",numThreads);
    pthread_t threadID[numThreads];
    sem_init(&catsem,1,1);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    sem_wait(&catsem);
    if (numThreads > 1){
        for (int i = 1; i<numThreads;i++){
            activeThreads++;
            pthread_create(&threadID[i],NULL,&getImages,(void *)&url);
        }
    }
    activeThreads++;
    getImagesSingleThread(&url);
    
    sem_wait(&catsem);
    catpng(51);
    if (numThreads > 1){
        for(int i = 1; i< numThreads; i++) {
            pthread_join(threadID[i], NULL);
        }
    }
    printf("Proccess Finished\n");
    
    curl_global_cleanup();
    return 0;
}