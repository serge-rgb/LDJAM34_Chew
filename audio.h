#pragma once

void audio_init();
void audio_push_sample(short* samples, int num_samples, int n_loops = 1);
void audio_deinit();
