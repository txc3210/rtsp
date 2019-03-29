/*************************************************************************
* File Name: memory_guard.h
* Author: Tao xiaochao
* Mail: 356594122@qq.com
* Created Time: 2019年03月15日 星期五 10时16分29秒
************************************************************************/

// 自动释放动态分配的AVFormatContext对象
class AVFormatContext_memory_guard
{
public:
	AVFormatContext_memory_guard(AVFormatContext** context)
	{
		this->context = context;
	}
	~AVFormatContext_memory_guard()
	{
		if(*context != nullptr)
		{
			log_debug("avformat_free_context....\n");
			avformat_free_context(*context);
		}
	}
private:
	AVFormatContext** context;
};

// 自动关闭打开的AVFormatContext对象
class AVFormatContext_close_guard
{
public:
	AVFormatContext_close_guard(AVFormatContext** context)
	{
		this->context = context;
	}
	~AVFormatContext_close_guard()
	{
		if(*context != nullptr)
		{
			log_debug("avformat_close_input....\n");
			avformat_close_input(context);
		}
	}
private:
	AVFormatContext** context;
};

// 自动释放动态分配的AVPacket对象
class AVPacket_memory_guard
{
public:
	AVPacket_memory_guard(AVPacket** packet)
	{
		this->packet = packet;
	}
	~AVPacket_memory_guard()
	{
		if(*packet != nullptr)
		{
			log_debug("av_packet_free....\n");
			av_packet_free(packet);
		}
	}
private:
	AVPacket** packet;
};

// 自动释放动态分配的AVPacket对象
class AVFrame_memory_guard
{
public:
	AVFrame_memory_guard(AVFrame** frame)
	{
		this->frame = frame;
	}
	~AVFrame_memory_guard()
	{
		if(*frame != nullptr)
		{
			log_debug("av_frame_free....\n");
			av_frame_free(frame);
		}
	}
private:
	AVFrame** frame;
};

// 自动释放动态分配的AVIOContext对象
class AVIOContext_memory_guard
{
public:
	AVIOContext_memory_guard(AVIOContext** avio)
	{
		this->avio = avio;
	}
	~AVIOContext_memory_guard()
	{
		if(*avio != nullptr)
		{
			log_debug("av_free AVIOContext....\n");
			av_free(*avio);
		}
	}
private:
	AVIOContext** avio;
};

// 自动关闭avio打开的AVIOContext对象
class AVIOContext_close_guard
{
public:
	AVIOContext_close_guard(AVIOContext** context)
	{
		this->context = context;
	}
	~AVIOContext_close_guard()
	{
		if(*context != nullptr)
		{
			log_debug("avio_close....\n");
			avio_close(*context);
		}
	}
private:
	AVIOContext** context;
};

// 自动释放动态分配的AVIOContext对象
class memory_guard
{
public:
	memory_guard(void* ptr)
	{
		this->ptr = ptr;
	}
	~memory_guard()
	{
		if(ptr != nullptr)
		{
			log_debug("av_free memory begin....\n");
			av_free(ptr);
			log_debug("av_free memory end....\n");
		}
	}
private:
	void* ptr;
};
