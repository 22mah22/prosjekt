#ifndef Videoplayer_H
#define Videoplayer_H

// includes
#include <stdio.h>
#include <libavcodec/avcodec.h>

#define FRAME_RATE 30
// IMPORT macro will be defined by the implementation file
// Other clients will not define it, and will get an extern declaration
// This prevents the compiler from complaining about multiple definitions

#ifdef Videoplayer_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

// opaque:
EXTERN int create_videoplayer(void);
EXTERN void update_videoplayer(AVFrame *frame, uint32_t delay_time);
EXTERN void destroy_videoplayer(void);
EXTERN SDL_PixelFormat* select_format(AVPixelFormat FFMPEGformat);


// Function prototypes


#undef Videoplayer_IMPORT
#undef EXTERN
#endif