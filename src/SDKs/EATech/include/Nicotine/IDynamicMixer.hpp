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
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp" // the off_83250004 mixer allocator type

class NFSMixMaster;
class NFSLiveLink;

namespace Nicotine
{
class SnapshotMixer;
struct DMixIO;

class IDynamicMixer
{
public:
    // PDB: IDynamicMixer::MAP_CREATE_PARAMS (the mixer-create config, +0x10).
    struct MAP_CREATE_PARAMS
    {
        // DWARF (DynamicMixer.hpp): MixerAllocator is an EA::Allocator::ICoreAllocator*;
        // modelled here as the project's MixerAllocator (the same vtable shape, slot 2 =
        // Allocate, slot 3 = Free -- it is what CreateInstance installs into off_83250004).
        int             mPrintSelect;    // +0x00
        int             NumMixStates;    // +0x04
        MixerAllocator* MixerAllocator;  // +0x08
    };

    IDynamicMixer();                    // @0x82B44A78 -- zero-init + install the global instance
    virtual ~IDynamicMixer();           // @0x82B44DD0 -- vtable slot 0

    // Game-owned callback surface consumed by NFSMixMap. The slot order is
    // attested by ARTIST calls through +4/+8/+0x0C/+0x10.
    virtual bool ConnectDMixIO(DMixIO* apDMixIO) = 0;
    virtual int  GetStateCount(int aiState) = 0;
    virtual int  DMixPrintf(const char* apFormat, ...) = 0;
    virtual void DMixAssert(bool abCondition, const char* apFormat, ...) = 0;

    // @0x82B44AB8 -- the process-wide IDynamicMixer instance (set by the ctor).
    static IDynamicMixer* GetInstance();

    // @0x82B44AD0 -- install the mixer allocator + create the NFSMixMaster, SnapshotMixer
    // (and NFSLiveLink) sub-objects through it.
    void CreateInstance(const MAP_CREATE_PARAMS* lpParams);

    void InitMap(int* lpMapData);                 // @0x82B44C40
    void DestroyMap();                            // @0x82B44D20
    void InitSnapshots(void* apSnapshotData);     // @0x82B44D68
    void ProcessMixMap(int liUnused, float lfDeltaTime); // @0x82B44CA8

    // @0x825487C0 -- set the camera/mix state index; returns this (X360 returns r3).
    IDynamicMixer* SetCameraState(int liState);

    void SetSnapshot(int aiSnapshot, bool abActive); // @0x82B44DB8

    // vtable pointer occupies +0x00.
    int               miCamState;       // +0x04
    NFSMixMaster*     mMixMaster;       // +0x08
    NFSLiveLink*      mLiveLink;        // +0x0c
    MAP_CREATE_PARAMS mCreateParams;    // +0x10
    SnapshotMixer*    mpSnapshot;       // +0x1c
};

} // namespace Nicotine
