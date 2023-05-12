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
#include "videoplayer.h"
#include <libavcodec/avcodec.h>

//Defines
#define SEGMENT_SIZE sizeof(FrameData)
#define RECEIVER_NODE_ID 8
#define FRAMEDATA_SEGMENT_ID 4
#define FRAME_SEGMENT_ID 8
#define ADAPTER_NO 0
#define NO_FLAGS 0
#define NO_ARGS 0
#define NO_CALLBACK 0

//Global variables
sci_error_t err;

sci_desc_t v_dev;
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


int request_segment_size(FrameData* frame_data){
    //Open a virtual device
    SCIOpen(&v_dev, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Create a local segment
    SCICreateSegment(v_dev, &l_seg, FRAMEDATA_SEGMENT_ID, SEGMENT_SIZE, NO_CALLBACK, NO_ARGS, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error creating local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Prepare the segment for use
    SCIPrepareSegment(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if (err == SCI_ERR_OK){
        //Map the segment into the virtual address space
        map_addr = SCIMapLocalSegment(l_seg, &l_map, 0, SEGMENT_SIZE, NULL, NO_FLAGS, &err);
    }
    else{
        printf("Error preparing local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Set the segment available
    SCISetSegmentAvailable(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment available: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Wait for server to write the metadata

    printf("Waiting for server to write metadata\n");
    //Initialize the segment to 0
    *(int*)map_addr = 0;
    while(*(int*)map_addr == 0){
        SleepMilliseconds(100);
    }
    printf("Received data from server\n");
    //Use memcpy to copy the data from the segment to the frame_data struct
    memcpy(frame_data, (FrameData*)map_addr, sizeof(FrameData));
    printf("Frame data: %d, %d, %d, %d\n", frame_data->width, frame_data->height, frame_data->pix_fmt, frame_data->buffer_size);
    //In later iterations, we can avoid closing the segment for 
    // updates to the frame_data struct if the video format changes

    //For now, we close the segment
    SCISetSegmentUnavailable(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment unavailable: %s\n", SCIGetErrorString(err));
        return 1;
    }
    
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
    return 0;
}


int main(int argc, char* argv[]){
    
    // Initialize SISCI
    SCIInitialize(NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error initializing SISCI: %s\n", SCIGetErrorString(err));
        return 1;
    }

    FrameData* frame_data = malloc(sizeof(FrameData));
    request_segment_size(frame_data);
    printf("Frame data: %d, %d, %d, %d\n", frame_data->width, frame_data->height, frame_data->pix_fmt, frame_data->buffer_size);

    //Prepare for frame data transfers

    // Open a virtual device
    SCIOpen(&v_dev, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    // Create a local segment
    SCICreateSegment(v_dev, &l_seg, FRAME_SEGMENT_ID, frame_data->buffer_size , NO_CALLBACK, NO_FLAGS, NO_ARGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error creating local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    
    SCIPrepareSegment(l_seg, ADAPTER_NO, NO_FLAGS, &err);
    if(err == SCI_ERR_OK){
        size_t offset = 0;
        size_t size = frame_data->buffer_size;
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
    else{
        printf("Segment set available\n");
    }

    //create videoplayer
    VideoPlayer vp;
    int non_sisci_err = create_videoplayer(&vp);
    if(non_sisci_err != 0){
        printf("Error creating videoplayer: \n");
        return 1;
    }
    printf("Videoplayer created successfully \n");

    
    AVFrame* frame = av_frame_alloc();
    if(!frame){
        printf("Error allocating frame\n");
        return 1;
    }
    

    uint32_t delay_time = FRAME_RATE;
    //DOuble check if this is correct
    //Need a buffer between CISCI address and AVFrame
    // Do I actually or will the texture work as a buffer?
    //uint8_t buffer* = malloc(frame_data->buffer_size)
    //wait for the server to write to the segment
    *(int*)map_addr = 0;
    while(1){
        if(*(int*)map_addr == 0){
            // Only before the server has written to the segment
            // One frame lasts 33.3 ms so otherwise this would be too much
            printf("Waiting\n");
            SleepMilliseconds(100);
        }
        else{
            // Hopefully the delay time here works as an actual delay
            printf("We ball\n");
            //Create AVFrame from FrameData
            int non_sisci_err = update_avframe_from_framedata(frame_data, frame, map_addr);
            if(non_sisci_err != 0){
                printf("Error creating AVFrame from FrameData: \n");
                return 1;
            }
            else{
                printf("AVFrame created successfully \n");
            }
            update_videoplayer(&vp, frame, delay_time);
            delay_time = FRAME_RATE - SDL_GetTicks() % FRAME_RATE;
        }
    }

    //clean up
    av_frame_free(&frame);
    destroy_videoplayer(&vp);

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

