target_sources(${PROJECT_NAME}
    PRIVATE
        BrnAssetIds.cpp
        BrnGameDataAllocatorList.cpp
        BrnGameDataRequestInterface_512.cpp
        BrnGameDataRequestInterface_1024.cpp
        BrnGameDataRequestInterface_3072.cpp
        BrnGameDataRequestInterface_8192.cpp
        BrnGameDataRequestInterface_32768.cpp
        BrnGameDataRequestQueue.cpp

    PUBLIC
        BrnAssetIds.h
        BrnGameDataAllocatorList.h
        BrnGameDataEvents.h
        BrnGameDataRequestQueue.h
        BrnGameDataRequestQueueImpl.h
)
