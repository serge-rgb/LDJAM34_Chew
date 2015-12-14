#pragma once

#include <limits.h>

#ifndef UNUSED
#define UNUSED(x) (void*)&(x)
#endif

static void sgl_init_PA();

struct paTestData {
    float left_phase;
    float right_phase;
};
static paTestData data;

static PaStream *g_stream;

enum class ItemEndBehavior {
    NEXT_ELEM,
    REPEAT,
};

/* // Assumed to be stereo at 44100 */
struct SampleQueueItem {
    short* samples;
    int playback_position;
    int num_samples;
    ItemEndBehavior behavior;
};

static const int k_max_samples_queued = 1024;

struct AudioQueue {
    SampleQueueItem items[k_max_samples_queued];
    int head;
    int tail;
};

static AudioQueue g_audio_queue;

static void sgl_PA_push_sample(short* samples, int num_samples, int n_loops = 1)
{
    auto add_elem = [&](ItemEndBehavior b) {
        SampleQueueItem it;
        it.samples = samples;
        it.num_samples = num_samples;
        it.playback_position = 0;
        it.behavior = b;
        g_audio_queue.items[g_audio_queue.tail] = it;
        g_audio_queue.tail = g_audio_queue.tail + 1 % k_max_samples_queued;
    };

    for (int i = 0; i < n_loops; ++i) {
        add_elem(ItemEndBehavior::NEXT_ELEM);
    }
    // Forever
    if ( n_loops == -1 ) {
        add_elem(ItemEndBehavior::REPEAT);
    }
}

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
    static float dt = 0.0f;
    /* UNUSED(userData); */
    /* Cast data passed through stream to our structure. */
    paTestData *data = (paTestData*)userData;
    /* if (g_audio_queue.count == 0) */
    /*     goto end; */

    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */

    if (g_audio_queue.tail == g_audio_queue.head) {
        for( i=0; i < framesPerBuffer; i++ ) {
            *out++ = 0;
            *out++ = 0;
        }
    } else { //  }for ( int ai = 0; ai < g_audio_queue.count; ++ ai ) {
        SampleQueueItem* qitem = &g_audio_queue.items[g_audio_queue.head];
        for( i=0; i < framesPerBuffer; i++ ) {
            if (qitem && qitem->num_samples) {
                auto* samples = qitem->samples;
                short left16  = samples[qitem->playback_position++];
                short right16 = samples[qitem->playback_position++];

                float left = (float)left16 / (1 << 16);
                float right = (float)right16 / (1 << 16);

                *out++ = left;
                *out++ = right;

                assert  (qitem->num_samples*2 >= qitem->playback_position);
                if ( qitem->num_samples*2 == qitem->playback_position ) {
                    // Consumed one
                    switch (qitem->behavior) {
                    case ItemEndBehavior::NEXT_ELEM: {
                            g_audio_queue.head = (g_audio_queue.head + 1) % k_max_samples_queued;
                            if (g_audio_queue.head == g_audio_queue.tail) {
                                qitem = NULL;
                            } else {
                                qitem = &g_audio_queue.items[g_audio_queue.head];
                            }
                        }
                    case ItemEndBehavior::REPEAT: {
                        qitem->playback_position = 0;
                    }
                    }
                }
            } else {
                // Probably will cause some clipping, fix???
                *out++ = 0;
                *out++ = 0;
            }
        }
    }
end:
    return 0;
}

static void sgl_init_PA()
{
    PaError err;

    printf("PortAudio Test: output sawtooth wave.\n");
    /* Initialize our data for use by callback. */
    data.left_phase = data.right_phase = 0.0;
    /* Initialize library before making any other calls. */
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
                               &data);
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

static void sgl_deinit_PA()
{
    PaError err = paNoError;
    err = Pa_StopStream( g_stream );
    if( err != paNoError ) goto error;
    err = Pa_CloseStream( g_stream );
    if( err != paNoError ) goto error;
error:
    Pa_Terminate();
}
