
#include <portaudio.h>

#include <limits.h>

enum class ItemEndBehavior {
    NEXT_ELEM,
    REPEAT,
};

/* // Assumed to be stereo at 44100 */
struct SampleQueueItem {
    short* samples;
    int playback_position;
    int num_samples;
    ItemEndBehavior end_behavior;
};

static const int k_num_audio_queues   = 4;  // How many simultaneous audio items in flight.
static const int k_max_audio_items_queued = 32;

struct AudioQueue {
    SampleQueueItem items[k_max_audio_items_queued];
    int head;
    int tail;
};
static AudioQueue g_audio_queues[k_num_audio_queues];
static PaStream *g_stream;

/* This routine will be called by the PortAudio engine when audio is needed.
 ** It may called at interrupt level on some machines so don't do anything
 ** that could mess up the system like calling malloc() or free().
 */
static int sgl_PA_Callback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */

    // Clear out.
    for( i=0; i < framesPerBuffer; i++ ) {
        *(out + 2*i)     = 0;
        *(out + 2*i + 1) = 0;
    }

    for (int aq_i = 0; aq_i < k_num_audio_queues; ++aq_i) {
        auto& audio_queue = g_audio_queues[aq_i];
        if (audio_queue.tail != audio_queue.head) {
            SampleQueueItem* qitem = &audio_queue.items[audio_queue.head];
            for( i=0; i < framesPerBuffer; i++ ) {
                if (qitem && qitem->num_samples) {
                    auto* samples = qitem->samples;
                    short left16  = samples[qitem->playback_position++];
                    short right16 = samples[qitem->playback_position++];

                    float left = (float)left16 / (1 << 16);
                    float right = (float)right16 / (1 << 16);

                    *(out + 2*i)   += left;
                    *(out + 2*i+1) += right;

                    assert  (qitem->num_samples*2 >= qitem->playback_position);
                    if ( qitem->num_samples*2 == qitem->playback_position ) {
                        // Consumed one
                        switch (qitem->end_behavior) {
                        case ItemEndBehavior::NEXT_ELEM: {
                            audio_queue.head = (audio_queue.head + 1) % k_max_audio_items_queued;
                            if (audio_queue.head == audio_queue.tail) {
                                qitem = NULL;
                            } else {
                                qitem = &audio_queue.items[audio_queue.head];
                            }
                        } break;
                        case ItemEndBehavior::REPEAT: {
                            qitem->playback_position = 0;
                        } break;
                        }
                    }
                } else {
                    *(out + 2*i)   = 0;
                    *(out + 2*i+1) = 0;
                }
            }
        }
    }
end:
    return 0;
}

void audio_push_sample(int queue_i, short* samples, int num_samples, int n_loops)
{
    auto add_elem = [&](ItemEndBehavior b) {
        SampleQueueItem it;
        it.samples = samples;
        it.num_samples = num_samples;
        it.playback_position = 0;
        it.end_behavior = b;
        g_audio_queues[queue_i].items[g_audio_queues[queue_i].tail] = it;
        g_audio_queues[queue_i].tail = g_audio_queues[queue_i].tail + 1 % k_max_audio_items_queued;
    };

    for (int i = 0; i < n_loops; ++i) {
        add_elem(ItemEndBehavior::NEXT_ELEM);
    }
    // Forever
    if ( n_loops == -1 ) {
        add_elem(ItemEndBehavior::REPEAT);
    }
}

void audio_init()
{
    PaError err;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream(&g_stream,
                               0,          /* no input channels */
                               2,          /* stereo output */
                               paFloat32,  /* 32 bit floating point output */
                               44100,
                               256,        /* frames per buffer */
                               sgl_PA_Callback,
                               NULL);
    if( err != paNoError ) goto error;

    err = Pa_StartStream( g_stream );
    if( err != paNoError ) goto error;

    return;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    die_gracefully("something went wrong initting pulse audio\n");
}

void audio_deinit()
{
    PaError err = paNoError;
    err = Pa_StopStream( g_stream );
    if( err != paNoError ) goto error;
    err = Pa_CloseStream( g_stream );
    if( err != paNoError ) goto error;
error:
    Pa_Terminate();
}
