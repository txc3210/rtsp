#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include "config.h"
#include "log.h"
#include "crtsp.h"
#include "base64.h"

#include "ffm.h"

#if RUN_BACKGROUND
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
#endif

int main(int argc, char* argv[])
{
	log_info("Program start running......\n");
#if RUN_BACKGROUND
	run_background();
#endif
	int picture_num = 0;
	while(true)
	{		
	 	crtsp rtsp("admin", "jns87250605", "192.168.108.17", 554);
	 	if(rtsp.start() < 0)
	 		return 0; 	
	 	
	 	while(!rtsp.is_work_done())
	 	{
	 		usleep(200 * 1000);
	 	}   
	 	rtsp.stop();	 
	 	picture_num++;	
	 		 	
	 	if(picture_num >= MAX_PICTURE_NUM)
	 		break;
	 	log_info("I will get a picture %d minutes later....\n", PICTURE_INTERVAL);
	 	sleep(PICTURE_INTERVAL * 60);
	 	
 	}
	return 0;
}
