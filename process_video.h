#ifndef Process_video_H
#define Process_video_H

// includes
#include <stdio.h>
#include <pthread.h>

// Might be excessive, should be trimmed down to only what is needed
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>


// IMPORT macro will be defined by the implementation file
// Other clients will not define it, and will get an extern declaration
// This prevents the compiler from complaining about multiple definitions

#ifdef Process_video_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

// opaque:
typedef struct AVFrame_Q AVFrame_Q;
EXTERN void av_frame_deep_copy(AVFrame* dest, const AVFrame* src);

EXTERN int ffmpeg_init();
EXTERN AVFrame_Q* avframe_Q_alloc(int capacity);
EXTERN void avframe_Q_destroy(AVFrame_Q* q);
EXTERN int avframe_Q_push(AVFrame_Q* q, AVFrame* frame);
EXTERN int avframe_Q_pop(AVFrame_Q* q, AVFrame* dest);
EXTERN int avframe_Q_get_size(AVFrame_Q* q);
EXTERN int avframe_Q_flush(AVFrame_Q* q);

// Arg struct for process_video
struct process_video_args{
    char* filename;
    AVFrame_Q* q;
    int stop_threshold;
} process_video_args;

// Function prototypes
EXTERN void* process_video(void* arg);


#undef Process_video_IMPORT
#undef EXTERN
#endif