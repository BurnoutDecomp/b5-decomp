target_sources(${PROJECT_NAME}
    PRIVATE
        BrnNetworkGameParams.cpp
        BrnNetworkGameResults.cpp
        BrnNetworkInEventTypeDefs.cpp
        BrnNetworkManager.cpp
        BrnNetworkManager_wN1_01.cpp
        BrnNetworkManager_wN1_02.cpp
        BrnNetworkManager_wN1_03.cpp
        BrnNetworkModule.cpp
        BrnNetworkModule_AddOutputGuiEvent_Inst.cpp
        BrnNetworkModuleIO.cpp
        BrnNetworkPlayer.cpp
        BrnNetworkPlayerMenuData.cpp
        BrnNetworkServers.cpp
        BrnServerInterface.cpp
        BrnServerInterfaceBase.cpp

        # Should these only be for X360?
        BrnServerInterfaceX360_wL_01.cpp
        BrnServerInterfaceX360_wL_02.cpp
        BrnServerInterfaceX360_wL_03.cpp
        BrnServerInterfaceX360_wL_04.cpp
        BrnServerInterfaceX360_wL_05.cpp

    PUBLIC
        BrnNetworkGameParams.h
        BrnNetworkGameResults.h
        BrnNetworkInEventTypeDefs.h
        BrnNetworkManager.h
        BrnNetworkModule.h
        BrnNetworkModuleIO.h
        BrnNetworkOutEventTypeDefs.h
        BrnNetworkPlayer.h
        BrnNetworkPlayerMenuData.h
        BrnNetworkServers.h
        BrnServerInterface.h
        BrnServerInterfaceBase.h
        network_defines.h
)

add_subdirectory(Components)
add_subdirectory("Debug Components")
add_subdirectory(Managers)
add_subdirectory(Messages)
add_subdirectory(Parameters)
add_subdirectory("Shared Server Types")
add_subdirectory(SharedIO)
add_subdirectory(Utilities)

if (X360)
    add_subdirectory(X360)
endif()