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

