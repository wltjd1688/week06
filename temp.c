#include <stdio.h>
#include "csapp.h"



/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// 양방향으로 캐쉬를 구현해볼것이다.
typedef struct Node_t{
    char uri[MAXLINE];
    char response_data[MAX_OBJECT_SIZE];
    size_t data_size;
    struct Node_t *next;
    struct Node_t *prev;
} Node_t;

typedef struct doublyList{
    struct Node_t *head;
    struct Node_t *tail;
    size_t total_data_size;
}mylist;

Node_t* createNode(char* uri, char* response_data, size_t data_size){
    if (data_size > MAX_OBJECT_SIZE)
        return NULL;
    Node_t* new_node = (Node_t*)malloc(sizeof(Node_t));
    if (new_node==NULL){
        fprintf(stderr,"메모리 할당 오류\n");
        return NULL;
    }
    strcpy(new_node->uri, uri);
    memcpy(new_node->response_data,&response_data,data_size);
    new_node->data_size = data_size;
    new_node->prev = NULL;
    new_node->next = NULL;
    return new_node;
}

// ====================== 함수 선언부 =======================================
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *host_uri, char* server_port);

void *thread(void *vargp);
// =======3번용 함수=================
void initialize_list(mylist* list);
void prepend(mylist* list, char* uri, char *response_data, size_t data_size);
void free_list(mylist* list, size_t data_size, int data);
void mv_pre(mylist* list, Node_t* node);
Node_t* find_node(mylist* list, const char* uri);
// =======================================================================

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static mylist list;

int main(int argc, char **argv) {
  //----------------------------------------------------------------------------------------------------------
  // int listenfd, connfd;
  // int clientlen;
  // char hostname[MAXLINE], port[MAXLINE];
  // struct sockaddr_storage clientaddr;
  //----------------------------------------------------------------------------------------------------------
  int listenfd, *connfdp;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2) {                                                                    // argc는 argument count로 우리가 이 프록시 파일을 실행할때 쓴 단어의 갯수를 저장해준다. "./proxy", "<port>"로 2개를 써야한다.
    fprintf(stderr, "usage: %s <port>\n", argv[0]);                                   // 아ㅋㅋ 포트없으면 못쓴다고
    exit(1);                                                                          // 강종
  }
  initialize_list(&list);
  listenfd = Open_listenfd(argv[1]);                                                  // 리스닝소켓을 만들기 위해서 ./proxy입력시에 받은 포트번호를 통해서 열어준다.
  while (1) {
    //---------------문제 1번 ------------------------------------------------------------------------------------
    // clientlen = sizeof(clientaddr);                                                   //클라이언트 길이 

    // connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                         //커넥트fd로 쓸걸 받아온다.

    // Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);   // 클라이언트 주소랑 호스트, 포트를 문자열로 바꿔준다.
    // printf("Accepted connection from (%s, %s)\n", hostname, port);

    // doit(connfd);                                                                     // doit함수 실행
    // Close(connfd);                                                                    // doit 다 실행한 후에는 connfd를 열어둘 필요가 없으므로 닫아준다.
    //---------------------------------------------------------------------------------------------------------
    clientlen = sizeof(clientaddr);                                                   // 클라이언트 길이 
    connfdp = Malloc(sizeof(int));                                                    // 파일 디스크립터가 int형이므로 그 size만큼을 항당해준다.
    *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);                      // 파일 디스크립터를 할당 받는다.
    Pthread_create(&tid, NULL, thread, connfdp);                                      // tid에 새롭게 만들어질 쓰레드의 ID를 주고 thread함수에 connfdp를 넣고 실행시킨다.
  }                                                                                   // malloc을 통해 스레드끼리 겹치는걸 방지한다.
  return 0;
}

/*Thread routine*/
void * thread(void *vargp){
  int connfp = *((int *)vargp);                                                       // 현재 void로 할당되어 있는 vargp형을 int형으로 형변환을 해당 포인터가 가리키는 값을 읽어온다.
  Pthread_detach(pthread_self());                                                     // 방금 만든 스레드의 id를 읽어서 부모 스레드로부터 동립을 시켜준다.
                                  // detach를 쓰는 이유는 이렇게 분리를 했을 때 부모 스레드와 관련없이 혼자서 돌아갈 수 있기 때문이며, 이후 함수가 끝나게 된다면 자동적으로 자식스레드에 대한 자원들을 회수할 수 있게 되기 때문이다.
  Free(vargp);                                                                        // vargp는 할일이 끝났으므로 free해준다.
  doit(connfp);                                                                       // connfp를 doit 시킨다.
  Close(connfp);                                                                      // connfp가 끝난다면 Close시킨다.
  return NULL;
}

void doit(int fd) {
  int is_static;  
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], host_uri[MAXLINE], host_port[MAXLINE];

  rio_t request_rio, response_rio;                                                    // rio를 두 개 쓸 필요없이 하나 끝나면 닫고 다시 열고 하기에 하나만 써도 된다.
                                                                                      
  Rio_readinitb(&request_rio, fd);                                                    // request_rio를 fd와 연결하고 읽기전에 커서를 제일 처음으로 옮기는 초기화 작업을 한다.
  Rio_readlineb(&request_rio, buf, MAXLINE);                                          // buf에 rio를 MAXLINE만큼 일어서 보낸다.

  printf("Request headrs :\n");
  printf("%s", buf);                                                                  // buf를 출력해본다.
  sscanf(buf, "%s %s %s", method, uri, version);                                      // buf에는 클라이언트에서 보낸 요청이 들어오게된다. 그걸 method, uri, version을 기준으로 
                                                                                      // 스페이스간격만큼 떨어져있기에 각각의 변수에 넣을 수 있게된다.
                                                                                      
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {                      // 말로하면 method에 "GET"이나 "HEAD"가 없을 때 오류를 띄운다인데, 조건문에서는 참일때 0(False)이므로 나중에 함수 찾아서 손으로 써보는게 더 도움될듯
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  parse_uri(uri, hostname, host_uri, host_port);                                      // parse_uri함수를 통해 uri를 hostname, host_uri, host_port를 나눠준다.
  printf("호스트 네임 : %s\n", hostname);
  printf("호스트 uri: %s\n", host_uri);
  printf("서버포트: %s\n", host_port);

  int clientfd = Open_clientfd(hostname, host_port);                                  // 서버로 보내야 하는 fd를 연다. clientfd로 지은 이유는 서버입장에서 proxy가 클라이기 때문이다.

  Node_t* node;
  if ((node = find_node(&list, uri)) != NULL){
    Rio_writen(clientfd,node->response_data,node->data_size);
    Close(clientfd);
    mv_pre(&list,node);
    return;
  }

  char request_buf[MAXBUF], response_buf[MAXBUF];  

  //-----------------------------------헤더---------------------
  sprintf(request_buf, "%s %s %s\r\n", method, host_uri, version);
  sprintf(request_buf, "%sUser-Agent:%s\r\n", request_buf, user_agent_hdr);
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);
  sprintf(request_buf, "%sProxy-Connection: close\r\n\r\n", request_buf);
  //----------------------------------------------------------
  printf("%s\n",request_buf);                                                         // request_buf 출력
  Rio_writen(clientfd, request_buf, strlen(request_buf));                             // clientfd에 request_buf를 입력해준다.
  
  //캐시에 있는지 확인하는 함수


  // Rio_readlineb 사용
  Rio_readinitb(&response_rio, clientfd);                                             // clientfd를 rio와 연결 후 읽기전 초기화

  // Rio_readlineb 사용
  int n;
  while ((n = Rio_readlineb(&response_rio, response_buf, MAXLINE)) > 0) {             // response_rio를 MAXLINE만큼 읽어 response_buf에 저장할때 정보가 없다면 넘김
    Rio_writen(fd, response_buf, n);                                                  // 정보가 있다면 fd 에 적어서 보낸다.
    if (list.total_data_size >= MAX_CACHE_SIZE){
      free_list(&list, sizeof(response_buf),0);
    }
    prepend(&list, uri, response_buf, strlen(response_buf));
  }
  Close(clientfd);                                                                    // 다 읽었으니 clientfd를 닫아준다.
}

void parse_uri(char *uri, char *hostname, char *host_uri, char* server_port) {
  strcpy(server_port, "80");          // server_port는 http기본이 80이기에 uri에 없다면 80을 넣어줘야한다.
  char* tmp[MAXLINE];             
  sscanf(uri, "http://%s", tmp);      // uri에서 tmp에 가공할 부분만 저장해준다.(ex. www.cmu.edu/hub)
                                      // + 다음과 같이 부를것이다. hostname: www.cmu.edu, hosturi: /hub
  char* p = index(tmp, '/');          // 위에서 말한거처럼 분할하기 위해서 /의 위치를 p에 저장한다.
  *p = '\0';                          // p를 참조하여 "/"위치에 "\0"을 넣어준다.
                                      // + "\0"은 왼쪽에서 배열을 읽을 때 여기까지만 읽어주세요 라는 의미가 된다.
  strcpy(hostname, tmp);              // 위에서 "/"에 "\0"를 넣었기에 strcpy는 hostname에 www.cmu.edu\0까지 저장된다. \0앞까지 읽기에 붙여도 상관없다.
  
  char* q = strchr(hostname, ':');    // index와 같은 함수로 hostname에서 포트번호를 입력하는지 확인한다. 만약 없다면 server_port에 저장된 80을 사용한다.
  if (q != NULL) {                    // q가 NULL이 아닐때 
    printf("포트 있음\n");              // hostname이 23.134.233.124:5000 이라고 생각하면 좋다.
    *q = '\0';                        // q가 ":"를 가리키므로 이를 "\0"으로 바꿔준다.
    strcpy(server_port,q+1);          // 이렇게 바꾼 q로 q+1은 포트번호만 들어가게 된다.
  }
  strcpy(host_uri, "/");              // host_uri는 제일 앞부분에 항상 "/"가 있기에 넣어준다. 넣을 값이 없더라도 "/"는 항상 붙여줘야한다.
  strcat(host_uri, p+1);              // 위에서 tmp를 쪼갤때 뒷부분이 host_uri가 된다.
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

void initialize_list(mylist* list){
  list->head = NULL;
  list->tail = NULL;
  list->total_data_size = 0;
}

void prepend(mylist* list, char* uri, char *response_data, size_t data_size){
    if(data_size > MAX_OBJECT_SIZE){
      printf("102400초과");
      return;
    }
    Node_t* new_node = createNode(uri, response_data, data_size);
    if (new_node == NULL){
      printf("메모리 할당 실패");
      return;
    }
    if (list->head==NULL){
        list->head = new_node;
        list->tail = new_node;
    } else {
        new_node->next = list -> head;
        list -> head -> prev = new_node;
        list->head = new_node;
    }
};

void mv_pre(mylist* list, Node_t* node){
    if (node == list->head)
        return;

    if (node->prev != NULL)
        node->prev->next = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;

    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL)
        list->head->prev = node;

    list->head = node;
    if (list->tail == node)
        list->tail = node->next;
}

void free_list(mylist *list, size_t data_size, int data) {
    Node_t *current = list->tail;

    if (data == 0) {
        // If data is 0, delete data_size amount of data
        while (current != NULL && data_size > 0) {
            size_t current_size = current->data_size;

            if (current->data_size >= data_size) {
                current->data_size -= data_size;
                data_size = 0;
            } else {
                data_size -= current_size;
            }

            Node_t *temp = current;
            current = current->prev;
            free(temp);
        }
    } else if (data == 1) {
        // If data is 1, delete all data
        while (current != NULL) {
            Node_t *temp = current;
            current = current->prev;
            free(temp);
        }
    } else {
        printf("Invalid data value. Please use 0 or 1.\n");
    }
}

Node_t* find_node(mylist* list, const char* uri){
  Node_t* current = list->head;
  while (current != NULL){
    if (strcmp(current->uri, uri)==0){
      return current;
    }
    current = current->next;
  }
  return NULL;
}