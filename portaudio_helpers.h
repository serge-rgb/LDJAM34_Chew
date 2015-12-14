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

/* // Assumed to be stereo at 44100 */
struct SampleQueueItem {
    short* samples;
    int playback_position;
    int num_samples;
};

static const int k_max_samples_queued = 1024;

// Might turn this into a FIFO, but stacks are simpler and game is simple...
struct AudioStack {
    SampleQueueItem items[k_max_samples_queued];
    int count;
};

static AudioStack g_audio_stack;

static void sgl_PA_push_sample(short* samples, int num_samples)
{
    SampleQueueItem it;
    it.samples = samples;
    it.num_samples = num_samples;
    it.playback_position = 0;
    g_audio_stack.items[g_audio_stack.count++] = it;

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
    /* if (g_audio_stack.count == 0) */
    /*     goto end; */

    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */

    if (!g_audio_stack.count) {
        for( i=0; i < framesPerBuffer; i++ ) {
            *out++ = 0;
            *out++ = 0;
        }
    }
    for ( int ai = 0; ai < g_audio_stack.count; ++ ai ) {
        SampleQueueItem* qitem = &g_audio_stack.items[g_audio_stack.count - 1];
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
                    g_audio_stack.count -= 1;  // -1 (the one we consumed)
                    if (g_audio_stack.count == 0) {
                        qitem = NULL;
                    } else {
                        qitem = &g_audio_stack.items[g_audio_stack.count];
                    }
                }
            } else {
                // Probably will cause some clipping, fix.
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
