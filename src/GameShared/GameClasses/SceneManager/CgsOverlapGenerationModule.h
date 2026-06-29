#pragma once

#include "types.hpp"

#include <eathread/eathread_rwmutex.h>

namespace CgsSceneManager
{
class OverlapGenerationModule
{
public:
    OverlapGenerationModule();

    void Construct();

    // Tear down the overlap generator. SceneManagerModule::Destruct dispatches this through the
    // module vtable destruct slot (X360 0x828D1640: (*(*(this+0x290)+12))(this+0x290)). Body lives
    // in this module's own TU.
    void Destruct();

    // Prepare the overlap generator. The SceneManagerModule invokes this through
    // the module vtable (CgsSceneManagerModule.cpp Prepare, stage 7):
    //   lpCullingGroupManager = the scene's culling-group manager,
    //   lpCullingTable        = its built culling table.
    // Returns success.
    bool Prepare(void* lpCullingGroupManager, void* lpCullingTable);

    // Generate the candidate overlap pairs for this frame. The SceneManagerModule
    // calls this from UpdateContactGeneration:
    //   lpOutputBuffer   = the OverlapGeneration output buffer,
    //   lpSceneInstances = the EntityManager's volume-instance scene.
    void GenerateOverlaps(void* lpOutputBuffer, void* lpSceneInstances);

private:
    u32                 muModuleVTable;
    EA::Thread::RWMutex mInputMutex;
    EA::Thread::RWMutex mOutputMutex;
    u8                  mPad0[272];
    u32                 muOverlapListVTable;
};
}
