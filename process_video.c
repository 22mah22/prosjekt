// import system and application specific headers
#include <stdio.h>


// include the header file for the module for compiler checking
// the IMPORT macro is used to prevent the compiler from generating
// multiple definitions of the module's public functions
#define Process_video_IMPORT
#include "process_video.h"


// private variables are defined using static:
static uint8_t nodeID = 4;

// publicly accessible but opaque data structure:
struct AVFrame_Q{
    AVFrame** frames;
    int capacity;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
}


// private functions are defined using static:
static void Process_video_private_function(void) {
    printf("test");
}


// publicly accessible functions can be implemented here:

    /*
    * Initialize the queue.
    */
AVFrame_Q* avframe_queue_alloc(AVFrameQueue* q, int capacity) {
    AVFrame_Q* q = malloc(sizeof(AVFrame_Q));
    if (!q) {
        return NULL;
    }

    q->frames = malloc(capacity * sizeof(AVFrame*));
    if (!q->frames) {
        free(q);
        return NULL;
    }

    for (int i = 0; i < capacity; i++) {
        q->frames[i] = av_frame_alloc();
        if (!q->frames[i]) {
            for (int j = 0; j < i; j++) {
                av_frame_free(&q->frames[j]);
            }
            free(q->frames);
            free(q);
            return NULL;
        }
    }

    q->capacity = capacity;
    q->size = 0;
    q->front = 0;
    q->rear = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);

    return q;
}

    /*
    * Destroy the queue.
    */
void avframe_queue_destroy(AVFrameQueue* q) {
    for (int i = 0; i < q->capacity; i++) {
        av_frame_free(&q->frames[i]);
    }
    free(q->frames);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q);
}

    /*
    * Push a frame into the queue.
    * Return 0 if successful, -1 if the queue is full.
    */
int avframe_queue_push(AVFrameQueue* q, AVFrame* frame) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    av_frame_copy(q->frames[q->rear], frame);
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

    /*
    * Pop a frame from the queue.
    * Return NULL if the queue is empty.
    */
int avframe_queue_pop(AVFrame_Q* q, AVFrame* dest) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    memcpy(dest, q->frames[q->front], sizeof(AVFrame));
    q->front = (q->front + 1) % q->capacity;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

    /*
    * Get the size of the queue.
    */
int avframe_queue_get_size(AVFrameQueue* q) {
    int size = 0;
    pthread_mutex_lock(&q->mutex);
    size = q->size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

/*
* Function for breaking down the video into frames.
* The frames are put into a queue.
*/

void process_video(void* arg){
    // Cast the argument to the correct type
    struct process_video_args* args = (struct process_video_args*) arg;
    // Local variables
    AVFormatContext* format_ctx = NULL;
    AVCodec* codec = NULL;

    //Initialize the FFMPEG library. Deprecation issues?
    avformat_network_init();
    avcodec_register_all();

    /*
    * Get the format of the file.
    */
    int ret = avformat_open_input(&format_ctx, args->filename, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open input file: %s\n", av_err2str(ret));
        return;
    }

    /*
    * Get the stream information.
    */
   ret = avformat_find_stream_info(format_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to read stream information: %s\n", av_err2str(ret));
        avformat_close_input(&format_ctx);
        return;
    }

    /*
    * Find correct codec for media format.
    * Video stream index is stored in stream_index.
    */
    int stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        fprintf(stderr, "Failed to find video stream: %s\n", av_err2str(stream_index));
        avformat_close_input(&format_ctx);
        return;
    }

    /*
    */
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context.\n");
        avformat_close_input(&format_ctx);
        return;
    }

    /*
    * Copy codec parameters from input stream to output codec context.
    */
    ret = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec parameters to codec context: %s\n", av_err2str(ret));
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return;
    }

    /*
    * Open the codec.
    */
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open codec: %s\n", av_err2str(ret));
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return;
    }

    /*
    * Allocate a frame for the decoded video.
    * Uncompressed frame.
    */

   AVFrame* pFrame = av_frame_alloc();
        if (!pFrame) {
            fprintf(stderr, "Failed to allocate frame.\n");
            return NULL;
        }
    
    // Cancel condition is a WIP and is not used yet
    int cancel_condition = 0;
    int frameFinished = 0;
    while(cancel_condition != -1){
        /*
        * Allocated and deallocated for each frame.
        */
        AVPacket packet;

        if (av_read_frame(format_ctx, &packet) >= 0) {
        
            // Check if the packet is from the video stream
            // Kind of redundant since we already determined the stream index
            if (packet.stream_index == stream_index) {
                // Decode the video frame
                avcodec_decode_video2(codec_ctx, pFrame, &frameFinished, &packet);

                // If we got a frame, process it
                if (frameFinished) {
                    // Do something with the frame here

                    // Push the frame into the queue
                    // This is a blocking call
                    // This is also atomic
                    avframe_queue_push(args->q, pFrame);
                }
            }

        // Free the packet
        av_packet_unref(&packet);
        frameFinished = 0;
        }
        else {
                // Video ended, skip backwards until the end of the video (hopefully)
                av_seek_frame(pFormatCtx, -1, 10000, AVSEEK_FLAG_BACKWARD);
        }
    }
    // Free the frame and the codec context
    av_frame_free(&pFrame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
}