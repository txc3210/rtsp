#include <sys/socket.h>
#include <cstring>
#include <stdexcept>
#include "config.h"
#include "log.h"
#include "rtsp.h"
#include "rtp.h"
#include "md5.h"

static std::string method("");
static std::string realm("");
static std::string nonce("");
static std::string username("admin");
static std::string password("jns87250605");
static char uri[255] = {0};
static std::string track_id("");
static std::string session("");

static unsigned short rtp_server_port = 60000;
static unsigned short rtcp_server_port = 60001;

static unsigned short rtp_client_port = 60000;
static unsigned short rtcp_client_port = 60001;

unsigned short get_rtp_client_port()
{
	return rtp_client_port;
}

unsigned short get_rtcp_client_port()
{
	return rtcp_client_port;
}

unsigned short get_rtp_server_port()
{
	return rtp_server_port;
}

unsigned short get_rtcp_server_port()
{
	return rtcp_server_port;
}

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
		header_end += 4; //算上\r\n\r\n
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
		log_debug("content-length:%s\n", str_content_length.c_str());
		
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

std::string get_response(std::string& method)
{
	char temp[1024];
	snprintf(temp, sizeof(temp), "%s:%s:%s", 
				username.c_str(), realm.c_str(), password.c_str());
	std::string part1;
	GetMD5(temp, part1);
	
	snprintf(temp, sizeof(temp), "%s:%s", method.c_str(), uri);
	std::string part3;
	GetMD5(temp, part3);	
	
	snprintf(temp, sizeof(temp), "%s:%s:%s",
					part1.c_str(), nonce.c_str(), part3.c_str());
	std::string response;
	GetMD5(temp, response);
	
	return response;
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
		uri);
		
	rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	log_debug("%s\n", buf);	
}

void rtsp_describe(int sockfd)
{
	method="DESCRIBE";
	char cmd[1024];
	char buf[2048];
	int num = 0;
	snprintf(cmd, sizeof(cmd),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 3\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		method.c_str(), uri);
		
	rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	log_debug("%s\n", buf);	
	
	std::string recv(buf);
	int code = get_response_code(recv);
	log_debug("%s: response code is %d\n", __func__, code);
	if(code != 401)
		return;
	std::string authenticate = get_header_line(recv, "WWW-Authenticate");
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
	
	std::string response = get_response(method);
	log_debug("response=%s\n", response.c_str());
}

void rtsp_describe_authorized(int sockfd)
{
	method = "DESCRIBE";
	char send_buf[1024];
	char recv_buf[2048];
	int num = 0;
	std::string response = get_response(method);
	snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 4\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		method.c_str(), uri,
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri, response.c_str());
		
	rtsp_send_recv(sockfd, send_buf, strlen(send_buf), recv_buf, sizeof(recv_buf));
	log_debug("%s\n", recv_buf);	
	
	//提取trackID信息
	std::string body = get_body(recv_buf);	
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	indexL = body.find("a=x-dimensions");
	if(indexL == std::string::npos)
		return;
	indexL = body.find("rtsp://", indexL);
	if(indexL == std::string::npos)
		return;
	indexR = body.find("\r\n", indexL);
	if(indexR == std::string::npos)
		return;
	track_id = body.substr(indexL, indexR - indexL);
	log_debug("trackID= %s\n", track_id.c_str());	
}

int get_server_port(const std::string& header, unsigned short* rtp_port, unsigned short* rtcp_port)
{
	std::size_t posL = 0;
	std::size_t posR = 0;
	if((posL = header.find("server_port=")) == std::string::npos)
		return -1;
	if((posL = header.find("=", posL)) == std::string::npos)
		return -2;
	if((posR = header.find("-", posL)) == std::string::npos)
		return -3;
	std::string str_port = header.substr(posL + 1, posR - posL -1);
	unsigned short port = static_cast<unsigned short>(stoi(str_port));
	*rtp_port = port;
	
	posL = posR;
	if(posR = header.find(";", posL) == std::string::npos)
		return -4;
	str_port = header.substr(posL + 1, posR - posL -1);
	port = static_cast<unsigned short>(stoi(str_port));
	*rtcp_port = port;
	return 0;
}

std::string get_session(std::string& header)
{
	std::size_t posL = 0;
	std::size_t posR = 0;
	if((posL = header.find("Session:")) == std::string::npos)
		return "";
	if((posL = header.find(":", posL)) == std::string::npos)
		return "";
	if((posR = header.find(";", posL)) == std::string::npos)
		return "";
	std::string session = header.substr(posL + 1, posR - posL -1);
	session.erase(0, session.find_first_not_of(' '));
	session.erase(session.find_last_not_of(' ') + 1);
	return session;
}

void rtsp_setup(int sockfd)
{
	method = "SETUP";
	char send_buf[1024];
	char recv_buf[2048];
	int num = 0;
	std::string response = get_response(method);
	snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 5\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
		method.c_str(), track_id.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri, response.c_str(), rtp_client_port, rtcp_client_port);
		
	rtsp_send_recv(sockfd, send_buf, strlen(send_buf), recv_buf, sizeof(recv_buf));
	log_debug("%s\n", recv_buf);	
	
	//从响应消息中提取server_port
	std::string header(recv_buf);
	get_server_port(header, &rtp_server_port, &rtcp_server_port);
	log_debug("rtp_port= %d, rtcp_port= %d\n", rtp_server_port, rtcp_server_port);
	
	session = get_session(header);
	log_debug("session: %s\n", session.c_str());
}

void rtsp_play(int sockfd)
{
	method = "PLAY";
	char send_buf[1024];
	char recv_buf[2048];
	int num = 0;
	std::string response = get_response(method);
	snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 6\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", "\
		"nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Session: %s\r\n"
		"Range: npt=0.000-\r\n\r\n",
		method.c_str(), track_id.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri, response.c_str(), session.c_str());
		
	rtsp_send_recv(sockfd, send_buf, strlen(send_buf), recv_buf, sizeof(recv_buf));
	log_debug("%s\n", recv_buf);	
	
	//从响应消息中提取server_port
	std::string header(recv_buf);
	get_server_port(header, &rtp_server_port, &rtcp_server_port);
	log_debug("rtp_port= %d, rtcp_port= %d\n", rtp_server_port, rtcp_server_port);
}

void rtsp(int sockfd)
{
	
	snprintf(uri, sizeof(uri), "rtsp://%s:%d%s", 
				SERVER_ADDR, SERVER_PORT, VIDEO_URL);
	rtsp_options(sockfd);
   rtsp_describe(sockfd);  
   rtsp_describe_authorized(sockfd); 
   rtsp_setup(sockfd);
   _rtp_init();
   rtsp_play(sockfd);
 }

