#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// ======================== MIT 6.5840 风格调试打印 ========================
#define DEBUG 1
#if DEBUG
#define DPrintf(format, ...)                                                                       \
    printf("[DEBUG] | %s():%d | " format "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define DPrintf(format, ...)
#endif
// =======================================================================

void doit(int clientfd);
void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int parse_url(char *url, char *host, char *port, char *uri);
void build__request(rio_t *client_rio, int serverfd, char *uri, char *host);
void read_client_requesthdrs(rio_t *client_rio);
void build_response(int serverfd, int clientfd);

void *thread(void *vargp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "main: %s \n", argv[0]);
        exit(1);
    }

    struct sockaddr_storage client_addr;
    socklen_t client_len;
    pthread_t tid;

    // 暴露client连接端口
    int listenfd = Open_listenfd(argv[1]);
    int *connectfdp;
    while (1) {
        client_len = sizeof(client_addr);
        connectfdp = Malloc(sizeof(int));
        *connectfdp = Accept(listenfd, (SA *)&client_addr, &client_len);
        if (pthread_create(&tid, NULL, thread, connectfdp)) {
            client_error(*connectfdp, "pthread_create error", "500", "Internal Server Error",
                         "Proxy couldn't create a thread to handle the request");
            Close(*connectfdp);
            Free(connectfdp);
        }
    }

    printf("%s", user_agent_hdr);
    return 0;
}

void *thread(void *vargp) {
    // 线程分离，线程结束后自动回收资源，不需要主线程调用pthread_join来回收资源
    pthread_detach(pthread_self());
    int clientfd = *((int *)vargp);
    Free(vargp);
    doit(clientfd);
    Close(clientfd);
    return NULL;
}

// 读取client的连接请求，连接到server端
void doit(int clientfd) {
    char buf[MAXLINE];
    char method[MAXLINE], url[MAXLINE], ver[MAXLINE]; // proxy里，uri变成了url
    rio_t client_rio;
    Rio_readinitb(&client_rio, clientfd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, ver);

    DPrintf("method:%s url:%s ver:%s \n", method, url, ver);

    if (strcasecmp(method, "GET")) {
        client_error(clientfd, method, "501", "Not Implemented",
                     "Proxy doesn't implement this method");
        return;
    }

    // 开始建立目标server的连接
    char host[MAXLINE], port[MAXLINE], uri[MAXLINE];
    if (!parse_url(url, host, port, uri)) {
        client_error(clientfd, method, "400", "Bad Request",
                     "Invalid URL format or non-HTTP protocol");
        return;
    }

    int serverfd = Open_clientfd(host, port);
    if (serverfd < 0) {
        client_error(clientfd, method, "502", "Bad Gateway",
                     "Proxy couldn't connect to the server");
        return;
    }

    // 将request line修改后发送给server端，再将其他部分全部发送给服务端
    build__request(&client_rio, serverfd, uri, host);
    read_client_requesthdrs(&client_rio);

    // 读取server端的响应，并返回给client端
    build_response(serverfd, clientfd);
    Close(serverfd);
    DPrintf("close");
}

// 从server端读取响应，并发送给client端
void build_response(int serverfd, int clientfd) {
    rio_t server_rio;
    char buf[MAXLINE];
    ssize_t n;
    Rio_readinitb(&server_rio, serverfd);
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n); // 用实际读取的字节数n，不是strlen
    }
}

// 将client的剩余请求头读完
void read_client_requesthdrs(rio_t *client_rio) {
    char buf[MAXLINE];
    Rio_readlineb(client_rio, buf, MAXLINE);
    // 假如读的时候出问题了，是不是一直停不下来了？
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(client_rio, buf, MAXLINE);
        DPrintf("read hdrs:%s", buf);
    }
}

// 给server端构建request line和hdrs，并发送给server端
void build__request(rio_t *client_rio, int serverfd, char *uri, char *host) {
    char buf[MAXLINE];
    sprintf(buf, "GET %s HTTP/1.0\r\n", uri);
    Rio_writen(serverfd, buf, strlen(buf));
    DPrintf("send line:%s", buf);

    sprintf(buf, "Host: %s\r\n", host);
    Rio_writen(serverfd, buf, strlen(buf));
    DPrintf("send hdrs:%s", buf);

    sprintf(buf, "%s", user_agent_hdr);
    Rio_writen(serverfd, buf, strlen(buf));
    DPrintf("send hdrs:%s", buf);

    sprintf(buf, "Connection: close\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
    DPrintf("send hdrs:%s", buf);

    sprintf(buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
    DPrintf("send hdrs:%s", buf);

    sprintf(buf, "\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
}

// 将url拆分成需要的参数
int parse_url(char *url, char *host, char *port, char *uri) {
    if (!strstr(url, "http://"))
        return 0;

    char *p = url + 7;
    char *port_start = strchr(p, ':');
    char *uri_start = strchr(p, '/');

    if (port_start != NULL)
        *port_start = '\0';

    if (uri_start != NULL)
        *uri_start = '\0';

    strcpy(host, p);

    if (port_start != NULL)
        strcpy(port, port_start + 1);
    else
        strcpy(port, "80");

    strcpy(uri, "/");
    if (uri_start != NULL)
        strcpy(uri + 1, uri_start + 1);

    DPrintf("host:%s port:%s uri:%s \n", host, port, uri);

    return 1;
}

void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The CSAPP Proxy</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
