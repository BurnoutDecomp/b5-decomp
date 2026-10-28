target_sources(${PROJECT_NAME}
    PRIVATE
        Array_MomentControllerMomentHandle_10.cpp
        Array_MomentDescription_10.cpp
        BrnMoment.cpp
        BrnMomentController.cpp
        BrnMomentParameterBank.cpp
        BrnMomentSelector.cpp
        BrnMomentSharedInfo.cpp
        ObjectPool_MomentBucket_20.cpp

    PUBLIC
        BrnMoment.h
        BrnMomentController.h
        BrnMomentParameterBank.h
        BrnMomentSelector.h
        BrnMomentSelectorSelector.h
        BrnMomentSharedInfo.h
)

add_subdirectory(Moments)
