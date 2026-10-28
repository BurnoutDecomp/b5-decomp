target_sources(${PROJECT_NAME}
    PRIVATE
        BrnParticleDescription.cpp
        BrnParticleResourceAccessors.cpp
        EffectsVertexBuffer.cpp
        EffectsVertexBufferManager.cpp
        LionParticleRender.cpp
        LionPerfMon.cpp
        ParticleCpuMonitors.cpp
        ParticleModule.cpp
        ParticleModule_DebrisSpawn.cpp
        ParticleModule_InternalAllocator.cpp
        ParticleModule_Lifecycle.cpp
        ParticleModule_SparkEvents.cpp
        ParticleModuleIO.cpp

    PUBLIC
        BrnParticleDescription.h
        BrnParticleModuleIO_EventTypes.h
        EffectsVertexBuffer.h
        EffectsVertexBufferManager.h
        LionParticleRender.h
        ParticleCpuMonitors.h
        ParticleModule.h
        ParticleModuleIO.h
)

add_subdirectory(Native)
