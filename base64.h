/*************************************************************************
* File Name: base64.cpp
* Author: Tao xiaochao
* Mail: 356594122@qq.com
* Created Time: 2019年03月12日 星期二 10时44分50秒
************************************************************************/

#pragma once
#include <string>
#include <vector>

std::string encode(const char* buf, std::size_t len);
int decode(const char* buf, std::size_t len, std::vector<unsigned char>& vec);
