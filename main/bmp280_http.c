/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "i2cdev.h"
#include "bmp280.h"
#include "esp_wifi.h"
#include "esp_event.h" // para el bucle de eventos
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "wifi_credentials.h"

// MACROS PARA EL LED RGB
#define LED_RGB_PIN 48
#define LED_NUMBERS 1
#define RESOLUTION_HZ 10000000 //10MHz
#define BLOCK_MEM 64
#define TRANS_QUEUE 4
#define SRC_CLK RMT_CLK_SRC_DEFAULT

// MACROS PARA TIEMPO DE PARPADEO
#define BLINK_PERIOD_ON_MS 1000
#define BLINK_PERIOD_OFF_MS 4000

// MACROS PARA PINES DEL BMP280
#define I2C_MASTER_SDA 4
#define I2C_MASTER_SCL 5

// MACROS PARA HTTP
#define SERVER_URL "http://192.168.1.13:8080/measurement"

// MACRO PARA EL DEVICE ID
#define DEVICE_ID "01"
#define DEVICE_KEY "234567"

// PROTOTIPOS DE FUNCIONES
void wifi_init_sta(void);

static void event_handler(void *arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);

esp_err_t bmp280_init_esp(bmp280_t *dev,gpio_num_t sda_pin,gpio_num_t scl_pin);

void bmp280_read(bmp280_t *dev,float *temperature,float *pressure);

void rgb_init(void);

void rgb_blink_red(void);

void rgb_blink_off(void);

static esp_err_t http_event_handler(esp_http_client_event_t *evt);

static void http_post(float *temperature,float *pressure);

static void http_get(void);

// VARIABLES GLOBALES
static int s_retry_num=0; // contador de antidad de intentos.

static rmt_channel_handle_t rmt_channel_handle = NULL;
static rmt_encoder_handle_t rmt_encoder_handle = NULL;

static EventGroupHandle_t wifi_event_group;  //para hacer el post luego de tener IP
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "example";

static uint8_t led_pixels[LED_NUMBERS*3]; // cada color necesita 3 bytes
// Configuración de transmisicón estándar??
rmt_transmit_config_t ts_config =
{
    .loop_count=0,
};

static char *POST_DATA_TEMPLATE = "id="DEVICE_ID"&key="DEVICE_KEY"&t=%0.2f&p=%0.2f";  // Formato URL encoded

void app_main(void)
{   
    // ----- Variables Locales ----- //
    bmp280_t dev;
    float pressure = 0.0f;
    float temperature = 0.0f;

    // ----- Inicializaciones ----- //

    // Inicialización de NVS Flash --> inicializar esto antes que cualquier cosa.

    esp_err_t ret_nvs_flash =  nvs_flash_init();
    if(ret_nvs_flash!=ESP_OK)
    {
        return;
    }

    // Inicialización de Wi-Fi
    wifi_init_sta();
   
    // Inicialización del RGB.
    rgb_init();
    
    // Inicialización del BMP280
    bmp280_init_esp(&dev,I2C_MASTER_SDA,I2C_MASTER_SCL);

    // Bloqueamos el bloque infinito hasta obtener IP del router.
    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    while(1)
    {   
        bmp280_read(&dev,&temperature,&pressure);
        http_post(&temperature,&pressure);
        rgb_blink_red();
        rgb_blink_off();
    }
}

/** 
 * @brief Implementa la lógica de conexión y eventos a la red Wi-Fi. 
 * 
 * Maneja la lógica de eventos ante una conexión a red, desconexión, 
 * define cantidad de reintentos,etc.
 * 
 * @param[in]: .....
 * 
 * @return: None
 * 
 * @note Se vincula con la función "wifi_init_sta" de forma automática. NO hace falta llamarla 
 * en la aplicación del código.
 * 
 * @warning None.
 * 
 * @see: wifi_init_sta function.
*/
static void event_handler(void *arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // Limpiamos el bit 
        xEventGroupClearBits(wifi_event_group,WIFI_CONNECTED_BIT);

        if(s_retry_num<WIFI_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG,"Intentando reconectarse");
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

        // Ya obtuvimos IP. Seteamos el bit para arrancar el bucle infinito

        xEventGroupSetBits(wifi_event_group,WIFI_CONNECTED_BIT);
    };
}

/** 
 * @brief Inicializa el Wi-FI
 * 
 * @param[in]
 * 
 * @return None
 * 
 * @note Se 
 * 
 * @warning,None.
 * 
 * @see
*/
void wifi_init_sta(void)
{
    // Flag para detectar IP asignada
    wifi_event_group = xEventGroupCreate(); 

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

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
            // si arriban datos del server.
            printf("%.*s",evt->data_len,(char*)evt->data); // .* se usa porque no tiene caracter nulo
            break;
        
        default:
            break;
    }
    return ESP_OK;
}

static void http_get(void)
{
    ESP_LOGI(TAG,"HTTP GET");

    // Estructura de configuración
    esp_http_client_config_t http_config =
    {
        .url = SERVER_URL,                      // url del back-end
        .event_handler = http_event_handler,    // Función que procesa la respuesta del servidor.
    };

    // Inicialización del cliente http. La inicialización devuelve un handle.
    esp_http_client_handle_t client_handle = esp_http_client_init(&http_config);    

    // Definimos el método HTTP GET
    esp_http_client_set_method(client_handle,HTTP_METHOD_GET);

    // Ejecutamos la petición GET.
    // Abrimos el socket TCP hacia la IP y puerto. Envía GET y espera respuesta.
    // Cuando llega la respuesta se ejecuta http_event_handler.

    esp_err_t err_http = esp_http_client_perform(client_handle); // pasar handle del cliente

    // Analizamos el resultado de la petición:
    if(err_http==ESP_OK)
    {
        ESP_LOGI(TAG,
            "GET status = %d, content_length = %lld",
            esp_http_client_get_status_code(client_handle),
            esp_http_client_get_content_length(client_handle));
    }
    else
    {
        ESP_LOGE(TAG,"GET Failed %s",esp_err_to_name(err_http));
    }

    // Liberamos recursos:
    esp_http_client_cleanup(client_handle);
}

static void http_post(float *temperature,float *pressure)
{
    // Tamaño del buffer
    char post_data[64];

    // Formateo el string con las mediciones
    sprintf(post_data,POST_DATA_TEMPLATE,*temperature,*pressure);

    // Mensajes informativos: URL de destino y MENSAJE:
    ESP_LOGI(TAG,"URL: %s",SERVER_URL);
    ESP_LOGI(TAG,"POST_DATA: %s",post_data);

    // Estructura de configuración con URL y handler.
    esp_http_client_config_t config_post = 
    {
        .url = SERVER_URL,
        .event_handler = http_event_handler  // pasar handler de eventos http
    };

    // Inicializamos el cliente http:
    esp_http_client_handle_t client_post_handle = esp_http_client_init(&config_post);

    // Definimos el método HTTP POST:
    esp_err_t http_post_err = esp_http_client_set_method(client_post_handle,HTTP_METHOD_POST);

    // Seteamos el header. Se le pasa el handle, la clave y el valor.
    esp_http_client_set_header(
        client_post_handle,
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    // Seteamos el payload. Se le pasa el handle, el puntero a los datos y la longitud.
    esp_http_client_set_post_field(
        client_post_handle,
        post_data,
        strlen(post_data)
    );

    // Petición POST
    esp_err_t post_err = esp_http_client_perform(client_post_handle); // pasar handle del cliente

    // Analizamos la respuesta de la petición POST.
    if(post_err == ESP_OK)
    {
        ESP_LOGI(TAG,
            "POST STATUS: %d",
            esp_http_client_get_status_code(client_post_handle)
            );
    }
    else
    {
        ESP_LOGE(TAG,
            "POST FAILED: %s",
            esp_err_to_name(post_err)
        );
    }

    // Finalizamos la petición POST.
    esp_http_client_cleanup(client_post_handle);

}

void rgb_init(void)
{
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
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&rmt_config,&rmt_channel_handle));

    // 2. CONFIGURO Y CREO EL ENCODER DE LA TIRA DE LED
    ESP_LOGI(TAG,"Inicializando encoder");

    led_strip_encoder_config_t encoder_config = 
    {
        .resolution=RESOLUTION_HZ,
    };

    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config,&rmt_encoder_handle));
    
    // 3. HABILITAMOS EL CANAL
    ESP_LOGI(TAG,"HABILITAMOS EL CANAL");

    ESP_ERROR_CHECK(rmt_enable(rmt_channel_handle));

}

void rgb_blink_red(void)
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
}


void rgb_blink_off(void)
{
    // PREPARAMOS EL COLOR A TRANSMITIR
    // Formato GRB: posición 0 es verde.
    led_pixels[0]=0;
    led_pixels[1]=0;
    led_pixels[2]=0;

    // Transmito el color verde por 1 segundo. 
    rmt_transmit(rmt_channel_handle,rmt_encoder_handle,&led_pixels,sizeof(led_pixels),&ts_config);
    rmt_tx_wait_all_done(rmt_channel_handle,portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_OFF_MS));
}

esp_err_t bmp280_init_esp(bmp280_t *dev,gpio_num_t sda_pin,gpio_num_t scl_pin)
{
    // --------------------- BMP280 ----------------------//
    ESP_LOGI(TAG,"Inicializando configuración del BMP280");

    // Inicializamos el i2c
    ESP_ERROR_CHECK(i2cdev_init());

    bmp280_params_t params;

    bmp280_init_default_params(&params);
    memset(dev,0,sizeof(bmp280_t));

    // Inicializa el descriptor
    ESP_ERROR_CHECK(bmp280_init_desc(dev,BMP280_I2C_ADDRESS_0,0,sda_pin,scl_pin));

    // Inicializamos el BMP280
    ESP_ERROR_CHECK(bmp280_init(dev,&params));

    ESP_LOGI(TAG,"Sensor Inicializado");

    return ESP_OK;
}


void bmp280_read(bmp280_t *dev,float *temperature,float *pressure)
{
    //----------------- LECTURA BMP280----------------------//

    if(bmp280_read_float(dev,temperature,pressure,NULL) == ESP_OK)
    {   
        ESP_LOGI(TAG,"Temperatura: %.2f °C | Presión: %.2f Pa",*temperature,*pressure);
    }
    else
    {
        ESP_LOGE(TAG,"Error al leer del BMP280 en el bus I2C");
    }
}