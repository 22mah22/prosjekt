/*
Server program using SISCI to set a segment available and post an image over to a connected client.
Does that make sense? I guess push is default behavior, so maybe the client should expose the memory.
No. The server always imports the memory segment set available by tbe client. 
That's how we can use DMA and multicast.
*/

#include <stdio.h>
#include "sisci_api.h"
#include "sisci_error.h"

//Defines
#define SEGMENT_SIZE 1024
#define SEGMENT_ID 4
#define RECEIVER_NODE_ID 8
#define RECEIVER_SEGMENT_ID 4
#define ADAPTER_NO 0
#define NO_FLAGS 0
#define NO_ARGS 0
#define NO_CALLBACK 0

//Global variables
sci_desc_t v_dev;
sci_error_t err;
sci_remote_segment_t r_seg;
size_t remote_segment_size;
sci_map_t r_map;
volatile int* remote_address;



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

    // Connect to the remote segment
    SCIConnectSegment(v_dev, &r_seg, RECEIVER_NODE_ID, RECEIVER_SEGMENT_ID, ADAPTER_NO, NO_CALLBACK, NO_ARGS, SCI_INFINITE_TIMEOUT, NO_FLAGS ,&err);
    if(err != SCI_ERR_OK){
        printf("Error connecting to remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }
    remote_segment_size = SCIGetRemoteSegmentSize(r_seg);
    remote_address = (volatile int*) SCIMapRemoteSegment(r_seg, &r_map, 0, remote_segment_size, 0, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error mapping remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Send data
    remote_address[0] = 0xdeadbeef;

    //Clean up
    
    SCIUnmapSegment(r_map, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error unmapping remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    SCIDisconnectSegment(r_seg, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error disconnecting remote segment: %s\n", SCIGetErrorString(err));
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