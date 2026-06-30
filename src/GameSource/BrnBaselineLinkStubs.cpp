// ===========================================================================
// BrnBaselineLinkStubs.cpp -- FLAG (baseline link stubs).
//
// Minimal out-of-line definitions so the game exe LINKS without compiling the full
// IntelliFrag / EmergencyFrag defrag-pool-state TUs and the ReplayModule TU, each of
// which cascades into a web of further un-homed CgsResource::Pool / replay-stream
// symbols (Pool::BeginDefragmentation / GetHeapAlignment / AddAddressedAllocRequest /
// GPUDiskWriteStream::Dispatch / DataStreamCommandPoster::End, ...).
//
// These paths are NOT exercised on the current loading-screen -> EA -> Criterion boot.
// Replace each stub with its real TU once that subsystem's lower layer is wired:
//   - CgsIntelliFragPoolModuleState.cpp / CgsEmergencyFragPoolModuleState.cpp  (need the
//     CgsResource::Pool defrag surface)
//   - GameSource/Replays/BrnReplayModule.cpp  (need GPUDiskWriteStream + the command poster)
// ===========================================================================

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameSource/Replays/BrnReplayModule.h"

namespace CgsResource
{
    // The concrete defrag strategies (the Base versions are stubbed in
    // CgsBaseDefragPoolModuleState.cpp). Returning false / doing nothing leaves the pool
    // un-defragmented, which is benign on the boot path.
    bool IntelliFragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)      { return false; }
    void IntelliFragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) {}
    bool EmergencyFragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)      { return false; }
    void EmergencyFragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) {}
}

namespace BrnReplays
{
    // Link stub for the replay module ctor (BrnGameModule constructs mReplayModule). The
    // member sub-objects default-construct; the real ctor body is in BrnReplayModule.cpp.
    ReplayModule::ReplayModule() {}
}
