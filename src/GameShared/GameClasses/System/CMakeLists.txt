target_sources(${PROJECT_NAME}
    PRIVATE

    PUBLIC
        CgsHardwareInit.h
)

if (WIN32)
    # Existing ARTIST-backed lifecycle/dispatch providers. This mounts definitions;
    # original main/outer preparation remains unselected. XNet/XContent stay external.
    target_sources(${PROJECT_NAME} PRIVATE
        FileSystem/CgsPatchManager.cpp
        FileSystem/Devices/CgsDevicePhysicalX360.cpp
        FileSystem/CgsDeviceAsyncOp.cpp
        Resource/CgsLiveUpdateModule.cpp
        Resource/CgsLiveUpdateModuleIO_OutputBuffer.cpp
        Resource/CgsResourceBasePool.cpp
        GameTalk/CgsGameTalk.cpp
        GameTalk/X360/CgsGameTalkProtocolX360.cpp
    )
    add_subdirectory(PC)
endif()
