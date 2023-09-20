/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {                           // 환경변수"QUERY_STRING"를 찾아서 buf에 넣을껀데 QUERY_STRING이 없다면 실행한다. - tiny에서 이 환경변수를 cgiargs로 바꿔주는걸 기억해내야한다.
	p = strchr(buf, '&');                                                   // p를 buf에서 "&"에 위치시킨다.
	*p = '\0';                                                              // 이후 &를 \0으로 만든다. - 이렇게되면 tiny에서 우리가 parse_uri에서 ptr을 "숫자1&숫자2\0"으로 만들어준걸 기억해내야한다. 이제는 "숫자1\0숫자2\0"이 된다.
	strcpy(arg1, buf);                                                      // 이렇게 하면 strcpy에서 \0까지만 읽어오기 때문에 두 숫자를 구분할 수 있게된다. 여기서는 strcpy를 이용해서 arg1에 buf의 제일 앞에서 부터 읽기에 "숫자1"만 들어간다.
	strcpy(arg2, p+1);                                                      // 여기서는 arg2에 p+1이니까 \0을 넣은 다음부터가 arg2에 들어가게 된다. 숫자2
	n1 = atoi(arg1);                                                        // 이렇게 저장한 문자열을 정수로 n1에 저장한다.
	n2 = atoi(arg2);                                                        // 위와같다
    }

    /* Make the response body */                                            //----------여기서부터 클라이언트에 들어가게된다.
    sprintf(content, "Welcome to add.com: ");                               // 첫문장을 넣고
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);    // 이전 파일뒤에 다음 문장을 넣고
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",                // 또 이전파일에 다음 에 들어갈거 넣고
	    content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);                // 마지막으로 방문에 감사하다고 넣고
  
    /* Generate the HTTP response */
    printf("Connection: close\r\n");                                        // 이건 자식터미널에 찍히는거라서 우리는 볼 수 없다. 우리가 보는 터미널은 부모꺼라... 약간 자식프로세서의 헤더느낌?
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);                                                  // 요만큼을 %s를 통해서 클라이언트에게 전달한다. 이건 tiny에서 소켓에 직접적으로 입출력을 연결했기에 클라이언트가 받고 브라우저에 표시되게되는것이다.
    fflush(stdout);                                                         // 다했으니까 남아있는거 다 프린트하고 비워준다.

    exit(0);                                                                // 요로코롬하면 adder할거 끝이니까 여기서 나가고 다시 tiny로 돌아간다.
}
/* $end adder */
