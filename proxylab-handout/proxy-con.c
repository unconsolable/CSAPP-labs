#include "csapp.h"
#include "fcntl.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void* doit(void* fdarg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        pthread_t tid;
        pthread_detach(tid);
        pthread_create(&tid, NULL, doit, (void *)connfd);
    }
    return 0;
}

void* doit(void *fdarg)
{
    int fd = (int)fdarg;
    char buf[MAXLINE], method[20], url[1024], version[20];
    char requests[MAXLINE];
    rio_t rio;

    /* Read request line */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return NULL;
    /* URI is absolute URL */
    sscanf(buf, "%s %s %s", method, url, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return NULL;
    }
    /* Parse absolute URL */
    char *urlPos = strstr(url, "http://");
    if (!urlPos) {
        clienterror(fd, method, "400", "Bad Request", "No 'http://' on request line");
        return NULL;
    }
    urlPos += 7;
    /* Parse URL, port, uri */
    char *urlDelim = strstr(urlPos, "/");
    char *portDelim = strstr(urlPos, ":");
    char *hostname = urlPos, *port = NULL, *uri;
    if (portDelim && (!urlDelim || portDelim < urlDelim)) {
        *portDelim = '\0';
        port = portDelim + 1;
    }
    if (!urlDelim) {
        uri = strdup("/");
    } else {
        uri = strdup(urlDelim);
    }
    /* Make port substring */
    if (urlDelim) {
        *urlDelim = '\0';
    }
    int clientfd;
    /* Connect to the server */
    if (!port) {
        /* Default HTTP port */
        clientfd = Open_clientfd(hostname, "80");
    } else {
        /* Specific HTTP port */
        clientfd = Open_clientfd(hostname, port);
    }
    sprintf(requests, "%s %s %s\r\n", method, uri, "HTTP/1.0");
    Rio_writen(clientfd, requests, strlen(requests));
    free(uri);
    if (port) {
        sprintf(requests, "Host: %s:%s\r\n", hostname, port);
        Rio_writen(clientfd, requests, strlen(requests));
    } else {
        /* default port */
        sprintf(requests, "Host: %s\r\n", hostname);
    }
    /* End of HTTP header */
    sprintf(requests, "%sConnection: close\r\nProxy-Connection: close\r\n\r\n", user_agent_hdr);
    Rio_writen(clientfd, requests, strlen(requests));
    read_requesthdrs(&rio);
    /* Send request */
    Rio_writen(clientfd, requests, strlen(requests));
    /* Read and send reply */
    int size = Rio_readn(clientfd, requests, MAXLINE);
    Rio_writen(fd, requests, size);
    while (size > 0) {
        // Maybe more...
        size = Rio_readn(clientfd, requests, MAXLINE);
        Rio_writen(fd, requests, size);
    }
    /* close connection */
    Close(fd);
    return NULL;
}

void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
	    Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}