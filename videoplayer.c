// import system and application specific headers


// include the header file for the module for compiler checking
// the IMPORT macro is used to prevent the compiler from generating
// multiple definitions of the module's public functions
#define Videoplayer_IMPORT
#include "videoplayer.h"
//#include <libavutil/pixfmt.h>

Uint32 select_format(enum AVPixelFormat ffmpeg_pix_fmt);

Uint32 select_format(enum AVPixelFormat ffmpeg_pix_fmt) {
    switch (ffmpeg_pix_fmt) {
        case AV_PIX_FMT_RGB24:
            return SDL_PIXELFORMAT_RGB24;
        case AV_PIX_FMT_BGR24:
            return SDL_PIXELFORMAT_BGR24;
        case AV_PIX_FMT_RGBA:
            return SDL_PIXELFORMAT_RGBA32;
        case AV_PIX_FMT_BGRA:
            return SDL_PIXELFORMAT_BGRA32;
        case AV_PIX_FMT_YUV420P:
            return SDL_PIXELFORMAT_IYUV;
        case AV_PIX_FMT_NV12:
            return SDL_PIXELFORMAT_NV12;
        case AV_PIX_FMT_YUYV422:
            return SDL_PIXELFORMAT_YUY2;
        case AV_PIX_FMT_UYVY422:
            return SDL_PIXELFORMAT_UYVY;
        default:
            printf("Unsupported pixel format: %d\n", ffmpeg_pix_fmt);
            return 0;
    }
}

int update_avframe_from_framedata(FrameData *framedata, AVFrame *frame, void *data_addr) {
    int ret;
    
    // Set up the AVFrame data and linesize arrays
    ret = av_image_fill_arrays(frame->data, frame->linesize, (uint8_t*)data_addr, framedata->pix_fmt, framedata->width, framedata->height, framedata->align);
    if (ret < 0) {
        fprintf(stderr, "Failed to set up AVFrame data and linesize arrays: %s\n", av_err2str(ret));
        return ret;
    }
    
    // Set other frame properties
    frame->format = framedata->pix_fmt;
    frame->width = framedata->width;
    frame->height = framedata->height;

    return 0;
}

int update_framedata_from_avframe(AVFrame *frame, FrameData *framedata) {
    // Set the dimensions and pixel format
    framedata->width = frame->width;
    framedata->height = frame->height;
    framedata->pix_fmt = frame->format;

    // Calculate the required buffer size
    int buffer_size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);
    if (buffer_size < 0) {
        return buffer_size;
    }
    framedata->buffer_size = buffer_size;

    //Update linesizes
    int i;
    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        framedata->linesize[i] = frame->linesize[i];
    }
    framedata->align = 1;
    return 0;
}


int create_videoplayer(VideoPlayer* vp){
 // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window to display the video
    vp->window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN);
    if (!vp->window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a renderer for the window
    vp->renderer = SDL_CreateRenderer(vp->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!vp->renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(vp->window);
        SDL_Quit();
        return 1;
    }

    // Set the color to which the renderer will clear the screen
    SDL_SetRenderDrawColor(vp->renderer, 0, 0, 0, 255);
    return 0;
}

void update_videoplayer(VideoPlayer* vp, AVFrame *frame, uint32_t delay_time){
    
    // Create a texture from the frame
    Uint32 SDLFormat = select_format(frame->format);

    SDL_Texture* texture = SDL_CreateTexture(vp->renderer, SDLFormat , SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
    SDL_UpdateYUVTexture(texture, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
    // Clear the renderer
    SDL_RenderClear(vp->renderer);
    // Copy the texture to the renderer
    SDL_RenderCopy(vp->renderer, texture, NULL, NULL);

    // Show the renderer
    SDL_RenderPresent(vp->renderer);

    // Delay to achieve the desired frame rate
    SDL_Delay(delay_time);
    // Free the frame and the texture
    SDL_DestroyTexture(texture);
}


void destroy_videoplayer(VideoPlayer* vp){
    // Destroy the renderer and the window
    SDL_DestroyRenderer(vp->renderer);
    SDL_DestroyWindow(vp->window);

    // Quit SDL video subsystem
    SDL_Quit();
}
