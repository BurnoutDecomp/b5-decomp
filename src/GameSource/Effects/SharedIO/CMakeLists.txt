target_sources(${PROJECT_NAME}
    PRIVATE
        BrnEffectsModuleIO_DispatchInputBuffer.cpp
        BrnEffectsModuleIO_DispatchInputBuffer_IOHelper.cpp
        BrnEffectsModuleIO_InputBuffer_Accessors.cpp
        BrnEffectsModuleIO_OutputBuffer.cpp

    PUBLIC
        BrnEffectsEnvironmentInterface.h
        BrnEffectsModuleIO_DispatchInputBuffer.h
        BrnEffectsModuleIO_InputBuffer.h
        BrnEffectsModuleIO_OutputBuffer.h
)
