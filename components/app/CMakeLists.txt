idf_component_register(
    SRCS "src/app.c" "src/mqtt.c" "src/controller.c"
    INCLUDE_DIRS "include"
    REQUIRES modem relay led button json
    PRIV_REQUIRES driver mqtt esp_netif esp_event
)
