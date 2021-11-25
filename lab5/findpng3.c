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
#include <unistd.h>
#include <curl/multi.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>


#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define MAX_WAIT_MSECS 30*1000 /* Wait max. 30 seconds */
#define MAX_PNG 50

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define LOGFILE "log.txt"
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

int uniquePNGNum = 0;

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

static size_t cb(char *d, size_t n, size_t l, void *p)
{
  /* take care of the data here, ignored in this example */
  (void)d;
  (void)p;
  return n*l;
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
        fprintf(stderr, "Document not parsed successfully.\n");
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
        printf("No result\n");
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
                printf("href: %s\n", href);
                char data[256];
                
                sprintf(data, "%s", href); // must be in mutex!
                
                if(isInList(&toVisitURLList, data) == 0 && isInList(&visitedURLList, data) == 0){   
                    node_t* temp = malloc(sizeof(node_t));
                    temp->next = NULL;
                    strcpy(temp->val, data);
                    addToList(&toVisitURLList, temp);
                } 

            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
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


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

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
    ptr->seq = -1;              /* valid seq should be positive */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	    return 1;
    }
    
    free(ptr->buf);
    free(ptr);
    return 0;
}

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        //curl_global_cleanup();
        recv_buf_cleanup(ptr);
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

int append_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    char line[1] = "\n";

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "a+");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    fwrite(line, 1, 1, fp);
    return fclose(fp);
}

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

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
        fprintf(stderr, "curl_easy_init: returned NULL\n");
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

    curl_easy_setopt(curl_handle, CURLOPT_PRIVATE, ptr);

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

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
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
    // need mutex
    if(isInList(&visitedURLList, url) == 0){
        printf("ADDED EXTRA!!!\n");
        // Add to visited List
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, url);
        addToList(&visitedURLList, temp);

    }
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 
    sprintf(fname, "./output_%d.html", pid);
    
    return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    printf("PNG NUM: %d\n", uniquePNGNum);
    U8 header[8];
    memcpy(header, p_recv_buf->buf, sizeof(U8)*8);
    if(is_png(header) == 0){
        return 0;
    }
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        printf("The PNG url is: %s\n", eurl);
    }
        // need mutex
    if(isInList(&visitedURLList, eurl) == 0){
        printf("ADDED EXTRA!!!\n");
        // Add to visited List
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, eurl);
        addToList(&visitedURLList, temp);

    }
    if(isInList(&visitedPNGList, eurl) == 0){
        node_t* temp = malloc(sizeof(node_t));
        temp->next = NULL;
        strcpy(temp->val, eurl);
        addToList(&visitedPNGList, temp);
    }
    printf("END OF PNG PROC");
    uniquePNGNum++; // need mutex
    
    return 0;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
    	fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf);
    } else {
        sprintf(fname, "./output_%d", pid);
    }

    return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}

int main( int argc, char** argv ) 
{
    int cm_max = 1;
    int max_png = 5;

    char url[256];
    strcpy(url, SEED_URL); 

    if (argc != 1) {
        printf("HERE\n");
        for (int i = 1; i < argc-1; i+=2){
            if (strcmp(argv[i],"-t") == 0){
                const char *t = argv[i+1];
                int k = atoi(t);
                //cm_max = atoi(argv[i+1]);
            }
            else if (strcmp(argv[i],"-m") == 0){
                char *m = argv[i+1];
                max_png = atoi(argv[i+1]);
                if (max_png > MAX_PNG){
                    max_png = MAX_PNG;
                }
            }
            // else if (strcmp(argv[i],"-v") == 0){
            //     strcpy(LOGFILE,argv[i+1]);
            //     log = 1;
            // }
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

    // curl multi setup
    int still_running = 0;
    int msg_left = 0;
    int in_cm = 0;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURLM *cm=NULL;
    cm = curl_multi_init();
    CURLMsg *msg = NULL;

    // init recv buf array
    //bufs = malloc(sizeof(RECV_BUF) * cm_max);

    // CURL *init_curl_handle = easy_handle_init(&buf[bufIndex], url);
    // bufIndex = (bufIndex + 1) % cm_max;
    // curl_multi_add_handle(cm, init_curl_handle);

    // populate curl multi handler
    // wait until

    while(uniquePNGNum < max_png){

        //need mutex
        if(toVisitURLList.head == NULL && still_running == 0){
            printf("NULL head\n");
            break;
        }

        while(in_cm < cm_max && toVisitURLList.head != NULL){
            char initURL[256];
            strcpy(initURL, toVisitURLList.head->val);
            
            // get next url
            removeFromList(&toVisitURLList);

            // Add to visited List, need mutex
            node_t* temp = malloc(sizeof(node_t));
            temp->next = NULL;
            strcpy(temp->val, initURL);
            addToList(&visitedURLList, temp);
            printf("URL: %s \n", initURL);

            RECV_BUF* recv_buf = malloc(sizeof(RECV_BUF));
            CURL *curl_handle = easy_handle_init(recv_buf, initURL);

            if ( curl_handle == NULL ) {
                fprintf(stderr, "Curl initialization failed. Exiting...\n");
                curl_global_cleanup();
                abort();
            }

            curl_multi_add_handle(cm, curl_handle);

            in_cm++;
        }

        curl_multi_perform(cm, &still_running);

        do {
            printf("In wait: %d\n", in_cm);
            int numfds=0;
            int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
            if(res != CURLM_OK) {
                fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
                return EXIT_FAILURE;
            }
            curl_multi_perform(cm, &still_running);
        } while (in_cm == still_running);
        printf("Out of wait\n");
        while((msg = curl_multi_info_read(cm, &msg_left))){
            if(msg->msg == CURLMSG_DONE){
                printf("read message");
                CURL *eh = msg->easy_handle;

                int http_status_code = 0;
                RECV_BUF *recv_buf = NULL;

                curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &recv_buf);


                CURLcode res = msg->data.result;
                if(res == CURLE_OK){
                    printf("%lu bytes received in memory %p, seq=%d.\n", \
                        recv_buf->size, recv_buf->buf, recv_buf->seq);
                    process_data(eh, recv_buf);
                    cleanup(eh, recv_buf);  
                }else{
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                    curl_multi_remove_handle(cm, eh);
                    cleanup(eh, recv_buf);
                }
                in_cm--;
                if(uniquePNGNum >= max_png){
                    break;
                }
            }
        }

        

        // if( res != CURLE_OK) {
        //     fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        //     cleanup(curl_handle, &recv_buf);
        // } else {
        //     printf("%lu bytes received in memory %p, seq=%d.\n", \
        //         recv_buf.size, recv_buf.buf, recv_buf.seq);
        //                 /* process the download data */
        //     process_data(curl_handle, &recv_buf);

        //     /* cleaning up */
        //     cleanup(curl_handle, &recv_buf);    
        // }

        
    }

    curl_multi_cleanup(cm);
    //printList(&toVisitURLList);
    appendList(&visitedURLList, LOGFILE);
    appendList(&visitedPNGList, PNGFILE);
    freeList(&visitedURLList);
    freeList(&toVisitURLList);
    freeList(&visitedPNGList);

    return 0;
}
