target_sources(DirtySDK INTERFACE
    connapipc.cpp
    gamemanagerpc.cpp
    hlbudapipc.cpp
    lobbyapipc.cpp
    lobbyloginpc.cpp
    lobbymiscpc.cpp
    netconnpc.cpp
    netgamelinkpc.cpp
    pingmanagerpc.cpp
    protopc.cpp
    telemetryapipc.cpp
    voippc.cpp
)

add_subdirectory(lan)
add_subdirectory(online)
