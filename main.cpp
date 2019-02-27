#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cstring>
#include "config.h"
#include "log.h"
#include "rtsp.h"

/*********************************************************
* 功能：收到kill消息时的处理函数,清理内存，程序退出
* 参数：sig, kill指令后的参数，如kill -3
*********************************************************/
static void handler(int sig)
{
	log_info("I got a signal %d, I'm quitting.\n",sig);	
	exit(0);
}

/**************************************
* 功能：让程序以守护进程的方式运行
**************************************/
static void run_background(void)
{
	if(daemon(1, 1) == -1)
	{
		log_error("daemon error\n");
		exit(1);
	}
	struct sigaction act;
	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if(sigaction(SIGQUIT, &act, NULL))
	{
		log_error("sigaction error\n");
		exit(0);
	}
}

int connect_server()
{	
	struct sockaddr_in server_addr;
	//初始化SOCKET
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		log_error("%s(): create socket error:%s(errno:%d)\n", __func__, strerror(errno), errno);
		return -1;
	}	
	//初始化
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
	server_addr.sin_port = htons(SERVER_PORT);//设置的端口为SERVER_PORT	

	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		log_error("connect server error:%s(errno:%d)\n", strerror(errno), errno);
		return -1;
	}
	log_debug("connect server succeed, sockfd= %d\n", sockfd);	
	return sockfd;
}

int main(int argc, char* argv[])
{
   int sockfd = connect_server();
   rtsp(sockfd);   
 	close(sockfd);
   exit(0);
}
