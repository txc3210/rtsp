#pragma once

void rtsp_options(int sockfd);
void rtsp_describe(int sockfd);
void rtsp_describe_authorized(int sockfd);
std::string get_header_value(const std::string& header, const char* key);
void rtsp(int sockfd);
