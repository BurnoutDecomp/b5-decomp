#pragma once

// ===========================================================================
// CgsSceneManager::OverlapCullingModule
//   Home: GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.{h,cpp}
//
// The contact-generation "scene graph" stage that culls the candidate overlap
// pairs the OverlapGenerationModule produced down to the colliding pairs. It is
// a CgsModule sub-module (a manually-built module vtable head, matching its
// sibling CgsOverlapGenerationModule). Embedded BY VALUE in
// CgsSceneManager::SceneManagerModule (mOverlapCuller).
//
// The SceneManagerModule drives its lifecycle (Construct / Destruct) and its
// per-frame CullOverlaps. Modelled here with a manual module-vtable word head
// (matching CgsOverlapGenerationModule's muModuleVTable convention) plus the
// bulk overlap / interval state as a documented opaque buffer (owned by this
// module's own TUs; byte offsets are not preserved on the x64 PC compile, per
// the project's semantic-parity rule).
// ===========================================================================

#include "types.hpp"

#include <eathread/eathread_rwmutex.h>

namespace CgsSceneManager
{
    class OverlapCullingModule
    {
    public:
        OverlapCullingModule();

        void Construct();
        void Destruct();

        // Prepare the culler. The SceneManagerModule invokes this through the module
        // vtable (CgsSceneManagerModule.cpp Prepare, stage 8) with the entity +
        // volume managers. Returns success.
        bool Prepare(class EntityManager* lpEntityManager, class VolumeManager* lpVolumeManager);

        // Cull the generated overlaps. The SceneManagerModule invokes this through
        // the module vtable (CgsSceneManagerModule.cpp UpdateContactGeneration):
        //   lpInputBuffer  = the OverlapCulling input (overlap pairs to test),
        //   lpOutputBuffer = the culler output (the surviving colliding pairs).
        void CullOverlaps(void* lpInputBuffer, void* lpOutputBuffer);

    private:
        u32                 muModuleVTable;
        EA::Thread::RWMutex mInputMutex;
        EA::Thread::RWMutex mOutputMutex;

        // Bulk overlap / interval / pair state (owned by this module's TUs).
        u8 maEmbeddedState[443028 - 8 - 2 * sizeof(EA::Thread::RWMutex)];
    };
}
