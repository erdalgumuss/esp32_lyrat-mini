
/*=============================================================================
    audio_engine.c
=============================================================================*/

#include "audio_engine.h"
#include "audio_tone_uri.h"
#include "board.h"
#include "esp_log.h"
#include "tone_stream.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "sdkconfig.h"

static const char *TAG = "audio_engine";
static esp_audio_handle_t player = NULL;

esp_audio_handle_t audio_engine_init(void)
{
    if (player)
        return player;

    // Core audio configuration
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 48000;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;

    // Create esp_audio instance
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    // --- Input: Tone reader ---
    tone_stream_cfg_t tone_cfg = TONE_STREAM_CFG_DEFAULT();
    tone_cfg.type = AUDIO_STREAM_READER;
    esp_audio_input_stream_add(player, tone_stream_init(&tone_cfg));

    // --- Decoder: MP3 ---
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg.task_core = 1;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_cfg));

    // --- Output: I2S writer ---
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(
        I2S_NUM_0,
        48000,
        CODEC_ADC_BITS_PER_SAMPLE,
        AUDIO_STREAM_WRITER);
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);
    esp_audio_output_stream_add(player, i2s_writer);

    // Default volume
    esp_audio_vol_set(player, 60);
    ESP_LOGI(TAG, "audio engine initialized: %p", player);

    return player;
}

void audio_engine_play_tone(int tone_type)
{
    if (!player)
        return;
    if (tone_type < 0 || tone_type >= TONE_TYPE_MAX)
        return;
    esp_audio_sync_play(player, tone_uri[tone_type], 0);
}
