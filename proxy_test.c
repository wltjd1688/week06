#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *host_uri, char* server_port);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  printf("~~~\n");
  int listenfd, connfd;
  int clientlen;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); 

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    printf("커넥션 연결 : %d\n", connfd);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); 
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(connfd);  
    Close(connfd); 
  }
  return 0;
}

void doit(int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], host_uri[MAXLINE], host_port[MAXLINE];

  rio_t request_rio, response_rio;

  Rio_readinitb(&request_rio, fd);

  Rio_readlineb(&request_rio, buf, MAXLINE);

  printf("Request headrs :\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  parse_uri(uri, hostname, host_uri, host_port);
  printf("호스트 네임 : %s\n", hostname);
  printf("호스트 uri: %s\n", host_uri);
  printf("서버포트: %s\n", host_port);

  int clientfd = Open_clientfd(hostname, host_port);

  char request_buf[MAXBUF], response_buf[MAXBUF];
  int content_length;

  sprintf(request_buf, "%s %s %s\r\n", method, host_uri, version);
  sprintf(request_buf, "%sUser-Agent:%s", request_buf, user_agent_hdr);
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);
  sprintf(request_buf, "%sProxy-Connection: close\r\n\r\n", request_buf);
  printf("%s\n",request_buf);
  Rio_writen(clientfd, request_buf, strlen(request_buf));

  // Rio_readlineb 사용
  Rio_readinitb(&response_rio, clientfd);

  // Rio_readlineb 사용
  int n;
  while ((n = Rio_readlineb(&response_rio, response_buf, MAXLINE)) > 0) {
    Rio_writen(fd, response_buf, n);
  }


  Close(clientfd);
}

void parse_uri(char *uri, char *hostname, char *host_uri, char* server_port) {
  strcpy(server_port, "80");
  char* tmp[MAXLINE];
  sscanf(uri, "http://%s", tmp);

  char* p = index(tmp, '/');
  *p = '\0';
  
  strcpy(hostname, tmp);
  
  char* q = strchr(hostname, ':');
  if (q != NULL) {
    printf("포트 있음\n");
    *q = '\0';
    strcpy(server_port,q+1);
  }
  strcpy(host_uri, "/");
  strcat(host_uri, p+1);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}