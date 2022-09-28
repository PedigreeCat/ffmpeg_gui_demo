#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

void rec_audio(void);
void set_status(int status);

#ifdef __cplusplus
}
#endif