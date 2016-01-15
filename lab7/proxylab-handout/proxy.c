
/*
 * Name: Haoyang Yuan
 * Andrew id: haoyangy
 *
 * README: 
 * This program is a tiny proxy program. It read requests for http service
 * And the it parse the request to change some headers. Resend the request to 
 * reach the http resources, and transfer to the client.
 *
 * Function: 
 * This program has met the requirement 1&2&4
 * To make a http proxy and to implement concurrency
 * It can also work in the 4 required realpages
 *
 * Note: Getaddrinfo function from csapp.c cannot handle wrong url socket
 *
 * Usage:
 * Please include csapp.h and csapp.c files when compile.
 */
#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* modular functions */
void* proxy(void *vargp);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char* port,char* halfuri);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
void pass_request(rio_t* rio,int clientfd,char* host, char* halfuri);
void get_response(rio_t* rio2,int fd);

/* You won't lose style points for including this long line in your code */
/* The default headers, to change the original one */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *pro_hdr = "Proxy-Connection: close\r\n";



/* Main funcion, open listening port, and reproduce thread*/
int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    
    /* Ignore the PIPE signal */
    Signal(SIGPIPE, SIG_IGN);
    
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    /*Start listenning*/
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        
        /*Accept new connect*/
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        /*Create a new thread*/
        Pthread_create(&tid,NULL,proxy,connfdp);
    }
}

/* Thread function to process each thread and proxy work */
/* This function can parse the user request and forward request*/
/* And then it can send back the response from real server     */
void* proxy(void *vargp)
{
    /* process the thread*/
    int fd = *((int *)vargp);
    Pthread_detach(pthread_self());
    
    
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE],port[MAXLINE],halfuri[MAXLINE];
  
    rio_t rio,rio2;
    Free(vargp);
    
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)){
        Close(fd);
        return NULL;
    }
    printf("%s", buf);
    /* Parse the first request and retrivel url message*/
    sscanf(buf, "%s %s %s", method, uri, version);
    
    /* Check the version if it's legal */
    if (!strstr(version, "HTTP")) {
        clienterror(fd, version, "505", "Not http protocol",
                    "This is not an http protocol");
        Close(fd);
        return NULL;
    }
    
    /* Check the url to see if it's http header*/
    if (!strstr(uri, "http://")) {
        clienterror(fd, uri, "504", "Not http protocol",
                    "This is not an http protocol");
        
        Close(fd);
        return NULL;
    }
    
    /* Check the mehod to see if its GET*/
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        Close(fd);
        return NULL;
    }
    
    /* Parse URI from GET request */
    if(!parse_uri(uri, host,port,halfuri)){
        clienterror(fd, uri, "502", "Dynamic uri",
                    "doesn't implement dynamic process");
        Close(fd);
        return NULL;
    }
    
    /* Open new coonection to real server*/
    int clientfd = Open_clientfd(host, port);
    if(clientfd<0){
        clienterror(fd, uri, "503", "Cannot open host",
                    "Host or port is wrong, cannot open it");
        Close(fd);
        return NULL;
    }
    
    /* Initialize file descriptor*/
    Rio_readinitb(&rio2, clientfd);
    
    /* Generate new headers from the original one*/
    pass_request(&rio, clientfd,host,halfuri);
    
    /* Read and transfer the responses*/
    get_response(&rio2,fd);
    
    /* Close coonection */
    Close(clientfd);
    Close(fd);
    return NULL;
}


/* Function get and transfer response message to client*/
/* It reads the response message from io buffer*/
/* And writes it to the file descriptor of client*/
void get_response(rio_t* rio2,int fd){
    char one[MAXLINE];
    char *buf;
    char stringlen[MAXLINE];
    long length=-1;
    int linesize;
    
    /* Indication */
    printf("\n");
    printf("The coming content are respnses headers:\n");
    printf("\n");
    
    /* Iterate io buffer with the server to get reponse headers*/
    while(Rio_readlineb(rio2, one, MAXLINE) > 2) {
        
        /* retrieve the length content */
        if (strstr(one, "Content-Length:")) {
            strcpy(stringlen, strstr(one, " "));
            length = atol(stringlen);
            printf("length = %ld\n",length);
        }
        
        /* Write the reponse line to the client*/
        Rio_writen(fd, one, strlen(one));
        printf("%s", one);
    }
    /* Add ending indication*/
    Rio_writen(fd, "\r\n", 2);
    
    /* Get response body content and trnasfer*/
    if(length==-1){
        /* No explict length, read line by line*/
        while ((linesize = Rio_readnb(rio2, one, MAXLINE)) > 0)
            Rio_writen(fd, one, linesize);
    }else{
        /* Explicit length, read one time for all */
        buf = Malloc(length*sizeof(char));
        Rio_readnb(rio2, buf, length);/*get data from server*/
        Rio_writen(fd, buf, length);/*send data to client*/
        Free(buf);
    }
}


/* Function to generate the forward header */
/* It reads the header from client line by line */
/* Forward the new header to the server*/
void pass_request(rio_t* rio,int clientfd,char* host, char* halfuri){

    char one[MAXLINE];
    /* Set some default header values*/
    sprintf(one,"GET %s HTTP/1.0\r\n",halfuri);
    Rio_writen(clientfd,one, strlen(one));
    printf("%s",one);
    sprintf(one,"Host: %s\r\n",host);
    Rio_writen(clientfd,one, strlen(one));
    printf("%s",one);
    strcpy(one,user_agent_hdr);
    Rio_writen(clientfd,one, strlen(one));
    printf("%s",one);
    strcpy(one,conn_hdr);
    Rio_writen(clientfd,one, strlen(one));
    printf("%s",one);
    strcpy(one,pro_hdr);
    Rio_writen(clientfd,one, strlen(one));
    printf("%s",one);
    
    /* Read extra header lines and ignore the default ones*/
    while (Rio_readlineb(rio, one, MAXLINE) > 2)
    {
        if (strstr(one, "Proxy-Connection"))
            continue;
        else if (strstr(one, "Connection"))
            continue;
        else if(strstr(one,"User-Agent"))
            continue;
        else if(strstr(one,"Host"))
            continue;
        
        /* Send new header to the server */
        Rio_writen(clientfd, one, strlen(one));
        printf("%s",one);
    }
    /* Send ending indication */
    Rio_writen(clientfd, "\r\n", 2);
}



/* Function to properly parse given url */
/* And then get the accurate host,port,and relativePath*/
int parse_uri(char *uri, char *host, char* port,char* halfuri)
{
    
    char *ptr = uri;
    char *ind = uri;
    char *ind2 = uri;
    
    //default port
    strcpy(port, "80");
    
    if (!strstr(uri, "cgi-bin")) {  /* Static content */
        
        //retrieve host
        if((ind = strstr(ptr,"//"))){
            ptr = ind + 2;
        }
        
        //check if there is explicit port
        ind = index(ptr,':');
        if(ind){
            //check if there is relativePath after the port
            ind2 = index(ptr,'/');
            if(ind2>ind){
                *ind = '\0';
                *ind2 = '\0';
                strcpy(host,ptr);
                strcpy(port,ind+1);
                *ind2 = '/';
                strcpy(halfuri, ind2);
                
            }
            else{
                *ind = '\0';
                strcpy(host,ptr);
                strcpy(port,ind+1);
                strcpy(halfuri,"/");
            }
        }
        // use default port
        else{
            ind2 = index(ptr, '/');
            //thre is no relativePath
            if(ind2==NULL){
                strcpy(halfuri,"/");
                strcpy(host, ptr);
            }
            //there is relativePath
            else{
                *ind2 = '\0';
                strcpy(host, ptr);
                
                *ind2 = '/';
                strcpy(halfuri, ind2);
                
            }
        }
        printf("Connect check:%s,%s,%s\n", host,port,halfuri);
        return 1;
    }
    else {
        /* Dynamic content */
        return 0;
    }
}


 /* Error out put for the user */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);
    
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
