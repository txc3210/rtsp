#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <thread>
#include "crtsp.h"
#include "crtp.h"
#include "log.h"
#include "md5.h"


//RTP首次发送给摄像头的见面礼
const unsigned char start_cmd[] = {0xce, 0xfa, 0xed, 0xfe};

/********************************************************
* 功能：构造函数，初始化相关参数
* 参数：user, 访问摄像头的帐号
* 参数：pwd, 访问摄像头的密码
* 参数：ip, 摄像头的IP地址
* 参数：port, 摄像头的端口号
* 返回：无
********************************************************/
crtsp::crtsp(
	const char* user, 
	const char* pwd,
	const char* ip,
	unsigned short port )
{
	username = user;
	password = pwd;
	rtsp_ip = ip;
	rtsp_port = port;
	
	rtp_client_port = 60000;
	rtcp_client_port = 60001;
	
	char buf[255];
	snprintf(buf, sizeof(buf), "rtsp://%s:%d%s", 
				rtsp_ip.c_str(), rtsp_port, VIDEO_URL);
	uri = buf;
}

crtsp::~crtsp()
{
	if(sockfd > 0)
		close(sockfd);
}

int crtsp::start()
{
	int ret = connect_server();
	if(ret != 0)
	{
		log_error("%s: connect server failed, err= %d\n", __func__, ret);
		return -1;
	}
	
	ret = options();
	if(ret != 0)
	{
		log_error("%s: rtsp options failed, err= %d\n", __func__, ret);
		return -2;
	}
	
	ret = describe();
	if(ret != 0)
	{
		log_error("%s: rtsp describe failed, err= %d\n", __func__, ret);
		return -3;
	}
	
	ret = describe_authorized();
	if(ret != 0)
	{
		log_error("%s: rtsp describe authorized failed, err= %d\n", __func__, ret);
		return -4;
	}
	
	ret = setup();
	if(ret != 0)
	{
		log_error("%s: rtsp setup failed, err= %d\n", __func__, ret);
		return -5;
	}
	
	//ret = rtp_init();
	//crtp rtp(rtsp_ip, rtp_port, rtp_client_port);	
	rtp.init(rtsp_ip, rtp_port, rtp_client_port);	
	ret = rtp.start();
	if(ret != 0)
	{
		log_error("%s: rtp start failed, err= %d\n", __func__, ret);
		return -6;
	}
	
	ret = play();
	if(ret != 0)
	{
		log_error("%s: rtsp play failed, err= %d\n", __func__, ret);
		return -7;
	}
	return 0;
}


int crtsp::stop()
{
	int ret = teardown();
	if(ret != 0)
	{
		log_error("%s: rtsp teardown failed, err= %d\n", __func__, ret);
		return -1;
	}
	close(sockfd);
	sockfd = -1;
	return 0;
}

int crtsp::connect_server()
{
	struct sockaddr_in server_addr;
	//初始化SOCKET
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		log_error("%s(): create socket error:%s(errno:%d)\n", __func__, strerror(errno), errno);
		return -1;
	}	
	//初始化
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, rtsp_ip.c_str(), &server_addr.sin_addr);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
	server_addr.sin_port = htons(rtsp_port);//设置的端口为SERVER_PORT	

	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		log_error("connect server error:%s(errno:%d)\n", strerror(errno), errno);
		return -2;
	}
	log_debug("connect server succeed, sockfd= %d\n", sockfd);	
	return 0;
}

int crtsp::tcp_send(const char* buf, int len)
{
	int send_num = 0;
	//循环发送直到所有数据发送出去	
	while(true)
	{
		int num = send(sockfd, buf + send_num, len - send_num, 0);
		if(num < 0)
		{
			log_error("rtsp tcp send failed\n");
			return -1;
		}
		send_num += num;
		if(send_num >= len)
			break;
	}
	return send_num;
}

int crtsp::tcp_recv(char* buf, int len)
{
	//循环接收数据，直到全部接收完成
	int recv_num = 0;
	while(true)
	{
		int num = recv(sockfd, buf + recv_num, len - 1 - recv_num, 0);
		if(num < 0)
		{
			log_error("%s: tcp recv failed\n", __func__);
			return -1;
		}else if(num == 0)
		{
			log_error("%s: socket has been closed\n", __func__);
			return -2;
		}
		recv_num += num;
		buf[recv_num] = 0;
		
		if(recv_num >= (len - 1))
			return -3;
		//判断header部分是否接收完成
		char* header_end = strstr(buf, "\r\n\r\n");
		if( header_end == nullptr)
		{
			log_debug("data is not enough, continue recv\n");
			continue;
		}
		header_end += 4; //算上\r\n\r\n
		//没有Content-Length字段，表明数据接收完成
		if(strstr(buf, "Content-Length") == nullptr)
		{
//			log_debug("%s: Recv complete, no body part\n", __func__);
			break;
		}
		//有Content-Length字段，继续判断body部分是否接收完成
		std::string header(buf, static_cast<std::size_t>(header_end - buf));
		//header.clear();
		//header.append(buf, static_cast<std::size_t>(header_end - buf));
		std::string str_content_length = get_header_value(header, "Content-Length");
		if(str_content_length.empty())
		{
			log_error("Get Content-Length from header failed\n");
			return -4;
		}
//		log_debug("content-length:%s\n", str_content_length.c_str());
		
		std::size_t content_length = 0;
		try{
			content_length = static_cast<std::size_t> (std::stoul(str_content_length));
		}catch(const std::invalid_argument& ia)
		{	
			log_error("%s: invalid argument exception, what= %s\n", __func__, ia.what());
			return -5;
		}catch(const std::out_of_range& oor)
		{
			log_error("%s: ot of range exception, what= %s\n", __func__, oor.what());
			return -6;
		}
		//body.clear();
		//body.append(header_end);
		std::string body(header_end);
		if(body.length() >= content_length)
		{
//			log_debug("%s: Recv complete, include body part\n", __func__);
			break;
		}		
	}	
	return recv_num;
}

int crtsp::options()
{
	int ret = 0;
	char send_buf[1024];
	char recv_buf[1024];
	
	int len = snprintf(send_buf, sizeof(send_buf),
		"OPTIONS %s RTSP/1.0\r\n"\
		"CSep: 2\r\n"\
		"User-Agent: Linux program\r\n\r\n",
		uri.c_str());	
		
	log_debug("tcp_send:\n%s", send_buf);
	ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);	
	return 0;
}


int crtsp::describe()
{
	method="DESCRIBE";
	char send_buf[1024];
	char recv_buf[2048];
	
	int len = snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 3\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		method.c_str(), uri.c_str());
		
	//rtsp_send_recv(sockfd, cmd, strlen(cmd), buf, sizeof(buf));
	//log_debug("%s\n", buf);	
	log_debug("tcp_send:\n%s", send_buf);
	int ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);		
	
	std::string recv(recv_buf);
	int code = get_response_code(recv);
//	log_debug("%s: response code is %d\n", __func__, code);
	if(code != 401)
	{
		log_error("%s: response code is %d\n", __func__, code);
		return -3;
	}
	std::string authenticate = get_header_line(recv, "WWW-Authenticate");
	if(authenticate.empty())
		return -4;
//	log_debug("authenticate= %s\n", authenticate.c_str());
	nonce = get_string_value(authenticate, "nonce");
	if(nonce.empty())
		return -5;
//	log_debug("nonce= %s\n", nonce.c_str());
	
	realm = get_string_value(authenticate, "realm");
	if(realm.empty())
		return -6;
//	log_debug("realm= %s\n", realm.c_str());
	
	std::string response = get_response(method);
//	log_debug("response=%s\n", response.c_str());
	return 0;
}

int crtsp::describe_authorized()
{
	method = "DESCRIBE";
	char send_buf[1024];
	char recv_buf[2048];
	
	std::string response = get_response(method);
	int len = snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 4\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Accept: application/sdp\r\n\r\n",
		method.c_str(), uri.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri.c_str(), response.c_str());
		
	log_debug("tcp_send:\n%s", send_buf);
	int ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);		
	
	//提取trackID信息
	std::string body = get_body(recv_buf);	
	std::size_t indexL = 0;
	std::size_t indexR = 0;
	indexL = body.find("a=x-dimensions");
	if(indexL == std::string::npos)
		return -3;
	indexL = body.find("rtsp://", indexL);
	if(indexL == std::string::npos)
		return -4;
	indexR = body.find("\r\n", indexL);
	if(indexR == std::string::npos)
		return -5;
	track_id = body.substr(indexL, indexR - indexL);
	//log_debug("trackID= %s\n", track_id.c_str());	
	return 0;
}

int crtsp::setup()
{
	method = "SETUP";
	char send_buf[1024];
	char recv_buf[1024];
	
	std::string response = get_response(method);
	int len = snprintf(send_buf, sizeof(send_buf),
		"%s %s RTSP/1.0\r\n"\
		"CSep: 5\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
		method.c_str(), track_id.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri.c_str(), response.c_str(), rtp_client_port, rtcp_client_port);
		
	log_debug("tcp_send:\n%s", send_buf);
	int ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);		
	
	//从响应消息中提取server_port
	std::string header(recv_buf);
	get_server_port(header, &rtp_port, &rtcp_port);
//	log_debug("rtp_port= %d, rtcp_port= %d\n", rtp_server_port, rtcp_server_port);
	
	session = get_session(header);
//	log_debug("session: %s\n", session.c_str());
	return 0;
}

int crtsp::play()
{
	method = "PLAY";
	char send_buf[1024];
	char recv_buf[1024];
	
	std::string response = get_response(method);
	int len = snprintf(send_buf, sizeof(send_buf),
		"%s %s/ RTSP/1.0\r\n"\
		"CSep: 6\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", "\
		"nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Session: %s\r\n"
		"Range: npt=0.000-\r\n\r\n",
		method.c_str(), uri.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri.c_str(), response.c_str(), session.c_str());
		
	log_debug("tcp_send:\n%s", send_buf);
	int ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);
	return 0;
}

int crtsp::teardown()
{
	method = "TEARDOWN";
	char send_buf[1024];
	char recv_buf[1024];
	
	std::string response = get_response(method);
	int len = snprintf(send_buf, sizeof(send_buf),
		"%s %s/ RTSP/1.0\r\n"\
		"CSep: 7\r\n"\
		"Authorization: Digest username=\"%s\", realm=\"%s\", "\
		"nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"\
		"User-Agent: Linux program\r\n"\
		"Session: %s\r\n\r\n",
		method.c_str(), uri.c_str(),
		username.c_str(), realm.c_str(), nonce.c_str(),
		uri.c_str(), response.c_str(), session.c_str());
		
	log_debug("tcp_send:\n%s", send_buf);
	int ret = tcp_send(send_buf, len);	
	if(ret < 0)
		return -1;
		
	ret = tcp_recv(recv_buf, sizeof(recv_buf));
	if(ret < 0)
		return -2;	
	log_debug("tcp_recv:\n%s\n", recv_buf);
	return 0;
}

std::string crtsp::get_header_value(const std::string& header, const char* key)
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


int crtsp::get_response_code(const std::string& response)
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


std::string crtsp::get_header_line(const std::string& header, const char* name)
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


std::string crtsp::get_string_value(const std::string& str, const char* key)
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

std::string crtsp::get_response(std::string& method)
{
	char temp[1024];
	snprintf(temp, sizeof(temp), "%s:%s:%s", 
				username.c_str(), realm.c_str(), password.c_str());
	std::string part1;
	GetMD5(temp, part1);
	
	snprintf(temp, sizeof(temp), "%s:%s", method.c_str(), uri.c_str());
	std::string part3;
	GetMD5(temp, part3);	
	
	snprintf(temp, sizeof(temp), "%s:%s:%s",
					part1.c_str(), nonce.c_str(), part3.c_str());
	std::string response;
	GetMD5(temp, response);
	
	return response;
}

std::string crtsp::get_body(const char* buf)
{
	const char* header_end = strstr(buf, "\r\n\r\n");
	if( header_end == nullptr)
	{
		log_debug("header end flag does not exist\n");
		return "";
	}	
	std::string body(header_end + 4); //算上\r\n\r\n
	return body;
}

int crtsp::get_server_port(const std::string& header, unsigned short* _rtp_port, unsigned short* _rtcp_port)
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
	*_rtp_port = port;
	
	posL = posR;
	if( (posR = header.find(";", posL) ) == std::string::npos)
		return -4;
	str_port = header.substr(posL + 1, posR - posL -1);
	port = static_cast<unsigned short>(stoi(str_port));
	*_rtcp_port = port;
	return 0;
}

std::string crtsp::get_session(std::string& header)
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

/*
static void rtp_recv_thread(int sockfd)
{
	struct sockaddr_in client;
	char buf[BUFFER_SIZE + 1];
	int num = 0;
	log_debug("%s: rtp recv thread start....\n", __func__);
	socklen_t len = sizeof(struct sockaddr);
	unsigned long long sum = 0;
	while(true)
	{
		num = recvfrom(sockfd, buf, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr*>(&client), &len);
		if(num <= 0)
			break;
		sum += static_cast<unsigned long long>(num);
		log_debug("%s: recv %llu byte from UDP\r", __func__, sum);
	}	
}

int crtsp::rtp_init()
{
	struct sockaddr_in client;
	
	int rtp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(rtp_sockfd < 0)
	{
		log_error("%s: socket error\n", __func__);
		return -1;
	}	

	bzero(&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(rtp_client_port);
	client.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(rtp_sockfd, reinterpret_cast<struct sockaddr*>(&client), sizeof(struct sockaddr)) < 0)
	{
		log_error("%s: bind socket error\n", __func__);
		return -2;
	}
	
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(rtp_port);
	//inet_pton(AF_INET, "192.198.108.2",&server.sin_addr);
	server.sin_addr.s_addr = inet_addr(rtsp_ip.c_str());	
	
	int ret = sendto(rtp_sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -3;
	}
	log_debug("%s: rtp send start cmd(%d bytes)\n", __func__, ret);
	usleep(100000);
	
	ret = sendto(rtp_sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -4;
	}
	log_debug("%s: rtp send start cmd(%d bytes)\n", __func__, ret);
	
	std::thread th(rtp_recv_thread, rtp_sockfd);
	th.detach();
	
	return 0;
}
*/
