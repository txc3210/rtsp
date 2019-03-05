#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include "log.h"
#include "crtp.h"
#include "golomb_data.h"

//RTP首次发送给摄像头的见面礼
const unsigned char start_cmd[] = {0xce, 0xfa, 0xed, 0xfe};

crtp::crtp(const std::string& _server_ip, unsigned short _server_port, unsigned short _client_port)
	: server_ip(_server_ip), server_port(_server_port), client_port(_client_port)
{
	log_debug("upd: ip: %s %d %d", server_ip.c_str(), server_port, client_port);
}

crtp::~crtp()
{
}

int crtp::connect()
{
	
	return 0;
}

unsigned char NALU[500 * 1024];

int nalu_parse(const unsigned char* payload, std::size_t payload_num)
{
	//static fstream fs("data.h264");
	static bool first_nalu = true;
	if(!first_nalu)
		return 0;
		
	unsigned char FU_header = payload[1];
	const unsigned char* FU_payload = payload + 1;
	std::size_t FU_payload_len = payload_num - 1;
	static std::size_t offset = 0;
	
	
	if(FU_header & 0x80)
	{
		log_debug("%s: FU start flag ", __func__);		
	}
	if(FU_header & 0x40)
	{
		log_debug("%s: FU end flag ", __func__);		
		first_nalu = false;
		//log_debug("nalu size= %u ", offset);
		
	}
	log_debug(" FU payload size= %d ", payload_num - 1);
	
	for(std::size_t num = 0; num < FU_payload_len; num++)
	{
		NALU[offset + num] = FU_payload[num];
	}
	offset += FU_payload_len;
	log_debug(" offset= %u", offset);
	
	if(FU_header & 0x40)
	{	
		
		std::fstream fs("NALU.h264", std::fstream::out | std::fstream::binary | std::fstream::trunc);
		fs.write((const char*)NALU, offset);
		fs.close();
	}
	
	return 0;
}
/*
#include <cmath>
uev parse_uev(unsigned int * dat32)
{
	int zero_num = 0;
	unsigned int flag = 0x80000000;
	for(int i = 0; i < 16; ++i)
	{
		if(*dat32 & flag)
			break;
		flag >>= 1;
		zero_num ++;			
	}
	if(zero_num == 0)
		return 0;
	if(zero_num > 15)
	{
		log_error("%s: dat32= %u is too large\n", __func__, *dat32);
	}
	log_debug("zero_num= %u\n", zero_num);
	flag >>= 1;
	unsigned int info = 0;
	for(int i = 0; i < zero_num; ++i)
	{
		if(*dat32 & flag)
			info += 1;
		info <<= 1;
	}
	unsigned int dat = pow(2, zero_num) - 1 + info;
}

unsigned int make_dat32(unsigned char* data, unsigned int bits_num)
{
	unsigned int dat32 = 0;
	unsigned int dat8_num = bits_num / 8;
	unsigned int rest_bits_num = bits_num % 8;
	for(unsigned int i = 0; i < dat8_num; ++i)
	{
		dat32 <<= 8;
		dat32 += static_cast<unsigned int>(*data);
		data ++;
	}
	for(unsigned int i = 0; i < rest_bits_num; ++i)
	{
		dat32 <<= 8;
		dat32 += static_cast<unsigned int>(*data);
		data ++;
	}
	return dat32;
}
*/
//解析SPS信息
int sps_parse(const unsigned char* buf, std::size_t size, struct sps_info* sps)
{
	class golomb_data dat(buf, size);
	if(dat.init() != no_error)
	{
		log_error("%s: golomb_data init faild\n", __func__);
		return -1;
	}
	int err = 0;
	sps->profile_idc = dat.get_bits(8, &err);
	log_debug("SPS profile_ide= %u\n", sps->profile_idc);
	
	dat.get_bits(8,  &err);
	
	sps->level_idc = dat.get_bits(8,  &err);
	log_debug("SPS level_idc= %u\n", sps->level_idc);
	
	sps->seq_parameter_set_id = dat.get_uev(&err);
	log_debug("SPS seq_parameter_set_id= %u\n", sps->seq_parameter_set_id);
	
	sps->log2_max_frame_num_minus4 = dat.get_uev(&err);
	log_debug("SPS log2_max_frame_num_minus4= %u\n", sps->log2_max_frame_num_minus4);
	
	sps->pic_order_cnt_type = dat.get_uev(&err);
	log_debug("SPS pic_order_cnt_type= %u\n", sps->pic_order_cnt_type);
	
	sps->max_num_ref_frames = dat.get_uev(&err);
	log_debug("SPS max_num_ref_frames= %u\n", sps->max_num_ref_frames);
	
	sps->gaps_in_frame_num_value_allowd_flag = dat.get_bits(1,  &err);
	log_debug("SPS gaps_in_frame_num_value_allowd_flag= %u\n", sps->gaps_in_frame_num_value_allowd_flag);
	
	sps->pic_width_in_mbs_minus1 = dat.get_uev(&err);
	log_debug("SPS pic_width_in_mbs_minus1= %u\n", sps->pic_width_in_mbs_minus1);
	
	sps->pic_height_in_map_units_minus1 = dat.get_uev(&err);
	log_debug("SPS pic_height_in_map_units_minus1= %u\n", sps->pic_height_in_map_units_minus1);
	
	log_info("SPS: Video width= %u, height= %u\n",
				(sps->pic_width_in_mbs_minus1 + 1) * 16, (sps->pic_height_in_map_units_minus1 + 1) * 16 );
				
	sps->frames_mbs_only_flag = dat.get_bits(1,  &err);
	log_debug("SPS frames_mbs_only_flag= %u\n", sps->frames_mbs_only_flag);
	
	sps->direct_8x8_inference_flag = dat.get_bits(1,  &err);
	log_debug("SPS direct_8x8_inference_flag= %u\n", sps->direct_8x8_inference_flag);
	
	sps->frame_cropping_flag = dat.get_bits(1,  &err);
	log_debug("SPS frame_cropping_flag= %u\n", sps->frame_cropping_flag);
	
	sps->vui_parameters_present_flag = dat.get_bits(1,  &err);
	log_debug("SPS vui_parameters_present_flag= %u\n", sps->vui_parameters_present_flag);
	
	return 0;
}

//解析PPS信息
int pps_parse(const unsigned char* buf, std::size_t size, struct pps_info* pps)
{
	class golomb_data dat(buf, size);
	if(dat.init() != no_error)
	{
		log_error("%s: golomb_data init faild\n", __func__);
		return -1;
	}
	int err = 0;
	pps->pic_parameter_set_id = dat.get_uev(&err);
	log_debug("\nPPS pic_parameter_set_id= %u\n", pps->pic_parameter_set_id);
}

int rtp_pack_parse(const unsigned char* data, std::size_t size)
{
	static unsigned int id = 0;
	if(size < 12)
	{
		log_error("%s: rpt pack size<12\n", __func__);
		return -1;
	}	
	if((data[0] >> 6) != 2)
	{
		log_error("%s: RTP version is not 2\n", __func__);
		return -2;
	}
	if(data[0] & 0x20)
	{
		std::size_t padding_len = data[size - 1];
		if((padding_len + 12) > size)
			return -3;
		size -= padding_len;
	}
	std::size_t CSRC_num = data[0] & 0x0f;
	std::size_t payload_offset = 12 + CSRC_num * 4;
	if(size < payload_offset)
		return -4;
	std::size_t payload_num = size - payload_offset;
	const unsigned char *payload = data + payload_offset;
	
	log_debug("pack %u ", id++);
	log_debug("payload size= %u ", payload_num);
	log_debug("payload[0]= %02x ", payload[0]);
	
	unsigned char type = payload[0] & 0x1f;
	log_debug("Type= %u ", type);
	if(type == 6)
	{
		log_debug(" NAUL\n");
	}	
	else if(type == 7)
	{
		struct sps_info sps;
		sps_parse(payload + 1, payload_num - 1, &sps);
	}
	else if(type == 8)
	{
		struct pps_info pps;
		pps_parse(payload + 1, payload_num - 1, &pps);
	}
	else if(type == 28)
	{
		nalu_parse(payload, payload_num);
		log_debug("\n");
	}
}


void rtp_recv_thread(int sockfd)
{
	struct sockaddr_in client;
	char buf[BUFFER_SIZE + 1];
	int num = 0;
	log_debug("%s: rtp recv thread start....\n", __func__);
	socklen_t len = sizeof(struct sockaddr);
	unsigned long long sum = 0;
	unsigned int count = 0;
	while(true)
	{		
		num = recvfrom(sockfd, buf, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr*>(&client), &len);
		if(num <= 0)
			break;
		if(count++ > 3)
			continue;
		sum += static_cast<unsigned long long>(num);
		//log_debug("%s: recv %llu byte from UDP\r", __func__, sum);
		void *ptr = buf;
		rtp_pack_parse(reinterpret_cast<const unsigned char*>(ptr), num);
		
	}	
}

int crtp::start()
{
	struct sockaddr_in client;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		log_error("%s: socket error\n", __func__);
		return -1;
	}	

	bzero(&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(client_port);
	client.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(sockfd, reinterpret_cast<struct sockaddr*>(&client), sizeof(struct sockaddr)) < 0)
	{
		log_error("%s: bind socket error\n", __func__);
		return -2;
	}
	
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);
	//inet_pton(AF_INET, "192.198.108.2",&server.sin_addr);
	server.sin_addr.s_addr = inet_addr(server_ip.c_str());	
	
	int ret = sendto(sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -3;
	}
	log_debug("%s: rtp send start cmd(%d bytes)\n", __func__, ret);
	usleep(100000);
	
	ret = sendto(sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
	if(ret < 0)
	{
		log_error("%s: send to error\n", __func__);
		return -4;
	}
	log_debug("%s: rtp send start cmd(%d bytes)\n", __func__, ret);
	
	std::thread th(rtp_recv_thread, sockfd);
	th.detach();
	return 0;
}


