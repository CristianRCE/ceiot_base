idf_component_register(SRCS "bmp280_http.c" "led_strip_encoder.c"
                       PRIV_REQUIRES esp_driver_rmt
                       INCLUDE_DIRS ".")
