#pragma once

// ===========================================================================
//  NFSMixMapState -- one mixer "state" within an NFSMixMap (the NFS mix system).
//  An NFSMixMap owns an array of these (m_pStateProcs); each holds the per-state
//  proc/header pointers for its mix-controls, sub-mix / master-mix channels, 3D
//  controls and events, plus a ref-count shared across its instance copies.
//
//  LAYOUT [sizeof=96 / 0x60] from ProStreet08Milestone.pdb (NFSMixMapState :
//  public MixerMemBase, polymorphic). ARTIST-VERIFIED: Initialize @0x82B4C6B0
//  (+0x04 m_pNFSMixMap, +0x38 m_StateIndex, +0x3C m_ThisStateRefCnt,
//  +0x40 m_ObjectIndex, +0x44 m_NumStateCopies, +0x5C m_pFirstInstance, instance
//  stride 0x60) and the GetXxxProc accessors (param-bases +0x08/+0x0C/+0x10/+0x14/
//  +0x18 confirm the stMixStateParams order; bounds counts +0x48..+0x58). x64
//  widths; +0xNN are the X360 offsets.
// ===========================================================================

#include "types.hpp"
#include "SDKs/EATech/include/NFSMix/MixerMemBase.hpp"

class NFSMixMap;
struct stMixCtlProc;
struct stSubMixChProc;
struct stMasterMixChProc;
struct st3DMixCtlProc;
struct stEvtMixCtlProc;
struct stMixMapStateHdr;
struct stMixCtlHdr;
struct st3DMixCtlHdr;
struct stMixEventHdr;
struct stMixChHdr;

// The per-state proc-array bases (NFSMixMapState +0x08). Order ARTIST-verified by the
// GetXxxProc accessors' load offsets (+0x08/+0x0C/+0x10/+0x14/+0x18).
struct stMixStateParams
{
    stMixCtlProc*      pMixCtlProcs;      // +0x00 (state +0x08)
    stSubMixChProc*    pSubMixChProcs;    // +0x04 (state +0x0C)
    stMasterMixChProc* pMasterMixChProcs; // +0x08 (state +0x10)
    st3DMixCtlProc*    p3DMixCtlProc;     // +0x0C (state +0x14)
    stEvtMixCtlProc*   pEvtMixCtlProc;    // +0x10 (state +0x18)
};

class NFSMixMapState : public MixerMemBase
{
public:
    NFSMixMapState();
    virtual ~NFSMixMapState();

    // vtable slot 1 @0x82B4C6B0 (PDB: Initialize is virtual).
    virtual void Initialize(NFSMixMap* lpMap, int liStateIndex, int liNumStateCopies, int liObjectIndex);
    int  GetStateRefCount();             // @0x82B4C718 -- m_pFirstInstance->m_ThisStateRefCnt

    // ---- proc accessors (ARTIST-verified base/bounds offsets) -- each indexes the
    //   copy at m_pFirstInstance[copyIdx], bounds-checks procIdx, returns base[procIdx].
    stMixCtlProc*      GetMixCtlProc(unsigned char procIdx, int copyIdx);      // @0x82B4C728 base+0x08 bnd+0x48
    st3DMixCtlProc*    Get3DMixCtlProc(unsigned char procIdx, int copyIdx);    // @0x82B4C770 base+0x14 bnd+0x4C
    stEvtMixCtlProc*   GetEvtMixCtlProc(unsigned char procIdx, int copyIdx);   // @0x82B4C7B8 base+0x18 bnd+0x50
    stSubMixChProc*    GetSubMixChProc(unsigned char procIdx, int copyIdx);    // @0x82B4C800 base+0x0C bnd+0x54
    stMasterMixChProc* GetMasterMixChProc(unsigned char procIdx, int copyIdx); // @0x82B4C848 base+0x10 bnd+0x58

    // ---- state-array cursor + builder passes (called from NFSMixMap::CreateMainMapState /
    //      SetupStateProcArrays; ARTIST-verified against the X360 asm). ----
    NFSMixMapState* GetMixMapProc(int liIndex);   // @0x82B4D648 -- &m_pFirstInstance[liIndex]
    NFSMixMapState* AddMixState(int liObjectIndex, NFSMixMapState* lpFirstInstance); // @0x82B4D660
    void CreateSubMixChannels();      // @0x82B4CB00 -- build this state's sub-mix channel procs
    void CreateMasterMixChannels();   // @0x82B4CC48 -- build this state's master-mix channel procs
    void InitializeSubChannels();     // @0x82B4D2A8 -- wire sub-channel input arrays from the blob
    void InitializeMasterChannels();  // @0x82B4D420 -- wire master-channel input arrays from the blob

    // ---- BLOCKED (declared so future callers compile; bodies deferred) ----
    //   * CreateMixCtls    @0x82B4C890 -- needs the X360 dB->Q15 table word_82F8677A (guessed
    //     rodata, bytes not in this export) AND NFSMixMap::AssignMixCtlDataPtrs (un-homed callee).
    //   * CreateEvtMixCtls @0x82B4CE00 -- needs word_82F8677A (guessed rodata) AND the un-named
    //     NFSMixMap helper sub_82B493F8 (un-homed callee).
    void CreateMixCtls();     // @0x82B4C890  BLOCKED
    void CreateEvtMixCtls();  // @0x82B4CE00  BLOCKED

    // vtable pointer occupies +0x00 (virtual dtor above).
    NFSMixMap*        m_pNFSMixMap;        // +0x04
    stMixStateParams  m_MixStateParams;    // +0x08 (5 proc-array bases)
    stMixMapStateHdr* m_pMMStateHdr;       // +0x1c
    stMixCtlHdr*      m_pMixCtlHdr;        // +0x20
    st3DMixCtlHdr*    m_p3DMixCtlHdr;      // +0x24
    stMixEventHdr*    m_pEvtMixCtlHdr;     // +0x28
    stMixChHdr*       m_pMixChHdr;         // +0x2c
    stMixChHdr*       m_pSubChHdr;         // +0x30
    int*              m_pChOutArrays;      // +0x34
    int               m_StateIndex;        // +0x38
    int               m_ThisStateRefCnt;   // +0x3c
    int               m_ObjectIndex;       // +0x40
    int               m_NumStateCopies;    // +0x44
    int               m_MixCtlsAdded;      // +0x48
    int               m_3DMixCtlsAdded;    // +0x4c
    int               m_EvtMixCtlsAdded;   // +0x50
    int               m_SubMixChannelsAdded;  // +0x54
    int               m_MasterChannelsAdded;  // +0x58
    NFSMixMapState*   m_pFirstInstance;    // +0x5c
};
