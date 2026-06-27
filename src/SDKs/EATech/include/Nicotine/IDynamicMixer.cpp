#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp" // the mix master this drives

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
        // (the latter needs AllocateMixerMemory) and SnapshotMixer::AttachToMixMap
        // (SnapshotMixer not yet homed) are wired once those land. The map object is
        // created + blob-bound (CreateMainMainMap -> NFSMixMap::Init) here.
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
    // FLAG (deferred): the NFSLiveLink::ProcessLiveLink + SnapshotMixer::Update branches
    // need NFSLiveLink / SnapshotMixer (not yet homed); mLiveLink/mpSnapshot are null
    // until then, so the branches are no-ops. The mix-master drive below is live.
    if (mMixMaster)
        mMixMaster->ProcessMixMap(lfDeltaTime, miCamState); // bodied (-> NFSMixMap::ProcessMixMap)
}

} // namespace Nicotine
