#pragma once
#include <string>
#include "crtp.h"

#define VIDEO_URL		"/h264/ch1/main/av_stream"

#define BUFFER_SIZE	4096

class crtsp
{
public:
	crtsp(const char* username, 
		const char* password,
		const char* ip,
		unsigned short port);
	~crtsp();
	int start();
	int stop();
	bool is_work_done(){return rtp.is_work_done();};
	
private:
	int connect_server();
	int options();
	int describe();
	int describe_authorized();
	int setup();
	int rtp_int();
	int play();
	int teardown();
	int rtp_init();
	
	int tcp_send(const char* buf, int len);
	int tcp_recv(char* buf, int len);
	
	std::string get_header_value(const std::string& header, const char* key);
	int get_response_code(const std::string& response);
	std::string get_header_line(const std::string& header, const char* name);
	std::string get_string_value(const std::string& str, const char* key);
	std::string get_response(std::string& method);
	std::string get_body(const char* buf);
	int get_server_port(const std::string& header, unsigned short* _rtp_port, unsigned short* _rtcp_port);
	std::string get_session(std::string& header);

private:
	int sockfd;
	//int rtp_sockfd;
	std::string rtsp_ip;	
	unsigned short rtsp_port;
	unsigned short rtp_port;
	unsigned short rtcp_port;
	
	unsigned short rtp_client_port;
	unsigned short rtcp_client_port;
	
	std::string uri;
	std::string method;
	std::string realm;
	std::string nonce;
	std::string username;
	std::string password;
	std::string track_id;
	std::string session;
	
	crtp rtp;
};
