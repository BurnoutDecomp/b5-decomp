target_sources(${PROJECT_NAME}
    PRIVATE
        BrnReplayDiskReadStream.cpp
        BrnReplayGPUDiskWriteStream.cpp
        BrnReplayReadStream.cpp
        BrnReplayStreamHeader.cpp
        BrnReplayWriteStream.cpp
        BrnReplayWriteStream_embed_check.cpp

    PUBLIC
        BrnReplayDiskReadStream.h
        BrnReplayFrunkHeader.h
        BrnReplayGPUDiskWriteStream.h
        BrnReplayReadStream.h
        BrnReplayStreamHeader.h
        BrnReplayWriteStream.h
)
