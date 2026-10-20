target_sources(${PROJECT_NAME}
    PRIVATE
        BrnDirectorOutputInterface.cpp
        BrnDirectorVehicleInputInterface.cpp
        EventQueue_NewVehicleEvent_50.cpp

    PUBLIC
        BrnDirectorControllerInfo.h
        BrnDirectorEvents.h
        BrnDirectorOutputInterface.h
        BrnDirectorVehicleInputInterface.h
)
