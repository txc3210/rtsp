#pragma once
#include <string>

void rtsp_options(int sockfd);
void rtsp_describe(int sockfd);
void rtsp_describe_authorized(int sockfd);
std::string get_header_value(const std::string& header, const char* key);
void rtsp(int sockfd);

unsigned short get_rtp_client_port();
unsigned short get_rtcp_client_port();
unsigned short get_rtp_server_port();
unsigned short get_rtcp_server_port();
