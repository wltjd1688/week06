/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

//--함수선언부---------------------------------------------------------------
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char* method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char* method);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
//------------------------------------------------------------------------

int main(int argc, char **argv) 
{

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args - 처음에 make하고 ./tiny하잖아 그때 들어가는값이 
        tiny옆에 포트번호 넣어야해서 이친구가 실행하면서 받는게 총 2개이기 때문에 이렇게 확인하는거 */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);                                     // 실제로 포트번호없이 그냥 띄우면 usage: ./tiny <port> 라고 나온다
	exit(1);                                                                            // 그러고 포트번호가 없으니까 종료
    }

    listenfd = Open_listenfd(argv[1]);                                                  // open_listenfd함수로 포트번호에 해당하는 리스닝소켓을 열어줌
    while (1) {                                                                         // 서버는 클라에서 요청이 언제올지 모르니까 계속 while으로 돌려주는거(이거 안하면 딱한번 송수신하면 끝남)
	clientlen = sizeof(clientaddr);                                                     // 클라주소 길이
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept   // 이제 클라에서 요청한거 accept함수로 fd(file descript)를 받아서 송수신할 준비중
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,                   // hostname이랑 port, clientaddr을 문자열로 바꿔주는 함수
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);                  // 어떤 주소에 몇번포트로 주소를 할당해줌
	doit(connfd);                                             //line:netp:tiny:doit     // 송수신할 함수
	Close(connfd);                                            //line:netp:tiny:close    // 송수신이 끝나면 fd는 닫아야하니까 close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);                                                                        // rio변수에 fd를 연결하고 읽기전에 초기화잡업하는 함수, 책설명은 한개의 빈 버퍼를 설정하고(초기화하고), 이 버퍼를 한개의 오픈한 파일 식별자를 연결
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest                           // 이제 buf를 MAXLINE만큼 읽었을 때, 데이터가 안들어왔다면 return하고 끝낸다.
        return;
    printf("%s", buf);                                                                              // 데이터가 있다면 buf를 출력한다.
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest              // buf정보를 method, uri, version으로 나눠서 할당함
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {//line:netp:doit:beginrequesterr  // method가 "GET"인지 "HEAD" 인지 확인
        clienterror(fd, method, "501", "Not Implemented",                                           // method에 GET이랑 HEAD없으면 오류(501)
                    "Tiny does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr
    read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs           // method를 알았으니 요청사항 확인

    /* Parse URI from GET request */            
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck               // parse_uri에서 uri를 확인하고 홈페이지("/")인지 동적컨텐츠인지("/cgi-bin/adder?~")확인함
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound             // filename이 sbuf에 있다면 0 없거나 오류가 나면 -1을 반환
	clienterror(fd, filename, "404", "Not found",                                                   // 없을 때 404에러를 띄움
		    "Tiny couldn't find this file");
	return;
    }                                                    //line:netp:doit:endnotfound

    if (is_static) { /* Serve static content */                                                     // is_static이 아까 확인한건데 정적(uri에 따로 뭔가 표시된게 없다면)일때 1이다
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable          // 뭔가 확인하는데 알고싶지는 않음...;; - 요약하자면 권한확인용?
	    clienterror(fd, filename, "403", "Forbidden",                                               // 상태코드가 403인걸 봐서는 서버에 요청이 전달되었지만 권한때문에 거절되었다는 의미
			"Tiny couldn't read the file");
	    return;
	}
	serve_static(fd, filename, sbuf.st_size, method);        //line:netp:doit:servestatic           // 오류 없으면 정적콘텐츠 표시
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable        // is_static에서 동적컨텐츠가 발견되어서 0을 반환하였을 때
	    clienterror(fd, filename, "403", "Forbidden",                                               // 아까랑 같이 권한오류일때
			"Tiny couldn't run the CGI program");
	    return;
	}
	serve_dynamic(fd, filename, cgiargs, method);            //line:netp:doit:servedynamic          // 오류가 없다면 동적컨텐츠 표시
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)                                                                    // 아까봤던 함수인데 딱히 알고싶지는 않음;; - 대충 버퍼안에 있는거 확인해보고 끝까지 읽었을 때 그 값들 print하는 정도?
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs)                                             // uri를 통해 현재 요청한게 정적컨텐츠인지 동적컨텐츠인지 확인하는거
{
    char *ptr;
    printf("%s\n",uri);
    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic              // uri에 cgi_bin이 없을때(ex. uri = "/")
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi                  // cgiargs를 ""로 복사
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1             // filename에 "."로 복사
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1               // filename에 uri 붙이기
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck.               // 왜 있는지는 모르겠지만 아무튼 uri가 "/"니까 그냥 home.html을 표시함
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
        
	return 1;                                                                                       // 위에서 확인했을 때 정적이니까 1을 return
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic             // uri에 cgi_bin이 있을때(ex. uri = "/cgi_bin/adder?num1=12&num2=23")
	ptr = index(uri,'?');                           //line:netp:parseuri:beginextract               // ptr을 uri의 "?"위치시킴
	if (ptr) {                                                                                      // ptr이 0이 아니라면
	    strcpy(cgiargs, ptr+1);                                                                     // cgiargs에 ptr+1즉 값들이 들어갈 자리를 찾아서 복사하고 cgiargs에 붙여넣음(cgiargs는 num1=12&num2=23을 가리킴)
	    *ptr = '\0';                                                                                // 하고 ptr은 \0으로 만듬
	} else 
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract                // 0이면 그냥 cgiargs에 빈값을 넣음
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2             // filename에 "."을 넣음
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2               // 그 뒤에 uri를 넣고
    printf("파일이름:%s\n", filename);                                                                // 터미널에서 파일이름을 확인함 - 이거 내가 넣은건가...?
	return 0;                                                                                       // 동적이니까 0으로 return
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, char* method)                               // 이후 정적컨텐츠 보낼준비
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */                                                           //-----------여기서부터
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.1 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve                           //------------여기까지 헤더

    if (strcmp(method,"HEAD")==0)                                                                   // method가 head일때 head만 표출
        return;
    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open                               // parse_uri에서 만든 filename을 통해 송수신할 소켓 오픈
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap        // srcfd의 처음부터 filesize만큼을 읽고옴
    Close(srcfd);                       //line:netp:servestatic:close                               // mmap하면서 안에 내용을 다 읽어왔으니 socket닫기
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write                               // writen으로 fd에 srcp내용을 filesize만큼 쓰기
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap                              // mmap한다고 열어둔 메모리 닫기
    // srcp = (char *)malloc(filesize);
    // Rio_readn(srcfd,srcp,filesize);
    // Close(srcfd);
    // Rio_writen(fd,srcp,filesize);
    // free(srcp);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)                                                   // 그냥 filetype확인하는 함수, 요기에 적혀있는 친구들만 type으로 받아줌, 보통 정적콘텐츠함수에서 사용함
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".mp4"))
	strcpy(filetype, "videos/mp4");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, char* method)                             // 동적컨텐츠 보낼준비하는 함수
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */                                                        //------------- 아까처럼 여기서부터
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));                                                               //------------- 여기까지 헤더
    
    if (strcmp(method,"HEAD")==0)                                                                   // 마찬가지로 method head가 들어왔을 때 헤더만 출력하게하는거
        return;
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork                                    // Fork 함수를 통해서 현재 열여있는 리스닝 소켓의 자식을 만듬, 0일때는 자식이므로 자식이 if문을 다 할때까지 부모는 wait하라는 의미
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv                             // 대충 "QUERY_STRING"이 환경변수일때 cgiargs로 다시 변경해주는? 인데 잘 모르겠따...
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2  // Dup2는 fd에 직접적으로 스텐다드 입출력을 넣겠다는 선언 - 그렇게 되면 print를 했을 때 클라이언트에 바로 들어감
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve     // 이렇게 하면 자식프로그램에서 cgi 프로그램을 톨려줌(아까 uri에서 cgi-bin 그친구가 이거임, CGI는 Common Gateway Interface라고 씀)
    }                                                                                               // 여기서 우리는 동적컨탠츠라면 앞에서 filename에 ./cgi_bin/adder가 들어가있는걸 기억해내야한다. 즉, 여기서 adder.c를 실행시켜주는것이다.
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait                // 위에서 말한거처럼 자식프로세서가 다 끝날때까지 기다려주는것
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,                                                 // 그냥 클라에러함수, 터미널에 찍어주는용도?랑 클라이언트한테 오류띄워주는용, 문제풀때 자주보개될 화면의 출력
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */
