#ifndef RECORDER_ENGINE_H
#define RECORDER_ENGINE_H

#include "audio_recorder.h"
#include "freertos/queue.h"
#include "esp_err.h"

/**
 * @brief  Initialize recorder pipeline and speech-recognition engine
 * @param  rec_q  Queue to send REC_START, REC_STOP, REC_CANCEL messages
 */
void recorder_engine_init(QueueHandle_t rec_q);

/**
 * @brief  Start the FreeRTOS task that reads raw voice data
 * @param  rec_q  Queue used by recorder events
 */
void recorder_start_voice_task(QueueHandle_t rec_q);

/**
 * @brief  Audio recorder callback for wake/voice events
 * @param  event     Event data
 * @param  user_data The QueueHandlePtr passed from init
 * @return ESP_OK
 */
esp_err_t recorder_event_cb(audio_rec_evt_t *event, void *user_data);

#endif // RECORDER_ENGINE_H
