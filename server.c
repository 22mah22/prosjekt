/*
Server program using SISCI to set a segment available and post an image over to a connected client.
Does that make sense? I guess push is default behavior, so maybe the client should expose the memory.
No. The server always imports the memory segment set available by tbe client. 
That's how we can use DMA and multicast.
*/

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "sisci_api.h"
#include "sisci_error.h"
#include "sisci_types.h"
#include "videoplayer.h"
#include "process_video.h"


//Defines
#define SEGMENT_SIZE sizeof(FrameData)
#define FRAMEDATA_SEGMENT_ID 4
#define LOCAL_NODE_ID 4
#define FRAME_SEGMENT_ID 8
#define RECEIVER_NODE_ID 8
#define RECEIVER_SEGMENT_ID 4
#define ADAPTER_NO 0
#define NO_FLAGS 0
#define NO_ARGS 0
#define NO_CALLBACK 0

//Global variables
sci_error_t err;

/*
sci_desc_t v_dev_local;
sci_local_segment_t l_seg;
size_t local_segment_size;
sci_map_t l_map;
*/
sci_desc_t v_dev;
sci_remote_segment_t r_seg;
size_t remote_segment_size;
sci_map_t r_map;

static volatile void* remote_address;
//static volatile void* local_address;


int main(int argc, char* argv[]){
    
    // Initialize SISCI
    SCIInitialize(NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error initializing SISCI: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Initialize ffmpeg
    ffmpeg_init();

    //create videoplayer
    VideoPlayer vp;
    int non_sisci_err = create_videoplayer(&vp);
    if(non_sisci_err != 0){
        printf("Error creating videoplayer: \n");
        return 1;
    }

    //allocate a buffer AVFrame
    AVFrame* frame = av_frame_alloc();

    //Allocate frame data
    FrameData* frame_data = (FrameData*) malloc(sizeof(FrameData));

    //allocate AVframe queue for transfer
    int queue_size = 60;
    AVFrame_Q* q = avframe_Q_alloc(queue_size);
    if(!q){
        printf("Error allocating AVFrame queue: \n");
        return 1;
    }

    //Set filename
    char* filename = "test.mp4";

    //Fill the queue with frames
    struct process_video_args args = {filename, q, queue_size};
    process_video(&args);
    printf("Done processing video\n");

    // Pop frames until we get a valid frame
    frame->format = -1;
    while(frame->format == -1 && avframe_Q_get_size(q) > 0){
        avframe_Q_pop(q, frame);
    }
    if (frame->format == -1){
        printf("Could not find frame with proper format: \n");
        return 1;
    }

    // Create framedata from valid frame
    non_sisci_err = update_framedata_from_avframe(frame, frame_data);
    if (non_sisci_err != 0){
        printf("Error updating framedata from AVFrame: \n");
        return 1;
    }
    printf("Staeting CISCI STUFF\n");

    // Open a virtual device
    SCIOpen(&v_dev, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    // Connect to the remote segment
    SCIConnectSegment(v_dev, &r_seg, RECEIVER_NODE_ID, FRAMEDATA_SEGMENT_ID, ADAPTER_NO, NO_CALLBACK, NO_ARGS, SCI_INFINITE_TIMEOUT, NO_FLAGS ,&err);
    if(err != SCI_ERR_OK){
        printf("Error connecting to remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }
    remote_segment_size = SCIGetRemoteSegmentSize(r_seg);
    remote_address = (volatile FrameData*) SCIMapRemoteSegment(r_seg, &r_map, 0, remote_segment_size, 0, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error mapping remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }


    // Copy framedata to remote segment
    printf("Copying framedata to remote segment\n");
    memcpy((FrameData*)remote_address, frame_data, sizeof(FrameData));
    
    // Flush queue
    non_sisci_err = avframe_Q_flush(q);

    // Unmap and disconnect from remote segment after framedata transfer
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

    // Wait for receiver to finish
    // There is probably a better way of doing this
    sleep(2);
    // Reconnect with new segment size framedata->buffer_size and segment ID DATA_SEGMENT_ID
    SCIConnectSegment(v_dev, &r_seg, RECEIVER_NODE_ID, FRAME_SEGMENT_ID, ADAPTER_NO, NO_CALLBACK, NO_ARGS, SCI_INFINITE_TIMEOUT, NO_FLAGS ,&err);
    if(err != SCI_ERR_OK){
        printf("Error connecting to remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }
    remote_segment_size = SCIGetRemoteSegmentSize(r_seg);
    remote_address = (volatile void*) SCIMapRemoteSegment(r_seg, &r_map, 0, remote_segment_size, 0, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error mapping remote segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //start video processing in a separate thread
    pthread_t video_thread;
    args.stop_threshold = -1;
    if(pthread_create(&video_thread, NULL, process_video, &args)){
        printf("Error creating video processing thread: \n");
        exit(1);
    }
    if(non_sisci_err != 0){
        printf("Error creating video processing thread: \n");
        return 1;
    }

    //Playing frames as we transfer them
    uint32_t delay_time = 0;
    while(1){
        printf("Empty queue, waiting\n");
        //It should wait here until there is a frame to play
        // Possibly redundant double check
        non_sisci_err = avframe_Q_pop(q, frame);
        if (non_sisci_err != 0){
            printf("Error popping AVFrame from queue: \n");
            return 1;
        }
        
        //The first few frames in queue are empty, so we skip them
        //TODO: Fix this
        if(frame->format != -1){
            //Transfer the frame over PCIe:
            printf("Transferring frame\n");

            printf("Before memcpy\n");
            //PCIe transfer
            int test = 0;
            if(!test){
            int bytes_written = av_image_copy_to_buffer((uint8_t*) remote_address, remote_segment_size, (const uint8_t **)frame->data, frame->linesize, frame->format, frame->width, frame->height, 1);
                if (bytes_written < 0) {
                    fprintf(stderr, "Failed to copy AVFrame to buffer\n");
                    continue;
                }
            }
            printf("After memcpy\n");
            
            
            if (test){
                //Create AVFrame from FrameData
                AVFrame* testframe = av_frame_alloc();
                int non_sisci_err = update_avframe_from_framedata(frame_data, testframe, remote_address);
                if(non_sisci_err != 0){
                    printf("Error creating AVFrame from FrameData: \n");
                    return 1;
                }
                else{
                    printf("AVFrame created successfully \n");
                }
                update_videoplayer(&vp, testframe, delay_time);
                delay_time = FRAME_RATE - SDL_GetTicks() % FRAME_RATE;
                av_frame_free(&testframe);
            }

            //Play the frame
            if (!test)
            {
            printf("Playing frame\n");
            update_videoplayer(&vp, frame, delay_time);
            delay_time = FRAME_RATE - SDL_GetTicks() % FRAME_RATE;
            }
            
            
        }
        else{
            printf("Non-valid AVFrame popped from queue: \n");
            }
        
    }
    // Deallocate AVFrame
    av_frame_free(&frame);

    //Clean up
    destroy_videoplayer(&vp);
    
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