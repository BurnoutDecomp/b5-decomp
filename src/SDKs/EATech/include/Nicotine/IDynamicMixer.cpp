#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"   // the mix master this drives
#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp" // the snapshot mixer this drives

// ===========================================================================
//  Nicotine::IDynamicMixer -- bridge bodies (store-for-store from ARTIST). This is the
//  seam where CgsSound reaches the reconstructed NFS mix subsystem.
// ===========================================================================

namespace Nicotine
{

IDynamicMixer::~IDynamicMixer() {}   // vtable slot 0 (teardown frees the sub-objects -- FLAG: deferred)

// ---------------------------------------------------------------------------
// IDynamicMixer::InitMap @0x82B44C40 -- load a MixMap blob into the mix master.
//   if (mMixMaster) {
//       mMixMaster->CreateMainMainMap(lpMapData, 0);
//       mMixMaster->AssignSFXCallbacks(this);
//       mMixMaster->InitMixMap();
//       if (mpSnapshot) mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap);
//   }
// ---------------------------------------------------------------------------
void IDynamicMixer::InitMap(int* lpMapData)
{
    if (mMixMaster)
    {
        mMixMaster->CreateMainMainMap(lpMapData, 0);  // bodied (alloc+ctor+Init the map)
        // FLAG (deferred): NFSMixMaster::AssignSFXCallbacks + NFSMixMaster::InitMixMap
        // (the latter needs AllocateMixerMemory) are wired once those land.
        if (mpSnapshot)
            mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap); // homed (FLAG: body cascades to GetMasterMixChProc)
    }
}

// ---------------------------------------------------------------------------
// IDynamicMixer::ProcessMixMap @0x82B44CA8 -- per-frame drive.
//   if (mLiveLink)  NFSLiveLink::ProcessLiveLink(mLiveLink, mpSnapshot);
//   if (mpSnapshot) mpSnapshot->Update(dt);
//   if (mMixMaster) mMixMaster->ProcessMixMap(dt, miCamState);
// ---------------------------------------------------------------------------
void IDynamicMixer::ProcessMixMap(int /*liUnused*/, float lfDeltaTime)
{
    // FLAG (deferred): NFSLiveLink::ProcessLiveLink needs NFSLiveLink (a >6KB divergent
    // object, not yet homed); mLiveLink is null until then, so that branch is a no-op.
    if (mpSnapshot)
        mpSnapshot->Update(lfDeltaTime);                   // homed (FLAG: DSP rodata-blocked)
    if (mMixMaster)
        mMixMaster->ProcessMixMap(lfDeltaTime, miCamState); // bodied (-> NFSMixMap::ProcessMixMap)
}

} // namespace Nicotine
