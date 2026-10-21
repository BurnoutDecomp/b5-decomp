target_sources(${PROJECT_NAME}
    PRIVATE
        BrnTrafficJob.cpp
        BrnUpdateVehiclesJob.cpp
        BrnUpdateVehiclesJob_MoveToTarget.cpp
        TrafficCommon.cpp

    PUBLIC
        BrnTrafficJob.h
        BrnTrafficSwerveWatch.h
        BrnUpdateVehiclesJob.h
        TrafficCommon.h
)
