#include <sys/socket.h>
#include <cstring>
#include <string>
#include <stdexcept>
#include "config.h"
#include "log.h"
#include "rtsp.h"

static std::string realm("");
static std::string nonce("");
static std::string username("admin");
static std::string password("jns12345");
static char url[255] = {0};

int get_response_code(const std::string& response)
{
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	indexL = response.find("\r\n");
	if(indexL == std::string::npos)
		return -1;
	std::string status_line("");
	try{
		status_line = response.substr(0, indexL);
	}catch(const std::out_of_range& oor)
	{
		log_debug("%s: out of range exception, what= %s\n", __func__, oor.what());
		return -2;
	}catch(const std::bad_alloc& ba)
	{
		log_debug("%s: bac alloc exception, what= %s\n", __func__, ba.what());
		return -3;
	}	
	
	indexL = status_line.find(' ', 0);
	if(indexL == std::string::npos)
		return -4;
	indexR = status_line.find(' ', indexL + 1);
	if(indexR == std::string::npos)
		return -5;
		
	std::string str_code("");	
	int code = 0;
	try{
		str_code = status_line.substr(indexL +1, indexR - indexL -1);
		code = std::stoi(str_code);
	}catch(const std::invalid_argument& ia)
	{
		return -6;
	}catch(const std::out_of_range& oor)
	{
		return -7;
	}catch(const std::bad_alloc& ba)
	{
		return -8;
	}	
	return code;
}

std::string get_header_value(const std::string& header, const char* key)
{
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	indexL = header.find(key);
	if(indexL == std::string::npos)
		return "";
	indexL = header.find(":", indexL);
	if(indexL == std::string::npos)
		return "";
	indexR = header.find("\r\n", indexL);
	if(indexR == std::string::npos)
		return "";
	std::string sub("");
	try{
		sub = header.substr(indexL + 1, indexR - indexL -1);
		sub.erase(0, sub.find_first_not_of(' '));
		sub.erase(sub.find_last_not_of(' ') + 1);
	}catch(const std::bad_alloc& ba)
	{
		log_error("bad alloc exception\n");
		return "";
	}catch(const std::out_of_range& oor)
	{
		log_error("out of range exception\n");
		return "";
	}	
	return sub;
}

std::string get_string_value(const std::string& str, const char* key)
{
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	std::string key_name(key);
	key_name.append("=\"");
	indexL = str.find(key_name);
	if(indexL == std::string::npos)
		return "";
	indexL = str.find("=\"", indexL);
	if(indexL == std::string::npos)
		return "";	
	indexR = str.find("\"", indexL + 2);
	if(indexR == std::string::npos)
		return "";
	std::string sub("");
	try{
		sub = str.substr(indexL + 2, indexR - indexL -2);
//		sub.erase(0, sub.find_first_not_of(' '));
//		sub.erase(sub.find_last_not_of(' ') + 1);
	}catch(const std::bad_alloc& ba)
	{
		log_error("bad alloc exception\n");
		return "";
	}catch(const std::out_of_range& oor)
	{
		log_error("out of range exception\n");
		return "";
	}	
	return sub;
}

std::string get_header(const char* buf)
{
	const char* header_end = strstr(buf, "\r\n\r\n");
	if( header_end == nullptr)
	{
		log_debug("header end flag does not exist\n");
		return "";
	}
	//header_end + 4; //算上\r\n\r\n	
	std::string header(buf, static_cast<std::size_t>(header_end + 4 - buf));
	return header;
}

std::string get_header_line(const std::string& header, const char* name)
{
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	indexL = header.find(name, 0);
	if(indexL == std::string::npos)
		return "";
	indexR = header.find("\r\n", indexL);
	if(indexR == std::string::npos)
		return "";
	std::string line("");
	try{
		line = header.substr(indexL, indexR - indexL); //不含\r\n
	}catch(const std::out_of_range& oor)
	{
		log_debug("%s: out of range exception, what= %s\n", __func__, oor.what());
		return "";
	}catch(const std::bad_alloc& ba)
	{
		log_debug("%s: bad alloc exception, what= %s\n", __func__, ba.what());
		return "";
	}
	return line;
}


std::string get_body(const char* buf)
{
	const char* header_end = strstr(buf, "\r\n\r\n");
	if( header_end == nullptr)
	{
		log_debug("header end flag does not exist\n");
		return "";
	}
	//header_end + 4; //算上\r\n\r\n	
	std::string body(header_end + 4);
	return body;
}

int rtsp_send_recv(
	int sockfd, 
	const char* send_buf, 
	int send_len, 
	char * recv_buf, 
	int recv_len)
{
	int send_num = 0;
	//循环发送直到所有数据发送出去	
	while(true)
	{
		int num = send(sockfd, send_buf + send_num, send_len - send_num, 0);
		if(num < 0)
		{
			log_error("send cmd failed\n");
			return -1;
		}
		send_num += num;
		if(send_num >= send_len)
			break;
	}
	//循环接收数据，直到全部接收完成
	int recv_num = 0;
	while(true)
	{
		int num = recv(sockfd, recv_buf + recv_num, recv_len - 1 - recv_num, 0);
		if(num < 0)
		{
			log_error("recv data failed\n");
			return -2;
		}else if(num == 0)
		{
			log_error("socket has been closed\n");
			return -3;
		}
		recv_num += num;
		recv_buf[recv_num] = 0;
		
		if(recv_num >= (recv_len - 1))
			return -4;
		//判断header部分是否接收完成
		char* header_end = strstr(recv_buf, "\r\n\r\n");
		if( header_end == nullptr)
		{
			log_debug("data is not enough, continue recv\n");
			continue;
		}
		header_end + 4; //算上\r\n\r\n
		//没有Content-Length字段，表明数据接收完成
		if(strstr(recv_buf, "Content-Length") == nullptr)
		{
			log_debug("%s: Recv complete, no body part\n", __func__);
			break;
		}
		//有Content-Length字段，继续判断body部分是否接收完成
		std::string header(recv_buf, static_cast<std::size_t>(header_end - recv_buf));
		//header.clear();
		//header.append(recv_buf, static_cast<std::size_t>(header_end - recv_buf));
		std::string str_content_length = get_header_value(header, "Content-Length");
		if(str_content_length.empty())
		{
			log_error("Get Content-Length from header failed\n");
			return -5;
		}
		
		std::size_t content_length = 0;
		try{
			content_length = static_cast<std::size_t> (std::stoul(str_content_length));
		}catch(const std::invalid_argument& ia)
		{	
			log_error("%s: invalid argument exception, what= %s\n", __func__, ia.what());
			return -6;
		}catch(const std::out_of_range& oor)
		{
			log_error("%s: ot of range exception, what= %s\n", __func__, oor.what());
			return -7;
		}
		//body.clear();
		//body.append(header_end);
		std::string body(header_end);
		if(body.length() >= content_length)
		{
			log_debug("%s: Recv complete, include body part\n", __func__);
			break;
		}		
	}	
	return recv_num;
}

void rtsp_options(int sockfd)
{
	char cmd[1024];
	char buf[2048];
	int num = 0;
	snprintf(cmd, sizeof(cmd),
		"OPTIONS %s RTSP/1.0\r\n"\
		"CSep: 2\r\n"\
		"User-Agent: Linux program\r\n\r\n",
		url);
		
	rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	log_debug("%s\n", buf);	
}

void rtsp_describe(int sockfd)
{
	char cmd[1024];
	char buf[2048];
	int num = 0;
	snprintf(cmd, sizeof(cmd),
		"DESCRIBE %s RTSP/1.0\r\n"\
		"CSep: 3\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		url);
		
	rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	log_debug("%s\n", buf);	
	
	std::string response(buf);
	int code = get_response_code(response);
	log_debug("%s: response code is %d\n", __func__, code);
	if(code != 401)
		return;
	std::string authenticate = get_header_line(response, "WWW-Authenticate");
	if(authenticate.empty())
		return;
	log_debug("authenticate= %s\n", authenticate.c_str());
	nonce = get_string_value(authenticate, "nonce");
	if(nonce.empty())
		return;
	log_debug("nonce= %s\n", nonce.c_str());
	
	realm = get_string_value(authenticate, "realm");
	if(realm.empty())
		return;
	log_debug("realm= %s\n", realm.c_str());
	
}

void rtsp_describe_authorized(int sockfd)
{
	char cmd[1024];
	char buf[2048];
	int num = 0;
	snprintf(cmd, sizeof(cmd),
		"DESCRIBE %s RTSP/1.0\r\n"\
		"CSep: 3\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		url);
		
	rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	log_debug("%s\n", buf);	
}

void rtsp(int sockfd)
{
	
	snprintf(url, sizeof(url), "rtsp://%s:%d%s", 
				SERVER_ADDR, SERVER_PORT, VIDEO_URL);
	//rtsp_options(sockfd);
   rtsp_describe(sockfd);
}

