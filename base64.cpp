/*************************************************************************
* File Name: base64.cpp
* Author: Tao xiaochao
* Mail: 356594122@qq.com
* Created Time: 2019年03月12日 星期二 10时44分50秒
************************************************************************/
#include <memory>
#include "base64.h"

/**************************************************************
* 功能：将数字转换成base64码表中的字符
* 参数：value, 待转换的数据，取值范围为0-63
* 返回：数字对应base64码表中的字符
**************************************************************/
static char convert_to_base64(unsigned char value)
{
	//value &= 0x3F; // 确保数据在0～63之间
	if(value < 26)
		return value + 'A';
	else if(value < 52)
		return value - 26 + 'a';
	else if(value < 62)
		return value - 52 + '0';
	else if(value == 62)
		return '+';
	else
		return '=';			
}

/**************************************************************
* 功能：将base64码表中的字符转换成对应的数字
* 参数：c, 待转换的字符，取值只能是base64码表中的字符
* 返回：base64码表字符对应的数字（取值0～63之间）
**************************************************************/
static unsigned char base64_to_normal(char c)
{
	if(c >= 'A' &&  c <= 'Z')
		return c - 'A';
	else if(c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	else if(c >= '0' && c <= '9')
		return c - '0' + 52;
	else if(c == '+')
		return 62;
	else
		return 63;
}

/**************************************************************
* 功能：对字符串进行base64编码
* 参数：buf, 待转换的字符串地址
* 参数：len, 待转换的字符串长度
* 返回：base64编码后的字符串
**************************************************************/
std::string encode(const char* buf, std::size_t len)
{
	std::size_t count = len / 3;
	std::size_t rest_bytes = len % 3;
	
	std::size_t i = 0;
	std::size_t j = 0;
	unsigned int dat24 = 0; // 24 bit data from 3 bytes
	unsigned char temp = 0;

	std::unique_ptr<char[]> buf_out_ptr(new char[(count + rest_bytes) * 4  + 1]());
	char* buf_out = buf_out_ptr.get();
	
	std::size_t offset = 0;		
	
	char* ptr = const_cast<char*>(buf);
	for(i = 0; i < count; ++i)
	{			
		dat24 = (*(ptr) << 16) + (*(ptr + 1) << 8) + *(ptr + 2);					
		for(j = 0; j < 4; ++j)
		{
			temp = static_cast<unsigned char>( (dat24 & 0x00FC0000) >> 18 );
			*(buf_out + offset) = convert_to_base64(temp);
			++ offset;
			dat24 <<= 6;
		}
		ptr += 3;
	}
	
	if(rest_bytes == 2)
	{
		dat24 = (*(ptr) << 16) + (*(ptr + 1) << 8);
		for(j = 0; j < 3; ++j)
		{
			temp = static_cast<unsigned char>( (dat24 & 0x00FC0000) >> 18 );
			*(buf_out + offset) = convert_to_base64(temp);
			++ offset;
			dat24 <<= 6;
		}	
		*(buf_out + offset) = '=';
		++ offset;
	}
	
	if(rest_bytes == 1)
	{
		dat24 = (*(ptr) << 16);
		for(j = 0; j < 2; ++j)
		{
			temp = static_cast<unsigned char>( (dat24 & 0x00FC0000) >> 18 );
			*(buf_out + offset) = convert_to_base64(temp);
			++ offset;
			dat24 <<= 6;
		}		
		*(buf_out + offset) = '=';
		++ offset;
		*(buf_out + offset) = '=';
		++ offset;	
	}
	buf_out[offset] = 0;	
	std::string result(buf_out);		
	return result;
}

/**************************************************************
* 功能：将base64编码字符串解码成字节
* 参数：buf, 待解码的base64字符串地址
* 参数：len, 待解码的base64字符串长度
* 参数：vec, 存储解码后字节的vector的引用
* 返回：固定返回0
**************************************************************/
int decode(const char* buf, std::size_t len, std::vector<unsigned char>& vec)
{
	std::size_t count = len / 4;		
	unsigned int dat24 = 0; // 24 bit data from 4 base64 chars
	unsigned char temp = 0;	
	
	std::size_t num = 0;
	char* ptr = const_cast<char*>(buf);
	for(std::size_t i = 0; i < count; ++i)
	{
		dat24 = base64_to_normal(*ptr) << 18;
//		printf("dat24= %08x ", dat24);
		dat24 += base64_to_normal(*(ptr +1 )) << 12;
//		printf("dat24= %08x ", dat24);
		num = 3; // 有3个字节要解析
		if(*(ptr +2) != '=')	
		{		
			dat24 += base64_to_normal(*(ptr + 2)) << 6 ;	
			if(*(ptr + 3) != '=')			
				dat24 += base64_to_normal( *(ptr + 3) );					
			else
				num = 2; // 只有2个字节要解析
		}		
		else
			num = 1; // 只有1个字节要解析
		
//		printf("dat24= %08x\n", dat24);
		for(std::size_t j = 1; j <= 3; ++j)
		{
			temp = static_cast<unsigned char>( (dat24 & 0x00FF0000) >> 16 );
			vec.push_back(temp);			
			dat24 <<= 8;
			if(j == num)
				break;
		}	
		ptr += 4;
	}
	return 0;
}
