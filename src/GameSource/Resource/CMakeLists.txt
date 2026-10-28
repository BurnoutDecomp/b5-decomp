target_sources(${PROJECT_NAME}
    PRIVATE
        BrnDLCManager.cpp
        BrnDLCManagerMonitor.cpp
        BrnGameDataModule.cpp
        BrnGameDataModuleIO.cpp
        BrnGameDataModuleIO_ResourceInputBufferHelper.cpp
        BrnResourceAllocator.cpp
        BrnResourceDebugComponent.cpp
        IndexedPool_GameDataEventSlot.cpp

    PUBLIC
        BrnDLCManager.h
        BrnGameDataEventSlot.h
        BrnGameDataModule.h
        BrnGameDataModuleIO.h
        BrnMemoryMapData.h
        BrnResourceAllocator.h
        BrnResourceDebugComponent.h
)

add_subdirectory(SharedIO)
