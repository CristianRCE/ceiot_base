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
#include "i2cdev.h"
#include "bmp280.h"
#include "esp_wifi.h"
#include "esp_event.h" // para el bucle de eventos
#include "nvs_flash.h"
#include "esp_netif.h"

// MACROS PARA EL LED RGB
#define LED_RGB_PIN 48
#define LED_NUMBERS 1
#define RESOLUTION_HZ 10000000 //10MHz
#define BLOCK_MEM 64
#define TRANS_QUEUE 4
#define SRC_CLK RMT_CLK_SRC_DEFAULT

// MACRO PARA TIEMPO DE PARPADEO
#define BLINK_PERIOD_ON_MS 1000
#define BLINK_PERIOD_OFF_MS 1000

// PINES PARA EL BMP280
#define I2C_MASTER_SDA 4
#define I2C_MASTER_SCL 5

//MACROS PARA EL WI-FI
#define WIFI_SSID "WifiNet_Digital_Aguilera-2.4G"
#define WIFI_PASSWORD "AguileraY287"
#define WIFI_RETRY 5

// VARIABLE PARA EL LOGI
static const char *TAG = "example";
static uint8_t led_pixels[LED_NUMBERS*3]; // cada color necesita 3 bytes

// PROTOTIPOS DE LAS FUNCIONES
void wifi_init_sta(void);
static void event_handler(void *arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data
                            );

// VARIABLES GLOBALES
static int s_retry_num=0; // contador de antidad de intentos.



void app_main(void)
{   
    /** 
    // --------------------- RGB ----------------------//
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

    // --------------------- BMP280 ----------------------//

    ESP_LOGI(TAG,"Iniciando configuración del BMP280");

    // Inicializamos el bmp280
    ESP_ERROR_CHECK(i2cdev_init());

    // Estructura de configuración del sensor:
    bmp280_t dev;
    bmp280_params_t params;

    bmp280_init_default_params(&params);
    memset(&dev,0,sizeof(bmp280_t));

    // Inicializamos el descriptor conectando los pines SDA y SCL.
    ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_0,0,I2C_MASTER_SDA,I2C_MASTER_SCL));
    ESP_ERROR_CHECK(bmp280_init(&dev,&params));

    ESP_LOGI(TAG,"Sensor inicializado correctamente! chip conectado con ID: 0x%02X",dev.id);

    float pressure = 0.0f;
    float temperature = 0.0f;
    */
    // Inicializaciones:

    // Inicialización de NVS Flash
    esp_err_t ret_nvs_flash =  nvs_flash_init();
    if(ret_nvs_flash!=ESP_OK)
    {
        return;
    }
    // Inicialización de Wi-Fi
    wifi_init_sta();

    while(1)
    {
        /**
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

        //----------------- LECTURA BMP280----------------------//

        if(bmp280_read_float(&dev,&temperature,&pressure,NULL) == ESP_OK)
        {
            ESP_LOGI(TAG,"Temperatura: %.2f °C | Presión: %.2f Pa",temperature,pressure);
        }
        else
        {
            ESP_LOGE(TAG,"Error al leer del BMP280 en el bus I2C");
        }
        */
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        
    }
};

/** 
 * Esta función no se llama en el código principal. La llama el bucle de eventos del
 * esp32/FreeRTOS cuando ocurre algo a  nivel hardware o en la pila de red.
*/

static void event_handler(void *arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data
                            )
{

    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if(s_retry_num<WIFI_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG,"Intenando reconectarse");
        }
        else
        {
            ESP_LOGE(TAG,"Fallo al conectar a la red Wi-Fi.");
        };
    }
    // Cuando el router nos asigna una IP con éxito.
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // guardamos la dirección de memoria que obtenemos en event_data (primero la casteamos).
        ip_event_got_ip_t* event=(ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG,"CONECTADO GUACHIN");
        s_retry_num=0; //reiniciamos el contador, ya nos conectamos.
    };
}

void wifi_init_sta(void)
{
    // Inicializamos el stack TCP/IP
    esp_netif_init();

    // Inicializamos el sistema global de eventos de la ESP32.
    esp_event_loop_create_default();

    // Creamos la interfaz de red lógica para modo estación.
    esp_netif_create_default_wifi_sta();

    // Creamos estructura de inicialización con una macrofunción.
    wifi_init_config_t wifi_config_init = WIFI_INIT_CONFIG_DEFAULT();

    // Inicializamos el módulo Wi-Fi con la estructura de inicialización.
    esp_wifi_init(&wifi_config_init);

    // Cuando ocurre un evento de WiFi llamamos a la función event_handler.
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &event_handler,
                                        NULL,
                                        NULL
                                        );

    // Cando ocurre el evento de IP de obtención de IP, ejecutamos la función event_handler.
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &event_handler,
                                        NULL,
                                        NULL
                                        );
    // Definimos una estructura de configuración con las credenciales de la Red-WIfi.
    wifi_config_t wifi_config = 
    {
        .sta=
        {
            .ssid=WIFI_SSID,
            .password=WIFI_PASSWORD
        }
    };

    // Seteamos el modo station:
    esp_wifi_set_mode(WIFI_MODE_STA);
    
    // Pasamos la interface y los parámetros de configuración.
    esp_wifi_set_config(WIFI_IF_STA,&wifi_config);

    // Activamos el hardware, todavía no intenta conectarse.
    // Una vez terminado de iniciarse, dispara el evento WIFI_EVENT_STA_START.
    // y recién ahí nos podemos conectar con el event_handler.
    // Si no logra conectarse dispara el evento WIFI_EVENT_STA_DISCONECTED y debo reintentar.
    esp_wifi_start();


    ESP_LOGI(TAG,"wifi_init_sta function finished");
}