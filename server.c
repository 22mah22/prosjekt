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
#define FRAMEDATA_SEGMENT_ID 0
#define LOCAL_NODE_ID 4
#define FRAME_SEGMENT_ID 1
#define RECEIVER_NODE_ID 8
#define RECEIVER_SEGMENT_ID 4
#define ADAPTER_NO 0
#define NO_FLAGS 0
#define NO_ARGS 0
#define NO_CALLBACK 0

//Global variables
sci_error_t err;


sci_desc_t v_dev_frame;
sci_local_segment_t l_seg_frame;
sci_remote_segment_t r_seg_frame;
sci_map_t l_map_frame;
sci_map_t r_map_frame;
volatile void* map_addr_frame;

sci_desc_t v_dev_framedata;
sci_local_segment_t l_seg_framedata;
sci_remote_segment_t r_seg_framedata;
sci_map_t l_map_framedata;
sci_map_t r_map_framedata;
volatile void* map_addr_framedata;

volatile void* write_address_framedata;
volatile void* write_address_frame;

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
    int queue_size = 30;
    AVFrame_Q* q = avframe_Q_alloc(queue_size);

    //TEST STUFF
    AVFrame_Q* q_2 = avframe_Q_alloc(10);


    if(!q){
        printf("Error allocating AVFrame queue: \n");
        return 1;
    }

    //Set filename
    char* filename = "test.webm";

    //Fill the queue with frames
    struct process_video_args args = {filename, q_2, 10};
    process_video(&args);
    printf("Done pre-processing video\n");

    // Pop frames until we get a valid frame
    frame->format = -1;
    while(frame->format == -1 && avframe_Q_get_size(q_2) > 0){
        avframe_Q_pop(q_2, frame);
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
    printf("Starting SISCI transfer\n");



    // Open two virtual devices for Framedata and frame transfer

    SCIOpen(&v_dev_framedata, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Create a local segment
    SCICreateSegment(v_dev_framedata, &l_seg_framedata, FRAMEDATA_SEGMENT_ID, SEGMENT_SIZE, NO_CALLBACK, NO_ARGS, SCI_FLAG_BROADCAST, &err);
    if(err != SCI_ERR_OK){
        printf("Error creating local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Prepare the segment for use
    SCIPrepareSegment(l_seg_framedata, ADAPTER_NO, SCI_FLAG_BROADCAST, &err);
    if (err == SCI_ERR_OK){
        //Map the segment into the virtual address space
        map_addr_framedata = SCIMapLocalSegment(l_seg_framedata, &l_map_framedata, 0, SEGMENT_SIZE, NULL, NO_FLAGS, &err);
    }
    else{
        printf("Error preparing local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Set the segment available
    SCISetSegmentAvailable(l_seg_framedata, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment available: %s\n", SCIGetErrorString(err));
        return 1;
    }

        /* Connect to remote segment */
    printf("Connect to remote segment .... ");

    do { 
        SCIConnectSegment(v_dev_framedata,
                          &r_seg_framedata,
                          DIS_BROADCAST_NODEID_GROUP_ALL,
                          FRAMEDATA_SEGMENT_ID,
                          ADAPTER_NO,
                          NO_CALLBACK,
                          NULL,
                          SCI_INFINITE_TIMEOUT,
                          SCI_FLAG_BROADCAST,
                          &err);

        SleepMilliseconds(10);

    } while (err != SCI_ERR_OK);

    printf("Remote segment (id=0x%x) is connected.\n", FRAMEDATA_SEGMENT_ID);

     /* Map remote segment to user space */
    write_address_framedata = SCIMapRemoteSegment(r_seg_framedata,&r_map_framedata,0, SEGMENT_SIZE ,NULL,SCI_FLAG_BROADCAST,&err);
    if (err == SCI_ERR_OK) {
        printf("Remote segment (id=0x%x) is mapped to user space. \n", FRAMEDATA_SEGMENT_ID);         
    } else {
        fprintf(stderr,"SCIMapRemoteSegment failed - Error code 0x%x\n",err);
        return err;
    } 

    //Transfer framedata info
    // Copy framedata to remote segment
    printf("Copying framedata to remote segment\n");
    memcpy((FrameData*)write_address_framedata, frame_data, sizeof(FrameData));
    
    // Flush queue
    //non_sisci_err = avframe_Q_flush(q);


    // Create frame SISCI segment

    SCIOpen(&v_dev_frame, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error opening virtual device: %s\n", SCIGetErrorString(err));
        return 1;
    }

    // Print the buffer size
    printf("Buffer size: %d\n", frame_data->buffer_size);
    //Create a local segment
    SCICreateSegment(v_dev_frame, &l_seg_frame, FRAME_SEGMENT_ID, frame_data->buffer_size, NO_CALLBACK, NO_ARGS, SCI_FLAG_BROADCAST, &err);
    if(err != SCI_ERR_OK){
        printf("Error creating local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Prepare the segment for use
    SCIPrepareSegment(l_seg_frame, ADAPTER_NO, SCI_FLAG_BROADCAST, &err);
    if (err == SCI_ERR_OK){
        //Map the segment into the virtual address space
        map_addr_frame = SCIMapLocalSegment(l_seg_frame, &l_map_frame, 0, frame_data->buffer_size, NULL, NO_FLAGS, &err);
    }
    else{
        printf("Error preparing local segment: %s\n", SCIGetErrorString(err));
        return 1;
    }

    //Set the segment available
    SCISetSegmentAvailable(l_seg_frame, ADAPTER_NO, NO_FLAGS, &err);
    if(err != SCI_ERR_OK){
        printf("Error setting segment available: %s\n", SCIGetErrorString(err));
        return 1;
    }

    /* Connect to remote segment */
    printf("Connect to remote segment .... ");

    do { 
        SCIConnectSegment(v_dev_frame,
                          &r_seg_frame,
                          DIS_BROADCAST_NODEID_GROUP_ALL,
                          FRAME_SEGMENT_ID,
                          ADAPTER_NO,
                          NO_CALLBACK,
                          NULL,
                          SCI_INFINITE_TIMEOUT,
                          SCI_FLAG_BROADCAST,
                          &err);

        SleepMilliseconds(10);

    } while (err != SCI_ERR_OK);

    printf("Remote segment (id=0x%x) is connected.\n", FRAME_SEGMENT_ID);

     /* Map remote segment to user space */
    write_address_frame = SCIMapRemoteSegment(r_seg_frame,&r_map_frame,0, frame_data->buffer_size ,NULL,SCI_FLAG_BROADCAST,&err);
    if (err == SCI_ERR_OK) {
        printf("Remote segment (id=0x%x) is mapped to user space. \n", FRAME_SEGMENT_ID);         
    } else {
        fprintf(stderr,"SCIMapRemoteSegment failed - Error code 0x%x\n",err);
        return err;
    } 

    //start video processing in a separate thread
    pthread_t video_thread;
    args.q = q;
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
    uint32_t delay_time = FRAME_DELAY;
    int frame_end_time = 0;
    int test = 0;
    while(1){
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
            // First update frame_data and transfer that
            non_sisci_err = update_framedata_from_avframe(frame, frame_data);
            if (non_sisci_err != 0){
                printf("Error updating framedata from AVFrame: \n");
                return 1;
            }
            memcpy((FrameData*)write_address_framedata, frame_data, sizeof(FrameData));

            // Todo: Check if the segment size has changed
            // Re-connect to the segment with the correct size if it has changed
            // For now, this is not feasible since only single video transfer is supported

            //Transfer the frame over PCIe:
            printf("Transferring frame\n");
            if(!test){
            int bytes_written = av_image_copy_to_buffer((uint8_t*) write_address_frame, frame_data->buffer_size , (const uint8_t **)frame->data, frame->linesize, frame->format, frame->width, frame->height, 1);
                if (bytes_written < 0) {
                    fprintf(stderr, "Failed to copy AVFrame to buffer\n");
                    continue;
                }
            }
            printf("After memcpy\n");
            
            /*
            if (test){
                //Create AVFrame from FrameData
                AVFrame* testframe = av_frame_alloc();
                int non_sisci_err = update_avframe_from_framedata(frame_data, testframe, write_address_frame);
                if(non_sisci_err != 0){
                    printf("Error creating AVFrame from FrameData: \n");
                    return 1;
                }
                else{
                    printf("AVFrame created successfully \n");
                }
                update_videoplayer(&vp, testframe, delay_time);
                delay_time = FRAME_DELAY - SDL_GetTicks() % FRAME_DELAY;
                av_frame_free(&testframe);
            }
            */
            //Play the frame
            if (!test)
            {
            printf("Playing frame\n");
            delay_time = FRAME_DELAY - (SDL_GetTicks()-frame_end_time)% FRAME_DELAY;
            update_videoplayer(&vp, frame, delay_time);
            frame_end_time = SDL_GetTicks();
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
    
    // For frame
    /* Unmap local segment */
    SCIUnmapSegment(l_map_framedata,NO_FLAGS,&err);
    
    if (err == SCI_ERR_OK) {
        printf("The local segment is unmapped\n"); 
    } else {
        fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",err);
        return err;
    }
    
    /* Remove local segment */
    SCIRemoveSegment(l_seg_framedata,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The local segment is removed\n"); 
    } else {
        fprintf(stderr,"SCIRemoveSegment failed - Error code 0x%x\n",err);
        return err;
    } 
    
    /* Unmap remote segment */
    SCIUnmapSegment(r_map_framedata,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The remote segment is unmapped\n"); 
    } else {
        fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",err);
        return err;
    }
    
    /* Disconnect segment */
    SCIDisconnectSegment(r_seg_framedata,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The segment is disconnected\n"); 
    } else {
        fprintf(stderr,"SCIDisconnectSegment failed - Error code 0x%x\n",err);
        return err;
    } 

    // FOr frame
    /* Unmap local segment */
    SCIUnmapSegment(l_map_frame,NO_FLAGS,&err);
    
    if (err == SCI_ERR_OK) {
        printf("The local segment is unmapped\n"); 
    } else {
        fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",err);
        return err;
    }
    
    /* Remove local segment */
    SCIRemoveSegment(l_seg_frame,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The local segment is removed\n"); 
    } else {
        fprintf(stderr,"SCIRemoveSegment failed - Error code 0x%x\n",err);
        return err;
    } 
    
    /* Unmap remote segment */
    SCIUnmapSegment(r_map_frame,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The remote segment is unmapped\n"); 
    } else {
        fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",err);
        return err;
    }
    
    /* Disconnect segment */
    SCIDisconnectSegment(r_seg_frame,NO_FLAGS,&err);
    if (err == SCI_ERR_OK) {
        printf("The segment is disconnected\n"); 
    } else {
        fprintf(stderr,"SCIDisconnectSegment failed - Error code 0x%x\n",err);
        return err;
    } 

    SCITerminate();
    return 0;
}