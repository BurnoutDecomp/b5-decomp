#pragma once

// ===========================================================================
//  NFSMixMaster -- the root of the EA "NFS mix" dynamic-mixing system (shared EA
//  audio tech; the "NFS" prefix is legacy naming -- Burnout uses it too: its PS3
//  DecFIGS build has MixMap/MixMapResource (loaded as a CgsResource::
//  BinaryFileResource) and the NFSMixMaster/NFSMixMap/NFSMixShape family).
//
//  NFSMixMaster owns the single "main" NFSMixMap (the 564-byte mix-map processor),
//  the per-state ref-counts, and the load/ready bookkeeping. It is a process-wide
//  singleton (the ctor stores `this` into a global; the dtor clears it).
//
//  LAYOUT: member NAMES from ProStreet08Milestone.pdb (class NFSMixMaster
//  [sizeof=128] : public MixerMemBase); the layout is CONFIRMED against the ARTIST
//  asm (ctor @0x82B45740 zeroes exactly these offsets; CreateMainMainMap
//  @0x82B45868 allocates 0x234 == 564 bytes for the NFSMixMap, matching the PDB
//  NFSMixMap sizeof). x64 widths (real pointers); the +0xNN are the X360 (32-bit)
//  offsets, ARTIST-verified.
// ===========================================================================

#include "types.hpp"
#include "SDKs/EATech/include/NFSMix/MixerMemBase.hpp"

class NFSMixMap; // the 564-byte mix-map processor (homed in a follow-on)

class NFSMixMaster : public MixerMemBase
{
public:
    NFSMixMaster();   // @0x82B45740 -- zero-init + install the global singleton
    ~NFSMixMaster();  // @0x82B45780 -- clear the singleton + destroy the main map

    // ---- declared; bodied once NFSMixMap is homed (they construct/drive it) ----
    //   NFSMixMap* CreateMainMainMap(int* lpMapData, int* lpSBActiveMasks); // @0x82B45868
    //   void InitMixMap(...);        // @0x82B45920
    //   void AssignSFXCallbacks(...);// @0x82B45A80
    //   void ProcessMixMap(...);     // @0x82B45A88

    NFSMixMap*    m_pMainMixMap;       // +0x00  the owned main mix-map (564 B)
    int*          m_pMainMixMapData;   // +0x04  the loaded MixMap binary blob
    int           mNumStates;          // +0x08
    int           m_StateRefCount[25]; // +0x0C  per-state active ref-counts (25)
    bool          m_bMapReady;         // +0x70
    int*          m_pSBActiveMasks;    // +0x74  soundbank active-mask array
    int           m_LoadMapID;         // +0x78
    NFSMixMaster* m_pMixMaster;        // +0x7C  self back-pointer (set on create)
};
