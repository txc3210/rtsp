#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include "config.h"
#include "log.h"
#include "rtp.h"
#include "rtsp.h"

#define BUFFER_SIZE	4096

static struct sockaddr_in server;
static int sockfd = -1;

const unsigned char start_cmd[] = {0xce, 0xfa, 0xed, 0xfe};

int _rtp_init()
{
	struct sockaddr_in client;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		log_error("%s: socket error\n", __func__);
		return -1;
	}
	
//	unsigned short client_port = get_rtp_client_port();
	
	bzero(&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(get_rtp_client_port());
	client.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(sockfd, reinterpret_cast<struct sockaddr*>(&client), sizeof(struct sockaddr)) < 0)
	{
		log_error("%s: bind socket error\n", __func__);
		return -2;
	}
	
//	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(get_rtp_server_port());
	//inet_pton(AF_INET, "192.198.108.2",&server.sin_addr);
	server.sin_addr.s_addr = inet_addr(SERVER_ADDR);	
	
	int ret = sendto(sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -3;
	}
//	log_debug("%s: send %d bytes\n", __func__, ret);
	usleep(100000);
	
	ret = sendto(sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -4;
	}
//	log_debug("%s: send %d bytes\n", __func__, ret);
	
	std::thread th(rtp_recv, nullptr);
	th.detach();
	
	return 0;
}

void* rtp_recv(void* arg)
{
	struct sockaddr_in client;
	char buf[BUFFER_SIZE + 1];
	int ret = 0;
	log_debug("%s: rtp recv thread start....\n", __func__);
	socklen_t len = sizeof(struct sockaddr);
	while(true)
	{
		ret = recvfrom(sockfd, buf, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr*>(&client), &len);
		log_debug("%s: recv %d byte from UDP\n", __func__, ret);
		//sleep(1);
	}
	return nullptr;
}

