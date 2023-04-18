#ifndef Process_video_H
#define Process_video_H

// includes
#include <stdio.h>

// IMPORT macro will be defined by the implementation file
// Other clients will not define it, and will get an extern declaration
// This prevents the compiler from complaining about multiple definitions

#ifdef Process_video_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

// Function prototypes
EXTERN void Process_video(void);


#undef Process_video_IMPORT
#undef EXTERN
#endif