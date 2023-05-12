// import system and application specific headers
#include <stdio.h>


// include the header file for the module for compiler checking
// the IMPORT macro is used to prevent the compiler from generating
// multiple definitions of the module's public functions
#define Process_video_IMPORT
#include "process_video.h"

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
};

//Idk if this is useful?
void av_frame_deep_copy(AVFrame* dest, const AVFrame* src) {
    dest->format = src->format;
    dest->width = src->width;
    dest->height = src->height;
    dest->channels = src->channels;
    dest->channel_layout = src->channel_layout;
    dest->nb_samples = src->nb_samples;

    av_frame_get_buffer(dest, 32);
    av_frame_copy(dest, src);
    av_frame_copy_props(dest, src);
}

int ffmpeg_init(){
    //Initialize the FFMPEG library. Deprecation issues?
    av_register_all();
    avcodec_register_all();
    return 0;
}

// publicly accessible functions can be implemented here:

    /*
    * Initialize the queue.
    */
AVFrame_Q* avframe_Q_alloc(int capacity) {
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
void avframe_Q_destroy(AVFrame_Q* q) {
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
   int avframe_Q_push(AVFrame_Q* q, AVFrame* frame) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) {
        printf("Queue is full, at %i\n", q->size);
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    //This aint gonna work
    //Cant allocate without deallocating
    //Work on deep copying into q-rear instead? Same principle as av_frame_copy

    //Ok gringo what about this then:
    av_frame_deep_copy(q->frames[q->rear], frame);

    q->rear = (q->rear + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

//OLD, shallow copies?
/*int avframe_Q_push(AVFrame_Q* q, AVFrame* frame) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    av_frame_copy(q->frames[q->rear], frame);
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}*/

    /*
    * Pop a frame from the queue.
    * Return NULL if the queue is empty.
    */
int avframe_Q_pop(AVFrame_Q* q, AVFrame* dest) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    av_frame_deep_copy(dest, q->frames[q->rear]);
    q->front = (q->front + 1) % q->capacity;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

    /*
    * Get the size of the queue.
    */
int avframe_Q_get_size(AVFrame_Q* q) {
    int size = 0;
    pthread_mutex_lock(&q->mutex);
    size = q->size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

int avframe_Q_flush(AVFrame_Q* q) {
    pthread_mutex_lock(&q->mutex);
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

/*
* Function for breaking down the video into frames.
* The frames are put into a queue.
*/

void *process_video(void* arg){

    // Cast the argument to the correct type
    struct process_video_args* args = (struct process_video_args*) arg;
    // Local variables
    AVFormatContext* format_ctx = NULL;
    AVCodec* codec = NULL;

    //Initialize the FFMPEG library. Deprecation issues?
    av_register_all();
    avcodec_register_all();

    /*
    * Get the format of the file.
    */

    int ret = avformat_open_input(&format_ctx, args->filename, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open input file: %s\n", av_err2str(ret));
        return NULL;
    }

    /*
    * Get the stream information.
    */
   ret = avformat_find_stream_info(format_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to read stream information: %s\n", av_err2str(ret));
        avformat_close_input(&format_ctx);
        return NULL;
    }

    /*
    * Find correct codec for media format.
    * Video stream index is stored in stream_index.
    */
    int stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        fprintf(stderr, "Failed to find video stream: %s\n", av_err2str(stream_index));
        avformat_close_input(&format_ctx);
        return NULL;
    }

    /*
    */
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context.\n");
        avformat_close_input(&format_ctx);
        return NULL;
    }

    /*
    * Copy codec parameters from input stream to output codec context.
    */
    ret = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec parameters to codec context: %s\n", av_err2str(ret));
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return NULL;
    }

    /*
    * Open the codec.
    */
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open codec: %s\n", av_err2str(ret));
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return NULL;
    }

    /*
    * Allocate a frame for the decoded video.
    * Uncompressed frame.
    */

   AVFrame* frame = av_frame_alloc();
        if (!frame) {
            fprintf(stderr, "Failed to allocate frame.\n");
            return NULL;
        }
    
    int frameFinished = 0;
    //This method will eventually lead to integer overflow
    while(args->stop_threshold != 0){
        /*
        * Allocated and deallocated for each frame.
        */
        AVPacket packet;

        if (av_read_frame(format_ctx, &packet) >= 0) {
        
            // Check if the packet is from the video stream
            // Kind of redundant since we already determined the stream index
            if (packet.stream_index == stream_index) {
                // Decode the video frame
                avcodec_decode_video2(codec_ctx, frame, &frameFinished, &packet);

                // If we got a frame, process it
                if (frameFinished && frame->format != -1) {
                    // Do something with the frame here

                    // Push the frame into the queue
                    // This is a blocking call
                    // This is also atomic
                    if(avframe_Q_push(args->q, frame)){
                        printf("Failed to push frame into queue\n");
                    }

                    args->stop_threshold--;
                }
            }

        // Free the packet
        av_packet_unref(&packet);
        frameFinished = 0;
        }
        else {
                // Video ended, skip backwards until the end of the video (hopefully)
                printf("Video ended\n");
                av_seek_frame(format_ctx, -1, 10000, AVSEEK_FLAG_BACKWARD);
        }
    }
    // Free the frame and the codec context
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    return NULL;
}