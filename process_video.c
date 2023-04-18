// import system and application specific headers
#include <stdio.h>


// include the header file for the module for compiler checking
// the IMPORT macro is used to prevent the compiler from generating
// multiple definitions of the module's public functions
#define Process_video_IMPORT
#include "process_video.h"


// private variables are defined using static:
static uint8_t nodeID = 4;


// private functions are defined using static:
static void Process_video_private_function(void) {
    printf("test");
}


// publicly accessible functions can be implemented here:
void Process_video(void) {
    printf("test");
}