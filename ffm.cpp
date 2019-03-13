#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "log.h"

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

int write_jpg(AVFrame* frame, int width, int height)
{
	int ret = 0;
	const char* out_file = "test.jpg";
	AVFormatContext * out_cxt;// = avformat_alloc_context();	
	avformat_alloc_output_context2(&out_cxt, nullptr, "singlejpeg", out_file);
	if(out_cxt == nullptr)
	{
		log_error("%s: avformat_alloc_context failed\n", __func__);
		return -1;
	}
	
	ret = avio_open(&out_cxt->pb, out_file, AVIO_FLAG_READ_WRITE);
	if(ret < 0)
	{
		log_error("%s: avio_open failed, ret= %d\n", __func__, ret);
		return -2;
	}
	
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
	
	av_dump_format(out_cxt, 0, out_file, 1);
	
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
   //unsigned char* buf = new unsigned char[10 * 1024 * 1024];
   av_new_packet(packet,size * 3);
   
   int got_picture = 0;
   ret = avcodec_encode_video2(codec_cxt,packet,frame,&got_picture);
   if(ret < 0)
    {
        log_error("%s: avcodec_encode_video2 failed, ret= %d\n", __func__, ret);
        return -1;
    }

	log_debug("got_picture %d \n",got_picture);    
   if(got_picture == 1)
	{
        //将packet中的数据写入本地文件
		ret = av_write_frame(out_cxt, packet);
    }
    log_debug("111\n");
	av_free_packet(packet);	
    //将流尾写入输出媒体文件并释放文件数据
   av_write_trailer(out_cxt);   
   if(frame)
    {
   		av_frame_unref(frame);   		
    }
	avio_close(out_cxt->pb);	
   avformat_free_context(out_cxt); 

	return  0;
}

int ffm()
{
	int ret = 0;
	av_register_all();
	//avcodec_register_all();
	ret = avformat_network_init();
	log_debug("avformat_network_init, ret= %d\n", ret);
	
	static AVFormatContext *ic = nullptr;
	//const char* file = "rtsp://admin:jns87250605@192.168.108.17:554/h264/ch1/main/av_stream";
	const char* file = "test.mp4";
	ret = avformat_open_input(&ic, file, NULL, NULL);
	if(ret != 0)
	{
		log_error("%s: avformat open input failed, ret= %d\n", __func__, ret);
		return -1;
	}
	log_debug("%s: avformat open input succeed\n", __func__);
	
	if(avformat_find_stream_info(ic, NULL) < 0)
	{
		log_error("%s: avformat_find_stream_info failed.\n", __func__);
		return -2;
	}
	log_debug("%s: avformat_find_stream_info succeed.\n", __func__);
	
	log_debug("nb_streams= %u\n", ic->nb_streams);
	
	AVStream *stream = ic->streams[0];
	AVCodecContext *context = stream->codec;
	unsigned char* data = context->extradata;
	int data_size = context->extradata_size;
	log_debug("extradata:\n");
	for(int i = 0; i < data_size; i++)
	{
		log_debug("%02x ", data[i]);
	}
	log_debug("\n");
	
	av_dump_format(ic, 0, "av.mp4", 0);
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
		return -3;
	}
	
	AVCodecContext *avc_cxt = ic->streams[video_index]->codec;
	enum AVCodecID codec_id = avc_cxt->codec_id;
	AVCodec *codec = avcodec_find_decoder(codec_id);
	if(codec == nullptr)
	{
		log_error("%s: codec not found\n", __func__);
		return -4;
	}
	
	ret = avcodec_open2(avc_cxt, codec, nullptr);
	if(ret < 0)
	{
		log_error("%s: open decoder faile, err= %d\n", __func__, ret);
		return -5;
	}
	
	
	AVPacket *pack = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	
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
	
	av_frame_free(&frame);
	av_packet_free(&pack);
	//avcodec_close();
	
	/*
	if(av_read_frame(ic, &pack) < 0)
	{
		log_error("%s: av_read_fream failed\n", __func__);
		return -3;
	}
	log_debug("%s: av_read_fream succeed\n", __func__);
	log_debug("pack size= %d\n", pack.size);
	
	*/
	
	avformat_close_input(&ic);
	
	return 0;
}
