
/*=============================================================================
    recorder_engine.c
=============================================================================*/

#include "recorder_engine.h"
#include "audio_pipeline.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "recorder_sr.h"
#include "audio_recorder.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "sdkconfig.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "recorder_engine";
static audio_rec_handle_t recorder = NULL;
static audio_element_handle_t raw_read = NULL;

// Message IDs, mirror original enum
enum rec_msg_id
{
    REC_START = 1,
    REC_STOP,
    REC_CANCEL,
};

// Voice reading FreeRTOS task
static void voice_read_task(void *args)
{
    QueueHandle_t rec_q = (QueueHandle_t)args;
    const int buf_len = 2 * 1024;
    uint8_t *voiceData = calloc(1, buf_len);
    int msg = 0;
    TickType_t delay = portMAX_DELAY;

    while (true)
    {
        if (xQueueReceive(rec_q, &msg, delay) == pdTRUE)
        {
            switch (msg)
            {
            case REC_START:
                ESP_LOGW(TAG, "voice read begin");
                delay = 0;
                break;
            case REC_STOP:
                ESP_LOGW(TAG, "voice read stopped");
                delay = portMAX_DELAY;
                break;
            case REC_CANCEL:
                ESP_LOGW(TAG, "voice read cancel");
                delay = portMAX_DELAY;
                break;
            default:
                break;
            }
        }
        if (delay == 0)
        {
            int ret = audio_recorder_data_read(recorder, voiceData, buf_len, portMAX_DELAY);
            if (ret <= 0)
            {
                ESP_LOGW(TAG, "audio recorder read finished %d", ret);
                delay = portMAX_DELAY;
            }
            // TODO: send voiceData to WebSocket or file
        }
    }
    free(voiceData);
    vTaskDelete(NULL);
}

void recorder_engine_init(QueueHandle_t rec_q)
{
    // Build pipeline
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&DEFAULT_AUDIO_PIPELINE_CONFIG());
    if (!pipeline)
    {
        ESP_LOGE(TAG, "Failed to init pipeline");
        return;
    }
    // I2S->raw reader
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(
        CONFIG_CODEC_ADC_I2S_PORT,
        CONFIG_CODEC_ADC_SAMPLE_RATE,
        CONFIG_CODEC_ADC_BITS_PER_SAMPLE,
        AUDIO_STREAM_READER);
    audio_element_handle_t i2s_reader = i2s_stream_init(&i2s_cfg);
    audio_pipeline_register(pipeline, i2s_reader, "i2s");
    // Optional resample to 16k
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = CONFIG_CODEC_ADC_SAMPLE_RATE;
    rsp_cfg.dest_rate = 16000;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
    audio_pipeline_register(pipeline, filter, "filter");
    // Raw stream for SR engine
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);
    audio_pipeline_register(pipeline, raw_read, "raw");
    // Link elements: i2s->filter->raw
    const char *link_tag[] = {"i2s", "filter", "raw"};
    audio_pipeline_link(pipeline, link_tag, 3);
    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "Recorder pipeline started");

    // Recorder SR configuration
    recorder_sr_cfg_t sr_cfg = DEFAULT_RECORDER_SR_CFG();
    sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    sr_cfg.afe_cfg.wakenet_init = CONFIG_WAKENET_ENABLE;
    sr_cfg.afe_cfg.vad_mode = VAD_MODE_4;
    sr_cfg.multinet_init = CONFIG_MULTINET_ENABLE;
    // language etc. as original...
    sr_cfg.afe_cfg.aec_init = CONFIG_RECORD_HARDWARE_AEC;
    audio_rec_cfg_t rec_cfg = AUDIO_RECORDER_DEFAULT_CFG();
    rec_cfg.read = (recorder_data_read_t)raw_stream_read;
    rec_cfg.sr_handle = recorder_sr_create(&sr_cfg, &rec_cfg.sr_iface);
    rec_cfg.event_cb = recorder_event_cb;
    rec_cfg.vad_off = 1000;
    recorder = audio_recorder_create(&rec_cfg);
    if (!recorder)
    {
        ESP_LOGE(TAG, "Failed to create recorder");
    }
}

void recorder_start_voice_task(QueueHandle_t rec_q)
{
    xTaskCreate(voice_read_task, "voice_read", 4 * 1024, rec_q, 5, NULL);
}

esp_err_t recorder_event_cb(audio_rec_evt_t *event, void *user_data)
{
    QueueHandle_t rec_q = (QueueHandle_t)user_data;
    int msg = 0;
    switch (event->type)
    {
    case AUDIO_REC_WAKEUP_START:
        ESP_LOGI(TAG, "REC_EVENT_WAKEUP_START");
        audio_engine_play_tone(TONE_TYPE_DINGDONG);
        msg = REC_CANCEL;
        xQueueSend(rec_q, &msg, 0);
        break;
    case AUDIO_REC_VAD_START:
        ESP_LOGI(TAG, "REC_EVENT_VAD_START");
        msg = REC_START;
        xQueueSend(rec_q, &msg, 0);
        break;
    case AUDIO_REC_VAD_END:
        ESP_LOGI(TAG, "REC_EVENT_VAD_STOP");
        msg = REC_STOP;
        xQueueSend(rec_q, &msg, 0);
        break;
    case AUDIO_REC_WAKEUP_END:
        ESP_LOGI(TAG, "REC_EVENT_WAKEUP_END");
        break;
    case AUDIO_REC_COMMAND_DECT:
        ESP_LOGI(TAG, "REC_EVENT_COMMAND_DECT");
        audio_engine_play_tone(TONE_TYPE_HAODE);
        break;
    default:
        ESP_LOGE(TAG, "Unknown rec event %d", event->type);
        break;
    }
    return ESP_OK;
}
