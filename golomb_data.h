#pragma once
#include <cmath>

#define no_error				0
#define param_error			-1 
#define out_of_boundary		-2
#define uev_too_long			-3

class golomb_data
{
public:
	golomb_data(const unsigned char* buf, std::size_t len)
	{
		byte_offset = const_cast<unsigned char*>(buf);// 从第一个字节开始
		byte_offset_max = const_cast<unsigned char*>(buf + len -1);		
		bit_offset = 7; // 从bit7开始
	}
	
	~golomb_data()
	{
	}
	
	int init()
	{
		add_bits(32); // 填充32位
		log_debug("%s: dat32= %08x\n", __func__, dat32);
		return no_error;		
	}	
	
	//取出n bits作为一个数据，最多8位
	unsigned char get_bits(std::size_t n, int* err)
	{
		unsigned char dat = 0;
		if(n > 8)
		{
			*err = param_error; //出错
			return 0;
		}
		unsigned int temp = dat32;
		temp >>= (32-n);
		//log_debug("%s: temp= %08x\n", __func__, temp);
		dat = static_cast<unsigned char>(temp);		
		add_bits(n);
		return dat;		
	}	
	
	//取出一个uev数据
	uev get_uev(int* err)
	{
		//log_debug("%s: dat32= %08x\n", __func__, dat32);
		int zero_num = 0;
		unsigned int mask = 0x80000000;
		for(int i = 0; i < 16; ++i)
		{
			if(dat32 & mask)
				break;
			mask >>= 1;
			zero_num ++;			
		}
		//log_debug("%s: zero_num= %u\n", __func__, zero_num);	
		
		//32位数据，前面最多15个0，后面再读15位，总共31位，多于15个0就太大了
		if(zero_num > 15)
		{
			log_error("%s: dat32= %08x has too much zero\n", __func__, dat32);
			*err = uev_too_long;
			return 0;
		}
		
		unsigned int info = 0;
		for(int i = 0; i < zero_num; ++i)
		{
			mask >>= 1;
			info <<= 1;
			if(dat32 & mask)
				info ++;			
		}
		//log_debug("%s: info= %u\n", __func__, info);
		unsigned int dat = pow(2, zero_num) - 1 + info;
		*err = no_error;		
		add_bits(zero_num * 2 + 1);				
		return dat;
	}
	
	//取出一个sev数据
	sev get_sev(int* err)
	{		
		sev sev_dat = 0;
		uev uev_dat = get_uev(err);
		if( uev_dat & 0x01 )
			sev_dat = (uev_dat + 1) / 2;
		else
			sev_dat = 0 - (uev_dat / 2);					
		return sev_dat;
	}
	
	// 填充n bits到dat32尾部,超出数组边界后填充0
	void add_bits(std::size_t n)
	{		
		for(std::size_t i = 0; i < n; ++i)
		{			
			dat32 <<= 1;
			if(byte_offset > byte_offset_max)			
				continue;					
			if( (*byte_offset) & (0x01 << bit_offset) )			
				dat32++;		
			if(bit_offset == 0)
			{
				byte_offset ++;	// 指向下一个字节
				bit_offset = 7;
			}
			else
				bit_offset--;			
		}		
	}
	
private:
	unsigned int dat32; // 待解析的32位数据，从最高位开始使用
	unsigned char* byte_offset; // 下次要读入的字节在数组中的位置, from 0 to (len-1)	
	unsigned char* byte_offset_max;
	unsigned char bit_offset; // 下次要读入的bit在字节中的位置，from 7 to 0
};
