/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The xml example code is 
 * http://www.xmlsoft.org/tutorial/ape.html
 *
 * The paster.c code is 
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 * 
 * This software may be freely redistributed under the terms of the X11 license.
 */

/** 
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header if there is a sequence number.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */ 

// Linked List to keep track of list of URLs to visit
// Linked List to keep track of already visited URLs
// Linked List to keep track of already visited PNGs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>


#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9
#define MAXPNG 50

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define PNGFILE "png_urls.txt"

typedef unsigned char U8;
typedef unsigned int  U32;

typedef struct node {
    char val[256];
    struct node* next;
} node_t;

typedef struct linkedList {
    int size;
    struct node* head;
    struct node* tail;
} linkedList_t;

int numThreads = 0;
int uniquePNGNum = 0;
int neededPNG = 0;
char LOGFILE[256];

sem_t foundSem;
pthread_mutex_t visitedMutex;
pthread_mutex_t pngMutex;
pthread_mutex_t toVisitMutex;
pthread_mutex_t conMutex;
pthread_cond_t maxPNG;

linkedList_t toVisitURLList = {.size = 0, .head = NULL, .tail = NULL};
linkedList_t visitedURLList = {.size = 0, .head = NULL, .tail = NULL};
linkedList_t visitedPNGList = {.size = 0, .head = NULL, .tail = NULL};

void addToList(linkedList_t* list, node_t* node){
    if(list->size == 0){
        list->head = node;
    } else{
        list->tail->next = node;
    }
    list->tail = node;
    list->size++;
}

void removeFromList (linkedList_t* list){
    if(list->size == 0){
        return;
    } else if(list->size == 1){
        list->tail = NULL;
        free(list->head);
        list->head = NULL;
    } else{
        node_t* temp = list->head->next;
        free(list->head);
        list->head = temp;
    }
    list->size--;
}

int isInList(linkedList_t* list, char* find){
    node_t* cur = list->head;
    int res = 0;
    while(cur != NULL && res == 0){
        if(strcmp(cur->val, find) == 0){
            res = 1;
        }
        cur = cur->next;
    }
    return res;
}

void printList(linkedList_t* list){
    node_t* cur = list->head;
    while(cur != NULL){
        printf("Node is: %s \n", cur->val);
        cur = cur->next;
    }
}

void freeList(linkedList_t* list){
    node_t* cur = list->head;
    while(cur != NULL){
        node_t* temp = cur;
        cur = cur->next;
        free(temp);
    }
    list->head = NULL;
    list->tail = NULL;
}



int is_png(U8 *buf){
    if(buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47 && buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A){
        return 1;
    }
    return 0;
}


typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;


htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
int append_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf);


void appendList(linkedList_t* list, char* fname){
    node_t* cur = list->head;
    while(cur != NULL){
        append_file(fname, cur->val, strlen(cur->val));
        cur = cur->next;
    }
}


htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);
    
    if ( doc == NULL ) {
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{
	
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                //printf("href: %s\n", href);
                char data[256];
                
                sprintf(data, "%s", href); // must be in mutex!

                
                
                if(isInList(&toVisitURLList, data) == 0 && isInList(&visitedURLList, data) == 0){   
                    pthread_mutex_lock(&toVisitMutex);
                    node_t* temp = malloc(sizeof(node_t));
                    temp->next = NULL;
                    strcpy(temp->val, data);
                    addToList(&toVisitURLList, temp);
                    pthread_mutex_unlock(&toVisitMutex);
                } 

            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    return 0;
}
/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

#ifdef DEBUG1_
    printf("%s", p_recv);
#endif /* DEBUG1_ */
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
 
    if (p->size + realsize + 1 > p->max_size) {

        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize);
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
    ptr->seq = -1;           
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

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        recv_buf_cleanup(ptr);
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        return -1;
    }

    if (in == NULL) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        return -3; 
    }
    return fclose(fp);
}

int append_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    char line[1] = "\n";

    if (path == NULL) {
        return -1;
    }

    if (in == NULL) {
        return -1;
    }

    fp = fopen(path, "a+");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        return -3; 
    }
    fwrite(line, 1, 1, fp);
    return fclose(fp);
}

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */ 
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL; 
    pid_t pid =getpid();
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    if(isInList(&visitedURLList, url) == 0){
        pthread_mutex_lock(&visitedMutex);
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, url);
        addToList(&visitedURLList, temp);
        pthread_mutex_unlock(&visitedMutex);

    }
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 
    sprintf(fname, "./output_%d.html", pid);
    
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    U8 header[8];
    memcpy(header, p_recv_buf->buf, sizeof(U8)*8);
    if(is_png(header) == 0){
        return 0;
    }
    char *eurl = NULL;        
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    
    pthread_mutex_lock(&visitedMutex);
    if(isInList(&visitedURLList, eurl) == 0){
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, eurl);
        addToList(&visitedURLList, temp);

    }
    pthread_mutex_unlock(&visitedMutex);

    if(isInList(&visitedPNGList, eurl) == 0){
        pthread_mutex_lock(&pngMutex);
        if (uniquePNGNum == neededPNG){
            pthread_mutex_unlock(&pngMutex);
            return 1;
        }
        pthread_mutex_lock(&conMutex);
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, eurl);
        addToList(&visitedPNGList, temp);
        uniquePNGNum++;
        if (uniquePNGNum == neededPNG){
            pthread_cond_signal(&maxPNG);
        }
        pthread_mutex_unlock(&conMutex);
        pthread_mutex_unlock(&pngMutex);
    }
    return 0;
}

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    //printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	//printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf);
    }

    return 0;
}

void * crawler(void* variable){
    
    while (uniquePNGNum < neededPNG){
        sem_wait(&foundSem);
        if (neededPNG <= uniquePNGNum){
            sem_post(&foundSem);
            pthread_exit(0);
        }
        sem_post(&foundSem);
        pthread_mutex_lock(&toVisitMutex);
        //need mutex
        if(toVisitURLList.head == NULL){
            pthread_mutex_unlock(&toVisitMutex);
            if (neededPNG > uniquePNGNum){
                continue;
            }
            else{
                break;
            }
        }
        char initURL[256];
        strcpy(initURL, toVisitURLList.head->val);
        
        // get next url
        removeFromList(&toVisitURLList);
        pthread_mutex_unlock(&toVisitMutex);
        // Add to visited List, need mutex
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, initURL);
        pthread_mutex_lock(&visitedMutex);
        addToList(&visitedURLList, temp);
        pthread_mutex_unlock(&visitedMutex);

        CURL *curl_handle;
        CURLcode res;
        
        RECV_BUF recv_buf;
        curl_handle = easy_handle_init(&recv_buf, initURL);

        if ( curl_handle == NULL ) {
            curl_global_cleanup();
            abort();
        }
        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            cleanup(curl_handle, &recv_buf);
        } else {
            process_data(curl_handle, &recv_buf);

            /* cleaning up */
            cleanup(curl_handle, &recv_buf);    
        }
    }
    pthread_exit(0);
}

int main( int argc, char** argv ) 
{

    curl_global_init(CURL_GLOBAL_DEFAULT);

    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    char url[256];
    int log = 0;
    numThreads = 1;
    neededPNG = 50;
    strcpy(url,SEED_URL);
    if (argc != 1) {
        for (int i = 1; i < argc-1; i+=2){
            if (strcmp(argv[i],"-t") == 0){
                numThreads = atoi(argv[i+1]);
            }
            else if (strcmp(argv[i],"-m") == 0){
                neededPNG = atoi(argv[i+1]);
                if (neededPNG > MAXPNG){
                    neededPNG = MAXPNG;
                }
            }
            else if (strcmp(argv[i],"-v") == 0){
                strcpy(LOGFILE,argv[i+1]);
                log = 1;
            }
        }
        if (argc%2 == 0){
            strcpy(url, argv[argc-1]);
        }
    }

    node_t* temp = malloc(sizeof(node_t));
    temp->next = NULL;
    strcpy(temp->val, url);
    addToList(&toVisitURLList, temp);

    //initializing files
    char pngfile[256];
    strcpy(pngfile, PNGFILE);
    FILE *fp = NULL;
    fp = fopen(pngfile, "a");
    fclose(fp);

    sem_init(&foundSem,1,1);
    pthread_mutex_init(&visitedMutex,NULL);
    pthread_mutex_init(&pngMutex,NULL);
    pthread_mutex_init(&toVisitMutex,NULL);
    pthread_mutex_init(&conMutex,NULL);
    pthread_cond_init(&maxPNG,NULL);

    pthread_t pid[numThreads];

    for (int i = 0; i < numThreads; i++){
        pthread_create(&pid[i],NULL,&crawler,NULL);
    }
    
    pthread_mutex_lock(&conMutex);
    if (uniquePNGNum < neededPNG){
        pthread_cond_wait(&maxPNG,&conMutex);
        for (int i = 0; i < numThreads; i++){
            pthread_join(pid[i],NULL);
        }
    }
    pthread_mutex_unlock(&conMutex);
    

    //printList(&toVisitURLList);
    if (log == 1){
        appendList(&visitedURLList, LOGFILE);
    }
    appendList(&visitedPNGList, PNGFILE);

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);

    
    freeList(&visitedURLList);
    freeList(&toVisitURLList);
    freeList(&visitedPNGList);

    sem_destroy(&foundSem);
    pthread_mutex_destroy(&visitedMutex);
    pthread_mutex_destroy(&pngMutex);
    pthread_mutex_destroy(&toVisitMutex);
    pthread_mutex_destroy(&conMutex);
    pthread_cond_destroy(&maxPNG);

    xmlCleanupParser();
    curl_global_cleanup();

    return 0;
}