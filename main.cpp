#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <cstring>
//#include "config.h"
#include "log.h"
#include "crtsp.h"
#include "base64.h"

#include "ffm.h"
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

int main(int argc, char* argv[])
{		
 	crtsp rtsp("admin", "jns87250605", "192.168.108.17", 554);
 	if(rtsp.start() < 0)
 		return 0; 	
 	
// 	while(true)
 	{
 		sleep(5);
 	}   
 	rtsp.stop();
// 	while(true)
 	{
 		sleep(1);
 	} 
 	ffm();
	return 0;
}
