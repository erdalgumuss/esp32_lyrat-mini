#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_periph.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "board.h"

#include "audio_engine.h"
#include "recorder_engine.h"

static const char *TAG = "main";
static QueueHandle_t rec_q = NULL;

// Gereksiz logları susturur, sadece önemli mesajlar kalır
static void log_clear(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("main", ESP_LOG_INFO); // Sadece bizim TAG INFO kalsın
}

// Tuşa basıldığında kayıt başlatma, bırakıldığında durdurma
static esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    if (event->source_type == PERIPH_ID_BUTTON)
    {
        int button_id = (int)event->data;
        if (button_id == get_input_rec_id())
        {
            if (event->cmd == PERIPH_BUTTON_PRESSED)
            {
                ESP_LOGI(TAG, "Button pressed - recording start");
                recorder_trigger_start(); // Sesli kayıt başlat
            }
            else if (event->cmd == PERIPH_BUTTON_RELEASE ||
                     event->cmd == PERIPH_BUTTON_LONG_RELEASE)
            {
                ESP_LOGI(TAG, "Button released - recording stop");
                recorder_trigger_stop(); // Sesli kayıt durdur
            }
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    log_clear();

    // 1. Tuşlar ve SD kart için peripheral set oluştur
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.extern_stack = true;
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // 2. Giriş birimleri (tuş, sd kart, led) başlat
#if CONFIG_ENABLE_SD_CARD
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
#endif
    audio_board_key_init(set); // ADC tuşlar
    audio_board_init();        // Codec ve pin setup

    // 3. Tuş olaylarını dinlemek için callback kaydet
    esp_periph_set_register_callback(set, periph_callback, NULL);

    // 4. Ses oynatma motoru başlat
    esp_audio_handle_t player = audio_engine_init();
    if (!player)
    {
        ESP_LOGE(TAG, "Audio engine init failed");
        return;
    }

    // 5. Kayıt motoru başlat
    rec_q = xQueueCreate(3, sizeof(int));
    recorder_engine_init(rec_q);
    recorder_start_voice_task(rec_q);

    ESP_LOGI(TAG, "Sistem hazır. Tuşa basıldığında kayıt başlayacak.");

    // 6. Sonsuz döngü (gelecekte uyku modu burada tetiklenebilir)
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
