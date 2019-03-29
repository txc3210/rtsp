#pragma once

#define IP_CAMERA		1
#define My_Computer	2

#define TARGET			1

#if TARGET == 1
	#define SERVER_ADDR	"192.168.108.17"
	#define SERVER_PORT	554
#elif TARGET == 2
	#define SERVER_ADDR	"192.168.108.2"
	#define SERVER_PORT	8000
#endif

#define VIDEO_URL		"/h264/ch1/main/av_stream"

#define RUN_BACKGROUND			0 // 是否在后面运行
#define PICTURE_INTERVAL			10 //摄像头拍照周期，单位为分钟
#define MAX_PICTURE_NUM			1000

#define MAX_FRAME_NUM		2 // 从摄像头读取多少个NAL数据



