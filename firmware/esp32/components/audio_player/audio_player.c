/*
 * audio_player.c
 *
 *  Created on: 12.03.2017
 *      Author: michaelboeckling
 */


#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "esp_log.h"

#include "audio_player.h"
#include "spiram_fifo.h"
#include "freertos/task.h"
#include "mp3_decoder.h"

#define PRIO_MAD configMAX_PRIORITIES - 2

static const char* TAG = "audio_player";

static int t;
static bool mad_started = false;
/* pushes bytes into the FIFO queue, starts decoder task if necessary */
int audio_stream_consumer(char *recv_buf, ssize_t bytes_read, void *user_data)
{
    player_t *player = user_data;

    // don't bother consuming bytes if stopped
    if(player->state == STOPPED) {
        // TODO: add proper synchronization, this is just an assumption
        mad_started = false;
        return -1;
    }

    if (bytes_read > 0) {
        spiRamFifoWrite(recv_buf, bytes_read);
    }

    int bytes_in_buf = spiRamFifoFill();

    // seems 4k is enough to prevent initial buffer underflow
    if (!mad_started && player->state == PLAYING && (bytes_in_buf > 4096) )
    {
        mad_started = true;
        //Buffer is filled. Start up the MAD task.
        // TODO: 6300 not enough?
        if (xTaskCreatePinnedToCore(&mp3_decoder_task, "tskmad", 8000, player, PRIO_MAD, NULL, 1) != pdPASS)
        {
            ESP_LOGE(TAG, "ERROR creating MAD task! Out of memory?\n");
        } else {
            ESP_LOGD(TAG, "created MAD task\n");
        }
    }


    t = (t+1) & 255;
    if (t == 0) {
        // printf("Buffer fill %d, buff underrun ct %d\n", spiRamFifoFill(), (int)bufUnderrunCt);
        uint8_t fill_level = (bytes_in_buf * 100) / spiRamFifoLen();
        ESP_LOGD(TAG, "Buffer fill %u%%, %d bytes\n", fill_level, bytes_in_buf);
    }

    return 0;
}

void audio_player_init(player_t *player)
{
	audio_renderer_init(player->renderer_config);
}

void audio_player_start(player_t *player)
{
    audio_renderer_start(player->renderer_config);
    player->state = PLAYING;
}

void audio_player_stop(player_t *player)
{
    audio_renderer_stop(player->renderer_config);
    player->state = STOPPED;
}
