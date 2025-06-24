#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "esp_audio.h"

/**
 * @brief  Initialize the audio engine (codec, streams)
 * @return Handle to the esp_audio instance
 */
esp_audio_handle_t audio_engine_init(void);

/**
 * @brief  Play a built-in tone (e.g., wake/ding, confirmation)
 * @param  tone_type  Index of tone in tone_uri[]
 */
void audio_engine_play_tone(int tone_type);

#endif // AUDIO_ENGINE_H

