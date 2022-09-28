#define _CRT_SECURE_NO_WARNINGS
#include "test.h"
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include <Windows.h>
#include <string.h>

FILE* fp = NULL;
static int rec_status = 0;
const int MP2_UINT_SAMPLES = 1152;
const int AAC_UINT_SAMPLES = 1024;

void set_status(int status)
{
	rec_status = status;
}

void logcbk(void *ptr, int level, const char *fmt, va_list vl)
{
	if (!fp) {
		fp = fopen("temp.log", "w");
	}
	vfprintf(fp, fmt, vl);
	fflush(fp);
}

#define ADTS_HEADER_LEN (7)

const int sampleFrequencyTable[] = {
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
};

int getADTSHeader(char* adtsHeader, int packetSize, int profile, int sampleRate, int channels)
{
	int sampleFrequencyIndex = 3; // 默认采样率对应48000Hz
	int adtsLength = packetSize + ADTS_HEADER_LEN;

	for (int i = 0; i < sizeof(sampleFrequencyTable) / sizeof(sampleFrequencyTable[0]); i++)
	{
		if (sampleRate == sampleFrequencyTable[i])
		{
			sampleFrequencyIndex = i;
			break;
		}
	}

	adtsHeader[0] = 0xff;               // syncword:0xfff                                  12 bits

	adtsHeader[1] = 0xf0;               // syncword:0xfff
	adtsHeader[1] |= (0 << 3);          // MPEG Version: 0 for MPEG-4, 1 for MPEG-2         1 bit
	adtsHeader[1] |= (0 << 1);          // Layer:00                                         2 bits
	adtsHeader[1] |= 1;                 // protection absent:1                              1 bit

	adtsHeader[2] = (profile << 6);     // profile:0 for Main, 1 for LC, 2 for SSR          2 bits
	adtsHeader[2] |= (sampleFrequencyIndex & 0x0f) << 2; // sampling_frequency_index        4 bits
	adtsHeader[2] |= (0 << 1);                           // private bit:0                   1 bit
	adtsHeader[2] |= (channels & 0x04) >> 2;             // channel configuration           3 bits

	adtsHeader[3] = (channels & 0x03) << 6;              // channel configuration
	adtsHeader[3] |= (0 << 5);                           // original_copy                   1 bit
	adtsHeader[3] |= (0 << 4);                           // home                            1 bit
	adtsHeader[3] |= (0 << 3);                           // copyright_identification_bit    1 bit
	adtsHeader[3] |= (0 << 2);                           // copytight_identification_start  1 bit
	adtsHeader[3] |= ((adtsLength & 1800) >> 11);        // AAC frame length               13 bits

	adtsHeader[4] = (uint8_t)((adtsLength & 0x7f8) >> 3);// AAC frame length
	adtsHeader[5] = (uint8_t)((adtsLength & 0x7) << 5);  // AAC frame length
	adtsHeader[5] |= 0x1f;                               // buffer fullness:0x7ff          11 bits
	adtsHeader[6] = 0xfc;                                // buffer fullness:0x7ff
	return 0;
}

void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *output)
{
	int ret = 0;
	
	// 将数据送编码器
	ret = avcodec_send_frame(ctx, frame);

	// 如果ret>=0说明数据设置成功
	while (ret >= 0) {
		// 获取编码后的音频数据，如果成功，需要重复获取，知道失败为止
		ret = avcodec_receive_packet(ctx, pkt);
		if (ret < 0) {
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				return;
			}
			else if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "encoding audio frame\n");
				exit(-1);
			}
		}
		// write file
		// add ADTS header
		char* adts_header[ADTS_HEADER_LEN] = { 0 };
		getADTSHeader(adts_header, pkt->size, ctx->profile, ctx->sample_rate, ctx->channels);
		fwrite(adts_header, 1, ADTS_HEADER_LEN, output);
		fwrite(pkt->data, 1, pkt->size, output);
		fflush(output);
	}
	return;
}

SwrContext* init_swr()
{
	SwrContext* swr_ctx = NULL;
	// channel,number
	swr_ctx = swr_alloc_set_opts(
		NULL,					// ctx
		AV_CH_LAYOUT_STEREO,	// 输出channel布局
		AV_SAMPLE_FMT_FLTP,	    // 输出的采样格式
		44100,					// 输出采样率
		AV_CH_LAYOUT_STEREO,	// 输入channel布局
		AV_SAMPLE_FMT_S16,		// 输入的采样格式
		44100,					// 输入采样率
		0, NULL);
	if (!swr_ctx)
		exit(1);

	if (swr_init(swr_ctx) < 0)
		exit(1);
	return swr_ctx;
}

AVCodecContext* open_coder()
{
	// 打开编码器
	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	// AVCodec* codec = avcodec_find_encoder_by_name("libfdk_aac");
	if (!codec)
	{
		av_log(NULL, AV_LOG_ERROR, "avcodec_find_encoder failed.");
		return NULL;
	}

	// 创建codec上下文
	AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
	// ffmpeg内部在调用libfdk_aac的API限制了sample_fmt为S16（阅读源码可知）
	// 由于使用的库里没有编译libfdk_aac，这里使用ffmpeg内置的AAC编码器，fmt只能设置为FLTP
	codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;		// 输入音频的采样大小
	codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;// 输入音频的channel layout
	codec_ctx->channels = 2;						// 输入音频channel个数
	codec_ctx->sample_rate = 44100;					// 输入音频的采样率
	codec_ctx->bit_rate = 0;						// 码率：AAC_LC:128K,AAC HE:64K,AAC HE V2:32K
	codec_ctx->profile = FF_PROFILE_AAC_HE_V2;		// 阅读ffmpeg源码可知，bit_rate设置为0时，会更具当前profile去设置bit_rate

	// 打开编码器
	int ret = avcodec_open2(codec_ctx, codec, NULL);
	if (ret < 0)
	{
		av_log(codec_ctx, AV_LOG_ERROR, "err: %s", av_err2str(ret));
		return NULL;
	}
	return codec_ctx;
}

void rec_audio() {
	int ret = 0;
	char errors[1024];

	// ctx
	AVFormatContext* fmt_ctx = NULL;
	AVDictionary* options = NULL;

	// packet
	int count = 0;
	AVPacket pkt;

	wchar_t* wcdevicename = L"audio=麦克风 (HECATE G2 GAMING HEADSET)";
	int str_size = WideCharToMultiByte(CP_UTF8, 0, wcdevicename, -1, NULL, 0, NULL, NULL);
	char* cdevicename = malloc(str_size);
	WideCharToMultiByte(CP_UTF8, 0, wcdevicename, -1, cdevicename, str_size, NULL, NULL);

	av_log_set_level(AV_LOG_INFO);
	av_log_set_callback(logcbk);

	// start to record audio
	rec_status = 1;

	// register audio device
	avdevice_register_all();
	avcodec_register_all();

	// get format
	AVInputFormat* iformat = av_find_input_format("dshow");

	// set options
	//ret = av_dict_set(&options, "sample_rate", "16000", 0);
	//ret = av_dict_set(&options, "channels", "2", 0);

	// open device
	if (ret = avformat_open_input(&fmt_ctx, cdevicename, iformat, &options) < 0) {
		av_strerror(ret, errors, 1024);
		av_log(NULL, AV_LOG_ERROR, "Failed to open audio device, [%d]%s\n", ret, errors);
		return;
	}

	// av_init_packet(&pkt);

	// create file
	char* out = "audio.aac";
	char* pcm_file = "audio.pcm";
	FILE* outfile = fopen(out, "wb+");
	FILE* pcmfile= fopen(pcm_file, "wb+");


	AVCodecContext* c_ctx = open_coder();

	AVFrame* frame = av_frame_alloc();
	if (!frame)
		exit(1);

	frame->nb_samples = AAC_UINT_SAMPLES;	// 单通道一个音频帧的采样数
	frame->format = AV_SAMPLE_FMT_FLTP;	// 每个采样的大小
	frame->channel_layout = AV_CH_LAYOUT_STEREO;	// channel layout
	av_frame_get_buffer(frame, 0);	// nb_samples * channels
	if (!frame->buf[0])
		exit(1);

	AVPacket* newpkt = av_packet_alloc(); // 分配编码后的数据空间
	if (!newpkt)
		exit(1);

	SwrContext *swr_ctx = init_swr();
	uint8_t** src_data = NULL;
	int src_linesize = 0;
	uint8_t** dst_data = NULL;
	int dst_linesize = 0;
	int dst_count = 0;

	// 单个通道采样点数计算: 单个Packet大小/采样位数对于字节数/通道数
	// 创建输入缓冲区
	av_samples_alloc_array_and_samples(
		&src_data,					// 输入缓冲区地址
		&src_linesize,				// 缓冲区大小
		2,							// 通道个数
		22050,						// 单通道采样个数
		AV_SAMPLE_FMT_S16,			// 采样格式
		0); 

	// 创建输出缓冲区
	dst_count = av_rescale_rnd(22050, 44100, 44100, AV_ROUND_UP);
	av_samples_alloc_array_and_samples(
		&dst_data,					// 输出缓冲区地址
		&dst_linesize,				// 缓冲区大小
		2,							// 通道个数
		dst_count,					// 单通道采样个数
		AV_SAMPLE_FMT_FLTP,			// 采样格式
		0);

	// read data from device
	while (ret = av_read_frame(fmt_ctx, &pkt) == 0 && rec_status) {
		// write file

		av_log(NULL, AV_LOG_DEBUG, "packet size is %d(%p)\n",
			pkt.size, pkt.data);

		// 进行内存拷贝，按字节拷贝
		memcpy((void*)src_data[0], (void*)pkt.data, pkt.size);

		// 重采样
		swr_convert(
			swr_ctx,					// 重采样的上下文
			dst_data,					// 输出结果缓冲区
			dst_count,						// 输出每个通道的采样数
			(const uint8_t**)src_data,	// 输入缓冲区
			22050);						// 输入单个通道的采样数

		fwrite(dst_data[1], 1, dst_linesize, pcmfile);
		fflush(pcmfile);

		uint8_t* temp_ptr = dst_data[0];
		uint8_t* temp_ptr1 = dst_data[1];
		int temp_len = dst_linesize;
		while (temp_len > frame->linesize[0]) {
			memcpy((void*)frame->data[0], (void*)temp_ptr, frame->linesize[0]);
			memcpy((void*)frame->data[1], (void*)temp_ptr1, frame->linesize[0]);
			temp_ptr += frame->linesize[0];
			temp_ptr1 += frame->linesize[0];
			temp_len -= frame->linesize[0];
			encode(c_ctx, frame, newpkt, outfile);
		}
		memcpy((void*)frame->data[0], (void*)temp_ptr, temp_len);
		memcpy((void*)frame->data[1], (void*)temp_ptr1, temp_len);
		encode(c_ctx, frame, newpkt, outfile);

		av_packet_unref(&pkt); // release 
	}
	encode(c_ctx, NULL, newpkt, outfile);

	// close file
	fclose(outfile);
	fclose(pcmfile);

	// 是否输入输出缓冲区
	if (src_data) {
		av_freep(&src_data[0]);
	}
	av_freep(&src_data);

	if (dst_data) {
		av_freep(&dst_data[0]);
	}
	av_freep(&src_data);

	swr_free(&swr_ctx);
	// close device and release ctx
	avformat_close_input(&fmt_ctx);

	av_log(NULL, AV_LOG_INFO, "finished run.\n");

	return;
}