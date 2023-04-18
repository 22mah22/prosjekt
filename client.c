/*
Client program using SISCI to set a segment available and post an image over to a connected client.
Does that make sense? I guess push is default behavior, so maybe the client should expose the memory.
No. The server always imports the memory segment set available by tbe client. 
That's how we can use DMA and multicast.
*/

#include <stdio.h>
#include <unistd.h>
#include "sisci_api.h"
#include "sisci_error.h"

//Defines
#define SEGMENT_SIZE 1024
#define RECEIVER_NODE_ID 8
#define RECEIVER_SEGMENT_ID 4
#define ADAPTER_NO 0
#define NO_FLAGS 0
#define NO_ARGS 0
#define NO_CALLBACK 0

//Global variables
sci_desc_t v_dev;
sci_error_t err;
sci_local_segment_t l_seg;
sci_map_t l_map;
void* map_addr;

static void SleepMilliseconds(int  milliseconds)
{
if (milliseconds<1000) {
    usleep(1000*milliseconds);
} else if (milliseconds%1000 == 0) {
    sleep(milliseconds/1000);
} else {
    usleep(1000*(milliseconds%1000));
    sleep(milliseconds/1000);
}
}

int main(int argc, char* argv[]){
    
    // Initialize SISCI
    SCIInitialize(NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error initializing SISCI: %s\n", SCIGetErrorString(err));
        return 1;
    }

    // Open a virtual device
    SCIOpen(&v_dev, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    // Create a local segment
    SCICreateSegment(v_dev, &l_seg, RECEIVER_SEGMENT_ID, SEGMENT_SIZE, NO_CALLBACK, NO_FLAGS, NO_ARGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error creating local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    
    SCIPrepareSegment(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err == SCI_ERR_OK){
        size_t offset = 0;
        size_t size = SEGMENT_SIZE;
        void* suggested_addr = NULL;
        map_addr = SCIMapLocalSegment(l_seg, &l_map, offset, size, suggested_addr, NO_FLAGS, &err);
        if (err != SCI_ERR_OK) {
            printf("Error mapping local segment: %s\n", SCIGetErrorString(err));
            return 1;
        }
    }
    else{
        printf("Error preparing local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }


    // Set the segment available
    SCISetSegmentAvailable(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment available: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Cast the mapped address to a pointer to an unsigned int
    //and wait for the server to write to it

    while(*((unsigned int*)map_addr) == 0){
        SleepMilliseconds(100);
    }

    // print celebratory message
    printf("Received data: %s\n", (char*)map_addr);

    //clean up
    SCISetSegmentUnavailable(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment unavailable: %s\n", SCIGetErrorString(err));
        return 1;
    }

    SCIUnmapSegment(l_map, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error unmapping segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    SCIRemoveSegment(l_seg, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error removing segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    
    SCIClose(v_dev, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error closing virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    SCITerminate();
    return 0;
}

