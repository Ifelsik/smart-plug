idf_component_register(
    SRCS "src/modem.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common driver freertos esp_modem esp_netif
)