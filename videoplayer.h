#ifndef Videoplayer_H
#define Videoplayer_H

// includes
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>


// include SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_pixels.h>

#define FRAME_RATE 30
// IMPORT macro will be defined by the implementation file
// Other clients will not define it, and will get an extern declaration
// This prevents the compiler from complaining about multiple definitions

#ifdef Videoplayer_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

//Struct for frame data.
//Writing frame data without context over PCIe makes reconstruction impossible
//if we dont know the exact format. 
//We therefore need a metadata struct to send with the frame data.
typedef struct {
    int width;
    int height;
    enum AVPixelFormat pix_fmt;
    int buffer_size;
    int linesize[AV_NUM_DATA_POINTERS];
    int align;
} FrameData;

// Reconstruction function for after PCIe transfer
int update_avframe_from_framedata(FrameData *framedata, AVFrame *frame, void *data_addr);
int update_framedata_from_avframe(AVFrame *frame, FrameData *framedata);

typedef struct VideoPlayer {
    SDL_Window *window;
    SDL_Renderer *renderer;
} VideoPlayer;

// opaque:
EXTERN int create_videoplayer(VideoPlayer* vp);
EXTERN void update_videoplayer(VideoPlayer* vp, AVFrame *frame, uint32_t delay_time);
EXTERN void destroy_videoplayer(VideoPlayer* vp);


// Function prototypes


#undef Videoplayer_IMPORT
#undef EXTERN
#endif