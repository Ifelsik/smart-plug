idf_component_register(
    SRCS "src/app.c" "src/mqtt.c" "src/command_handler.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES modem relay driver mqtt json
)