#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include "log.h"
#include "crtp.h"
#include "golomb_data.h"
#include "ffm.h"
#include "config.h"

crtp::crtp()
{
	h264.data = nullptr;
	h264.size = 0;
	
	SPS = nullptr;
	PPS = nullptr;
	SEI = nullptr;
	NAL = nullptr;	
	
	work_done = false; // 工作内容未完成	
	NAL_num = 0; // 
	last_seq = 0;
}

crtp::~crtp()
{
	if(sockfd > 0)
		close(sockfd);
		
	if(h264.data != nullptr)
		delete[] h264.data;
		
	if(SPS != nullptr)
		delete[] SPS;
		
	if(PPS != nullptr)
		delete[] PPS;
		
	if(SEI != nullptr)
		delete[] SEI;
		
	if(NAL != nullptr)
		delete[] NAL;
}

int crtp::molloc_buffer()
{	
	h264.data = new unsigned char[DATA_SIZE];
	if(h264.data == nullptr)
		return -1;
	
	SPS = new unsigned char[255];
	if(SPS == nullptr)
		return -2;
	PPS = new unsigned char[255];
	if(PPS == nullptr)
		return -3;
	SEI = new unsigned char[255];
	if(SEI == nullptr)
		return -4;
	NAL = new unsigned char[DATA_SIZE];
	if(NAL == nullptr)
		return -5;
	return 0;
}

int crtp::init(
	const std::string& server_ip, 
	unsigned short server_port, 
	unsigned short client_port)
{
	this->server_ip = server_ip;
	this->server_port = server_port;
	this->client_port = client_port;	
	//log_debug("upd: ip: %s %d %d", server_ip.c_str(), server_port, client_port);	
	return molloc_buffer();	
}

int crtp::nalu_parse(const unsigned char* payload, std::size_t payload_num)
{
	unsigned char FU_indicator = payload[0];		
	unsigned char FU_header = payload[1];
	
	const unsigned char* FU_payload = payload + 2;
	std::size_t FU_payload_len = payload_num - 2;
	
	static std::size_t offset = 0;
	static unsigned char NAL_header = 0; // 组装后的NAL单元的头部
	
	if(FU_header & 0x80) // NAL的第一个包
	{
		log_debug("%s: seq= %d, FU start flag \n", __func__, last_seq);	
		NAL_header = (FU_indicator & 0xE0) | (FU_header & 0x1F);
/*		offset = 0;
		 
		NAL[offset] = NAL_header; // 新的header
		offset++;
*/		
		if(NAL_num == 0)
		{
			h264_write(SPS, sps_len);			
			h264_write(PPS, pps_len);			
			h264_write(SEI, sei_len);				
		}
		h264_write(&NAL_header, 1);		
	}
	if(FU_header & 0x40) // NAL的最后一个包
	{
		log_debug("%s: set= %d, FU end flag \n", __func__, last_seq);		
		NAL_num ++;
	}
/*	
	for(std::size_t num = 0; num < FU_payload_len; num++)
	{
		NAL[offset + num] = FU_payload[num];		
	}
	offset += FU_payload_len;
	*/
	h264_write_bytes(FU_payload, FU_payload_len);	
	
/*	
	if(FU_header & 0x40)
	{				
		if(NAL_num == 1)
		{
			h264_write(SPS, sps_len);			
			h264_write(PPS, pps_len);			
			h264_write(SEI, sei_len);
		}		
		h264_write(NAL, offset);			
		if(NAL_num == MAX_FRAME_NUM)
		{
			//save_file();
			log_debug("NAL data: size= %d\n", h264.size);
			for(int i = 0; i < 100; i++)
			{			
				log_debug("%02x ", h264.data[i]);
			}
			log_debug("\n");
		}
	}	*/
	return NAL_num;
}

//将数据保存到mp4文件中
int crtp::save_file()
{		
	std::fstream fs;
	fs.open("test.mp4", std::fstream::out | std::fstream::binary | std::fstream::trunc);
	fs.write((const char*)(h264.data), h264.size);
	fs.close();	
	return 0;
}

//解析SPS信息
int crtp::sps_parse(const unsigned char* buf, std::size_t size, struct sps_info* sps)
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
	log_debug("SPS seq_parameter_set_id= %u\n", 
				sps->seq_parameter_set_id);
	
	sps->log2_max_frame_num_minus4 = dat.get_uev(&err);
	log_debug("SPS log2_max_frame_num_minus4= %u\n", 
				sps->log2_max_frame_num_minus4);
	
	sps->pic_order_cnt_type = dat.get_uev(&err);
	log_debug("SPS pic_order_cnt_type= %u\n", 
				sps->pic_order_cnt_type);
	
	sps->max_num_ref_frames = dat.get_uev(&err);
	log_debug("SPS max_num_ref_frames= %u\n", 
				sps->max_num_ref_frames);
	
	sps->gaps_in_frame_num_value_allowd_flag = dat.get_bits(1,  &err);
	log_debug("SPS gaps_in_frame_num_value_allowd_flag= %u\n", 
				sps->gaps_in_frame_num_value_allowd_flag);
	
	sps->pic_width_in_mbs_minus1 = dat.get_uev(&err);
	log_debug("SPS pic_width_in_mbs_minus1= %u\n", 
				sps->pic_width_in_mbs_minus1);
	
	sps->pic_height_in_map_units_minus1 = dat.get_uev(&err);
	log_debug("SPS pic_height_in_map_units_minus1= %u\n", 
				sps->pic_height_in_map_units_minus1);
	
	log_info("SPS: Video width= %u, height= %u\n",
				(sps->pic_width_in_mbs_minus1 + 1) * 16,
				(sps->pic_height_in_map_units_minus1 + 1) * 16 );
				
	sps->frames_mbs_only_flag = dat.get_bits(1,  &err);
	log_debug("SPS frames_mbs_only_flag= %u\n", 
				sps->frames_mbs_only_flag);
	
	sps->direct_8x8_inference_flag = dat.get_bits(1,  &err);
	log_debug("SPS direct_8x8_inference_flag= %u\n", 
				sps->direct_8x8_inference_flag);
	
	sps->frame_cropping_flag = dat.get_bits(1,  &err);
	log_debug("SPS frame_cropping_flag= %u\n", 
				sps->frame_cropping_flag);
	
	sps->vui_parameters_present_flag = dat.get_bits(1,  &err);
	log_debug("SPS vui_parameters_present_flag= %u\n", 
				sps->vui_parameters_present_flag);
	
	return 0;
}

//解析PPS信息
int crtp::pps_parse(const unsigned char* buf, std::size_t size, struct pps_info* pps)
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
	return 0;
}

int crtp::rtp_pack_parse(const unsigned char* data, std::size_t size)
{
//	static unsigned int id = 0;
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
		size -= padding_len;//去掉padding data
	}
	
	std::size_t CSRC_num = data[0] & 0x0f;
	unsigned short seq_num = (data[2] << 8) + data[3];
	if(last_seq == 0)
		last_seq = seq_num; //  把第一个报文的seq当作起始seq,这里不严谨
	else
	{
		if(seq_num == (last_seq + 1))
			last_seq = seq_num;
		else
			log_error("%s: this message arrived ahead of schedule\n", __func__);
	}		
	
	std::size_t payload_offset = 12 + CSRC_num * 4;
	if(size < payload_offset)
		return -4;
	std::size_t payload_num = size - payload_offset;
	const unsigned char *payload = data + payload_offset;
	
//	log_debug("pack %u ", id++);
//	log_debug("seq= %u ", seq_num);
//	log_debug("payload size= %u ", payload_num);
//	log_debug("payload[0]= %02x ", payload[0]);
	
	unsigned char type = payload[0] & 0x1f;
//	log_debug("Type= %u ", type);
	if(type == TYPE_SEI)
	{
		//log_debug(" NAUL SEI information\n");
		for(std::size_t i = 0; i < payload_num; i++)
		{
			SEI[i] = payload[i];			
		}
		sei_len = payload_num;
	}	
	else if(type == TYPE_SPS)
	{
		struct sps_info sps;
		//sps_parse(payload + 1, payload_num - 1, &sps);
		
		for(std::size_t i = 0; i < payload_num; i++)
		{
			SPS[i] = payload[i];			
		}
		sps_len = payload_num;
	}
	else if(type == TYPE_PPS)
	{
		struct pps_info pps;
		//pps_parse(payload + 1, payload_num - 1, &pps);
		
		for(std::size_t i = 0; i < payload_num; i++)
		{
			PPS[i] = payload[i];			
		}
		pps_len = payload_num;
	}
	else if(type == TYPE_FU_A)
	{
		return nalu_parse(payload, payload_num);		
	}
	return 0;
}


void rtp_recv_thread(int sockfd, class crtp* rtp)
{
	struct sockaddr_in client;
	char buf[BUFFER_SIZE + 1];
	int num = 0;
	log_debug("%s: rtp recv thread start....\n", __func__);
	socklen_t len = sizeof(struct sockaddr);
	unsigned long long sum = 0;

	int frame_num = 0;
	rtp->h264.size = 0;
	rtp->NAL_num = 0;
	
	while(true)
	{		
		num = recvfrom(sockfd, buf, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr*>(&client), &len);
		if(num <= 0)
			break;
		sum += static_cast<unsigned long long>(num);
		//log_debug("%s: recv %llu byte from UDP\r", __func__, sum);
		void *ptr = buf;
		frame_num = rtp->rtp_pack_parse(reinterpret_cast<const unsigned char*>(ptr), num);
		if(frame_num >= MAX_FRAME_NUM)
			break;		
	}		
	log_debug("%s: rtp recv thread finished....\n", __func__);
	ffm(&rtp->h264);
	rtp->work_done = true; // 工作内容已完成	
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
	
	//RTP首次发送给摄像头的见面礼
	const unsigned char start_cmd[] = {0xce, 0xfa, 0xed, 0xfe};
	
	for(int i = 0; i < 2; i++)
	{
		int ret = sendto(sockfd, start_cmd, sizeof(start_cmd), 0, reinterpret_cast<struct sockaddr*>(&server), sizeof(server));
		if(ret < 0)
		{
			log_error("%s: send to error\n", __func__);
			return -3;
		}
		//log_debug("%s: rtp send start cmd(%d bytes)\n", __func__, ret);
		usleep(100000);
	}	
	
	std::thread th(rtp_recv_thread, sockfd, this);
	th.detach();

	return 0;
}

int crtp::h264_write(const unsigned char* buf, std::size_t size)
{
	const unsigned char frame_begin_flag[4] = {0x00,0x00,0x00,0x01}; // NAL起始符
	std::size_t index = 0;
	if((h264.size + size + sizeof(frame_begin_flag)) >= DATA_SIZE) // 剩余空间不足
	{
		log_error("%s: no more buffer\n", __func__);
		return -1;
	}
	for(index = 0; index < sizeof(frame_begin_flag); index++)
	{
		h264.data[h264.size] = frame_begin_flag[index];
		h264.size++;		
	}
	for(index = 0; index < size; index++)
	{
		h264.data[h264.size] = *(buf + index);
		h264.size++;		
	}
//	log_debug("h264.size= %u\n", h264.size);
	return 0;
}

int crtp::h264_write_bytes(const unsigned char* buf, std::size_t size)
{	
	std::size_t index = 0;
	if((h264.size + size) >= DATA_SIZE) // 剩余空间不足
	{
		log_error("%s: no more buffer\n", __func__);
		return -1;
	}	
	for(index = 0; index < size; index++)
	{
		h264.data[h264.size] = *(buf + index);
		h264.size++;		
	}
	//h264.size += size;
//	log_debug("h264.size= %u\n", h264.size);
	return 0;
}


