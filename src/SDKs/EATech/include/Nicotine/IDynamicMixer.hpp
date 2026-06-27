#pragma once

// ===========================================================================
//  Nicotine::IDynamicMixer -- the public dynamic-mixer interface the game's sound
//  system (CgsSound's MixerControl, via the Nicotine layer) drives. It is the bridge
//  that owns the NFS mix system: it creates/holds the NFSMixMaster, the NFSLiveLink
//  (game<->mixer parameter bridge), and the SnapshotMixer, and forwards Init/Process
//  down to them.
//
//  Integration chain:  CgsSound MixerControl -> Nicotine::IDynamicMixer ->
//                       NFSMixMaster -> NFSMixMap (the per-map processor).
//
//  LAYOUT [sizeof=32] from ProStreet08Milestone.pdb. Bridge bodies from
//  BURNOUT_X360_ARTIST.XEX (InitMap @0x82B44C40, ProcessMixMap @0x82B44CA8,
//  CreateInstance @0x82B44AD0 -> installs the global off_83250004). x64 widths;
//  +0xNN are the X360 offsets.
// ===========================================================================

#include "types.hpp"

namespace rw { class IResourceAllocator; }

class NFSMixMaster;
class NFSLiveLink;

namespace Nicotine
{
class SnapshotMixer;

class IDynamicMixer
{
public:
    // PDB: IDynamicMixer::MAP_CREATE_PARAMS (the mixer-create config, +0x10).
    struct MAP_CREATE_PARAMS
    {
        int                      mPrintSelect;    // +0x00
        int                      NumMixStates;    // +0x04
        rw::IResourceAllocator*  MixerAllocator;  // +0x08
    };

    virtual ~IDynamicMixer();           // vtable slot 0

    void InitMap(int* lpMapData);                 // @0x82B44C40
    void ProcessMixMap(int liUnused, float lfDeltaTime); // @0x82B44CA8

    // vtable pointer occupies +0x00.
    int               miCamState;       // +0x04
    NFSMixMaster*     mMixMaster;       // +0x08
    NFSLiveLink*      mLiveLink;        // +0x0c
    MAP_CREATE_PARAMS mCreateParams;    // +0x10
    SnapshotMixer*    mpSnapshot;       // +0x1c
};

} // namespace Nicotine
