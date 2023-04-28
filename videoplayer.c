// import system and application specific headers


// include the header file for the module for compiler checking
// the IMPORT macro is used to prevent the compiler from generating
// multiple definitions of the module's public functions
#define Videoplayer_IMPORT
#include "videoplayer.h"

// include for AVPixelFormat
// include SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_pixels.h>

#include <libavutil/pixfmt.h>


int create_videoplayer(){
 // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return;
    }

    // Create a window to display the video
    SDL_Window* window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    // Create a renderer for the window
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Set the color to which the renderer will clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

void update_videoplayer(AVFrame *frame, uint32_t delay_time){
    
    // Create a texture from the frame
    SDL_PixelFormat* sdl_pix_fmt = select_format(frame->format);
    SDL_Texture* texture = SDL_CreateTexture(renderer, sdl_pix_fmt, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
    SDL_UpdateYUVTexture(texture, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);

    // Clear the renderer
    SDL_RenderClear(renderer);

    // Copy the texture to the renderer
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Show the renderer
    SDL_RenderPresent(renderer);

    // Delay to achieve the desired frame rate
    SDL_Delay(delay_time);

    // Free the frame and the texture
    av_frame_free(&frame);
    SDL_DestroyTexture(texture);
}


void destroy_videoplayer(void){
    // Destroy the renderer and the window
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // Quit SDL video subsystem
    SDL_Quit();
}

SDL_PixelFormat* select_format(AVPixelFormat FFMPEGformat){
    SDL_PixelFormat* sdl_pix_fmt;
    switch (FFMPEGformat) {
    case AV_PIX_FMT_RGB24:
        sdl_pix_fmt = SDL_PIXELFORMAT_RGB24;
        break;
    case AV_PIX_FMT_BGR24:
        sdl_pix_fmt = SDL_PIXELFORMAT_BGR24;
        break;
    case AV_PIX_FMT_RGBA:
        sdl_pix_fmt = SDL_PIXELFORMAT_RGBA32;
        break;
    case AV_PIX_FMT_BGRA:
        sdl_pix_fmt = SDL_PIXELFORMAT_BGRA32;
        break;
    case AV_PIX_FMT_YUV420P:
        sdl_pix_fmt = SDL_PIXELFORMAT_IYUV;
        break;
    case AV_PIX_FMT_NV12:
        sdl_pix_fmt = SDL_PIXELFORMAT_NV12;
        break;
    case AV_PIX_FMT_YUYV422:
        sdl_pix_fmt = SDL_PIXELFORMAT_YUY2;
        break;
    case AV_PIX_FMT_UYVY422:
        sdl_pix_fmt = SDL_PIXELFORMAT_UYVY;
        break;
    case AV_PIX_FMT_MP4:
        sdl_pix_fmt = SDL_PIXELFORMAT_RGB24;
        break;
    case AV_PIX_FMT_MP4A:
        sdl_pix_fmt = SDL_PIXELFORMAT_RGB24;
        break;
    default:
        sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
        break;
}
    return sdl_pix_fmt;
}