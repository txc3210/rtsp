#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include "log.h"
#include "crtp.h"
#include <ctime>

extern "C"
{
#include <libavutil/error.h>
#include <libavutil/avstring.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include "ffm.h"
#include "raii.h"

/****************************************************************
* 功能：根据当前时间生成图片的文件名称
* 参数：无
* 返回：含相对路径的文件名称
****************************************************************/
std::string generate_file_name()
{
	char name[64];
	time_t tt;
	time(&tt);
	struct tm stm;
	localtime_r(&tt, &stm);
	strftime(name, sizeof(name), "./picture/%Y%m%d_%H%M%S.jpg", &stm);		
	return std::string(name);
}

int write_jpg(AVFrame* frame, int width, int height)
{
	int ret = 0;
	std::string file_name = generate_file_name();
	AVFormatContext * out_cxt;// = avformat_alloc_context();	
	avformat_alloc_output_context2(&out_cxt, nullptr, "singlejpeg", file_name.c_str());
	if(out_cxt == nullptr)
	{
		log_error("%s: avformat_alloc_context failed\n", __func__);
		return -1;
	}
	AVFormatContext_memory_guard guard(&out_cxt); // 自动释放内存
	
	ret = avio_open(&out_cxt->pb, file_name.c_str(), AVIO_FLAG_READ_WRITE);
	if(ret < 0)
	{
		log_error("%s: avio_open failed, ret= %d\n", __func__, ret);
		return -2;
	}
	AVIOContext_close_guard avio_close_guard(&(out_cxt->pb)); // RAII自动关闭对象
	
	AVStream* stream = avformat_new_stream(out_cxt, nullptr);
	if(stream == nullptr)
	{
		log_error("%s: avformat_new_stream failed\n", __func__);
		return -3;
	}
	
	AVCodecContext *codec_cxt = stream->codec;
	//AVCodecParameters *codec_cxt = stream->codecpar;
	codec_cxt->codec_id = out_cxt->oformat->video_codec;
	codec_cxt->codec_type = AVMEDIA_TYPE_VIDEO;
	codec_cxt->pix_fmt = AV_PIX_FMT_YUVJ420P;
	codec_cxt->height = height;
	codec_cxt->width = width;
	codec_cxt->time_base.num = 1;
	codec_cxt->time_base.den = 25;
	
//	av_dump_format(out_cxt, 0, file_name.c_str(), 1);
	
	AVCodec* codec = avcodec_find_encoder(codec_cxt->codec_id);
	if(codec == nullptr)
	{
		log_error("%s: jpe encoder not found\n", __func__);
		return -4;
	}
	
	ret = avcodec_open2(codec_cxt, codec, nullptr);
	if(ret < 0)
	{
		log_error("%s: avcodec_open2 failed, ret= %d\n", __func__, ret);
		return -5;
	}
	avcodec_parameters_from_context(stream->codecpar,codec_cxt);
	//写入文件头
   avformat_write_header(out_cxt, nullptr);
   int size = codec_cxt->width * codec_cxt->height;
   
   AVPacket* packet = av_packet_alloc();
   if(packet == nullptr)
	{
		log_error("%s: av_packet_alloc failed\n", __func__);
		return -6;
	}
   AVPacket_memory_guard packet_guard(&packet);
   
   
   //unsigned char* buf = new unsigned char[10 * 1024 * 1024];
   ret = av_new_packet(packet,size * 3);
   if(ret < 0)
	{
		log_error("%s: av_new_packet failed\n", __func__);
		return -7;
	}
	//这里是否需要释放内存？
   
   int got_picture = 0;
   ret = avcodec_encode_video2(codec_cxt,packet,frame,&got_picture);
   if(ret < 0)
    {
        log_error("%s: avcodec_encode_video2 failed, ret= %d\n", __func__, ret);
        return -8;
    }

	log_debug("got_picture %d \n",got_picture);    
   if(got_picture == 1)
	{
        //将packet中的数据写入本地文件
		ret = av_write_frame(out_cxt, packet);
    }    
	//av_free_packet(packet);	
    //将流尾写入输出媒体文件并释放文件数据
   av_write_trailer(out_cxt);   
   if(frame)
    {
   		av_frame_unref(frame);   		
    }
	//avio_close(out_cxt->pb);	
   //avformat_free_context(out_cxt); 

	return  0;
}

int fill_iobuffer(void *opaque, unsigned char* buf, int bufsize)
{
//	log_debug("ptr opaque= %p\n", opaque);
	struct h264_data* h264 = reinterpret_cast<struct h264_data*>(opaque);
	memcpy(buf, h264->data, bufsize);
	return bufsize;
}

int ffm(struct h264_data* h264)
{
	int ret = 0;
	av_register_all();
	ret = avformat_network_init();
	if(ret < 0)
	{
		log_error("%s: avformat_network_init failed, ret= %d\n", ret);
		return -1;
	}	
	
	AVFormatContext *ic = nullptr;
	//*************************************************************************************
	// 从内存中读取视频文件
	ic = avformat_alloc_context();
	if(ic == nullptr)
	{
		log_error("%s: avformat_alloc_context failed\n", __func__);
		return -2;
	}
	AVFormatContext_memory_guard ic_gaurd(&ic);// RAII自动释放内存
	
	std::size_t avio_ctx_buffer_size = DATA_SIZE;
	unsigned char* avio_ctx_buffer = (unsigned char*)av_malloc(avio_ctx_buffer_size);
	if(avio_ctx_buffer == nullptr)
	{
		log_error("%s: avio_ctx_buffer malloc failed\n", __func__);
		return -3;
	}
	//memory_guard guard((void**)&avio_ctx_buffer); //自动释放内存
	
//	log_debug("ptr h264= %p\n", h264);
	AVIOContext* avio_cxt = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, h264, fill_iobuffer, nullptr, nullptr);
	if(avio_cxt == nullptr)
	{
		log_error("%s: avio_alloc_context failed\n", __func__);
		return -4;
	}
	AVIOContext_memory_guard avio_guard(&avio_cxt);	
	
	ic->pb = avio_cxt;
	ret = avformat_open_input(&ic, NULL, NULL, NULL);
	//*************************************************************************************	
	//ret = avformat_open_input(&ic, file, NULL, NULL);
	if(ret != 0)
	{
		log_error("%s: avformat open input failed, ret= %d\n", __func__, ret);
		return -5;
	}
	log_debug("%s: avformat open input succeed\n", __func__);
	AVFormatContext_close_guard close_guard(&ic); // RAII自动关闭
	
	
	if(avformat_find_stream_info(ic, NULL) < 0)
	{
		log_error("%s: avformat_find_stream_info failed.\n", __func__);
		return -6;
	}
	log_debug("%s: avformat_find_stream_info succeed.\n", __func__);
	
	//log_debug("nb_streams= %u\n", ic->nb_streams);
	
	AVStream *stream = ic->streams[0];
	AVCodecContext *context = stream->codec;
	unsigned char* data = context->extradata;
	int data_size = context->extradata_size;
/*	
	log_debug("extradata:\n");
	for(int i = 0; i < data_size; i++)
	{
		log_debug("%02x ", data[i]);
	}
	log_debug("\n");
*/	
	//av_dump_format(ic, 0, "av.mp4", 0);
	int video_index = -1;
	for(unsigned int i = 0; i < ic->nb_streams; i++)
	{
		if(ic->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_index = i;
			break;
		}
	}
	if(video_index == -1)
	{
		log_error("%s: video stream not found\n", __func__);
		return -7;
	}
	
	AVCodecContext *avc_cxt = ic->streams[video_index]->codec;
	enum AVCodecID codec_id = avc_cxt->codec_id;
	AVCodec *codec = avcodec_find_decoder(codec_id);
	if(codec == nullptr)
	{
		log_error("%s: codec not found\n", __func__);
		return -8;
	}
	
	ret = avcodec_open2(avc_cxt, codec, nullptr);
	if(ret < 0)
	{
		log_error("%s: open decoder faile, err= %d\n", __func__, ret);
		return -9;
	}	
	
	AVPacket *pack = av_packet_alloc();
	if(pack == nullptr)
	{
		log_error("%s: av_packet_alloc failed\n", __func__);
		return -10;
	}
	AVPacket_memory_guard packet_guard(&pack); // 自动释放内存
	
	AVFrame *frame = av_frame_alloc();
	if(frame == nullptr)
	{
		log_error("%s: av_frame_alloc failed\n", __func__);
		return -11;
	}
	AVFrame_memory_guard frame_guard(&frame); // 自动释放内存
	
	av_init_packet(pack);
	pack->data = nullptr;
	pack->size = 0;
	
	while(av_read_frame(ic, pack) >= 0)
	{
		if(pack && pack->stream_index == video_index)
		{	
			int got_frame = 0;
			ret = avcodec_decode_video2(avc_cxt, frame, &got_frame, pack);
			if(got_frame)
			{
				log_debug("got frame, width= %d, height= %d\n", avc_cxt->width, avc_cxt->height);
				ret = write_jpg(frame, avc_cxt->width, avc_cxt->height);
				if(ret == 0)				
					break;
			}
		}
	}
	
	//av_frame_free(&frame);
	//av_packet_free(&pack);
	//avcodec_close();		
	//avformat_close_input(&ic);	
	return 0;
}
