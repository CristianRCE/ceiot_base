/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

// MACROS PARA EL LED RGB
#define LED_RGB_PIN 48
#define LED_NUMBERS 1
#define RESOLUTION_HZ 10000000 //10MHz
#define BLOCK_MEM 64
#define TRANS_QUEUE 4
#define SRC_CLK RMT_CLK_SRC_DEFAULT

// MACRO PARA TIEMPO DE PARPADEO
#define BLINK_PERIOD_ON_MS 1000
#define BLINK_PERIOD_OFF_MS 10000

// VARIABLE PARA EL LOGI
static const char *TAG = "example";
static uint8_t led_pixels[LED_NUMBERS*3]; // cada color necesita 3 bytes

void app_main(void)
{   
    // 1. CONFIGURAMOS Y CREAMOS EL CANAL                                     
    ESP_LOGI(TAG,"Creando el canal TX RGB");
    rmt_tx_channel_config_t rmt_config = 
    {
        .gpio_num=LED_RGB_PIN,
        .clk_src=SRC_CLK,
        .resolution_hz=RESOLUTION_HZ,  
        .mem_block_symbols=BLOCK_MEM,
        .trans_queue_depth=TRANS_QUEUE,
    };

    rmt_channel_handle_t rmt_channel_handle = NULL;
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&rmt_config,&rmt_channel_handle));

    // 2. CONFIGURO Y CREO EL ENCODER DE LA TIRA DE LED
    ESP_LOGI(TAG,"Inicializando encoder");

    led_strip_encoder_config_t encoder_config = 
    {
        .resolution=RESOLUTION_HZ,
    };

    rmt_encoder_handle_t rmt_encoder_handle=NULL;

    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config,&rmt_encoder_handle));
    
    // 3. HABILITAMOS EL CANAL
    ESP_LOGI(TAG,"HABILITAMOS EL CANAL");

    ESP_ERROR_CHECK(rmt_enable(rmt_channel_handle));

    // Configuración de transmisicón estándar??
    rmt_transmit_config_t ts_config =
    {
        .loop_count=0,
    };

    while(1)
    {
        // PREPARAMOS EL COLOR A TRANSMITIR
        // Formato GRB: posición 0 es verde.
        led_pixels[0]=255;
        led_pixels[1]=0;
        led_pixels[2]=0;

        // Transmito el color verde por 1 segundo. 
        rmt_transmit(rmt_channel_handle,rmt_encoder_handle,&led_pixels,sizeof(led_pixels),&ts_config);
        rmt_tx_wait_all_done(rmt_channel_handle,portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_ON_MS));

        // Transmito el color nulo por 10 segundos
        // Formato GRB: posición 0 es verde.
        led_pixels[0]=0;
        led_pixels[1]=0;
        led_pixels[2]=0;

        rmt_transmit(rmt_channel_handle,rmt_encoder_handle,&led_pixels,sizeof(led_pixels),&ts_config);
        rmt_tx_wait_all_done(rmt_channel_handle,portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_OFF_MS));
    }
}