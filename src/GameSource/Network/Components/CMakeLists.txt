target_sources(${PROJECT_NAME}
    PRIVATE
        BrnServerInterfaceCustomCommands.cpp
        BrnServerInterfaceDownloadableConfig.cpp
        BrnServerInterfaceDownloadableConfig_embed_check.cpp
        BrnServerInterfaceTelemetry.cpp

    PUBLIC
        BrnServerInterfaceCustomCommands.h
        BrnServerInterfaceDownloadableConfig.h
        BrnServerInterfaceTelemetry.h
)
