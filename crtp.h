#pragma once
#include <string>

#define BUFFER_SIZE	4096

typedef unsigned int uev;
typedef signed int sev;

struct sps_info{

	unsigned char profile_idc; // u(8)
	bool constraint_set0_flag; // u(1)
	bool constraint_set1_flag;
	bool constraint_set2_flag;
	bool constraint_set3_flag;
	bool constraint_set4_flag;
	bool constraint_set5_flag;
	unsigned char reserved_bits; // u(2)
	unsigned char level_idc; // U(8)
	uev seq_parameter_set_id; // ue(v)

// if(profile_idc == 100, 110, 122, 244, 44, 83, 86, 118, 128)	
	uev chroma_format_idc;
	//if (chroma_format_idc == 3) {
		bool separate_colour_plane_flag; // 1 bit
	//}
	uev bit_depth_luma_minus8;
	uev bit_depth_chroma_minus8;
	bool qpprime_y_zero_transform_bypass_flag; // 1 bit
	bool seq_scaling_matrix_present_flag; // 1 bit
	//if seq_scaling_matrix_present_flag
		bool seq_scaling_list_present_flag[12]; // 8 or 12 flag array
	//#endif
//endif	
	uev log2_max_frame_num_minus4;
	uev pic_order_cnt_type;
	
	//if pic_order_cnt_type == 0
	uev log2_max_pic_order_cnt_lsb_minus4;
	
	//else if pic_order_cnt_type == 1	
	bool delta_pic_order_always_zero_flag; 
	sev offset_for_non_ref_pic;
	sev offset_for_top_to_bottom_field;
	uev num_ref_frames_in_pic_order_cnt_cycle;
	
	sev offset_for_ref_frames[100]; // 100 = num_ref_frames_in_pic_order_cnt_cycle
	
	uev max_num_ref_frames;
	bool gaps_in_frame_num_value_allowd_flag;
	uev pic_width_in_mbs_minus1;
	uev pic_height_in_map_units_minus1;
	bool frames_mbs_only_flag;
	
	//if (frames_mbs_only_flag) {
		bool mb_adaptive_frame_field_flag;
	//}
	
	bool direct_8x8_inference_flag;
	bool frame_cropping_flag;
	
	//if (frame_cropping_flag) {
		uev frame_crop_left_offset;
		uev frame_crop_right_offset;
		uev frame_crop_top_offset;
		uev frame_crop_bottom_offset;
	//}
	
	bool vui_parameters_present_flag;
	
	//if (vui_parameters_present_flag) {
		// vui parameters
	//}
};

struct pps_info{

	uev pic_parameter_set_id;
	uev seq_parameter_set_id;
	bool entropy_coding_mode_flag;
	bool bottom_field_pic_order_in_frame_present_flag;
	uev num_slice_groups_minus1;
	/*
	if (num_slice_groups_minus1 > 0) 
	{
		uev slice_group_map_type;
		if( slice_group_map_type == 0)
		{
			for(iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
				uev run_length_minus1[iGroup];
		}
		else if(slice_group_map_type == 2)
		{
			for(iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ )
			{
				uev top_left[iGroup];
				uev bottom_right[iGroup];
			}
		}
		else if(slice_group_map_type == 3 || 
			slice_group_map_type == 4 ||
			slice_group_map_type == 5 )
		{
			bool slice_group_change_direction_flag;
			uev slice_group_change_rate_minus1;
		}
		else if(slice_group_map_type == 6)
		{
			uev pic_size_in_map_units_minus1;
			for(i = 0; i <= pic_size_in_map_units_minus1; i++)
				u(v) slice_group_id[i];
		}
	}
	*/
	uev mum_ref_idx_10_default_active_minus1;
	uev mum_ref_idx_11_default_active_minus1;
	bool weighted_pred_flag;
	unsigned char weighted_bipred_idc; //u(2)
	sev pic_init_qp_minus26;
	sev pic_init_qs_minus26;
	sev chroma_qp_index_offset;
	bool deblocking_filter_control_present_flag;
	bool constrained_intra_pred_flag;
	bool redundant_pic_cnt_present_flag;
	/*
	if(more_rbsp_data())
	{
		bool transform_8x8_mode_flag;
		bool pic_scaling_matrix_present_flag;
		if( pic_scaling_matrix_present_flag)
		{
			for(i = 0; i < 6 + ((chroma_format_idc != 3) ? 2 : 6) * transform_8x8_mode_flag; i++)
			{
				bool bic_scaling_list_present_flag[i];
				if( bic_scaling_list_present_flag[i] )
				{
					if(i < 6)
						scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[i]);
					else
						scaling_list(ScalingList8x8[i - 6], 64, UseDefaultScalingMatrix8x8Flag[i - 6]);
				}
			}			
		}	
		sev second_croma_qp_index_offset;	
	}
	rbsp_trailing_bits();
	*/
};

#define DATA_SIZE	512 * 1024
struct h264_data
{
	unsigned char* data;
	std::size_t size;
	
	//bool work_done;
};
//extern struct h264_data h264; //用于将视频流数据存储在内存中

#define TYPE_SEI		6
#define TYPE_SPS		7
#define TYPE_PPS		8
#define TYPE_FU_A		28

class crtp
{
public:
	crtp();
	//crtp(const std::string& server_ip, unsigned short server_port, unsigned short client_port);
	~crtp();
	int molloc_buffer();
	int init(const std::string& server_ip, unsigned short server_port, unsigned short client_port);
	int start();
	int stop();
	bool is_work_done(){return work_done;};
	int h264_write(const unsigned char* buf, std::size_t size);
	int h264_write_bytes(const unsigned char* buf, std::size_t size);
	int save_file();
	
	int rtp_pack_parse(const unsigned char* data, std::size_t size);
	int pps_parse(const unsigned char* buf, std::size_t size, struct pps_info* pps);
	int sps_parse(const unsigned char* buf, std::size_t size, struct sps_info* sps);
	int nalu_parse(const unsigned char* payload, std::size_t payload_num);
	//void rtp_recv_thread(int sockfd);
	bool work_done; // 指示工作内容是否完成
	struct h264_data h264;	
	std::size_t NAL_num; // 接收到的NAL数量
	
	unsigned char* SPS;
	unsigned char* PPS;
	unsigned char* SEI;
//	unsigned char* NAL;
	
	std::size_t sps_len;
	std::size_t pps_len;
	std::size_t sei_len;
	
private:
	int sockfd;
	std::string server_ip;
	unsigned short server_port;
	unsigned short client_port;
	unsigned short last_seq; // 最后一次发送的顺序正确的报文序号
};
