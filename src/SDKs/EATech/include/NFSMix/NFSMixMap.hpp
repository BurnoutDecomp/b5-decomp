#pragma once

// ===========================================================================
//  NFSMixMap -- the per-map dynamic-mixing PROCESSOR of the EA "NFS mix" system.
//  Owned by NFSMixMaster (one "main" map). It parses a loaded MixMap binary blob
//  (a CgsResource::BinaryFileResource) and builds the runtime mixer graph: the
//  per-state procs, the DMixIO objects, and the shared/unique mix-control / sub-mix
//  / master-mix / 3D / event channel data blocks, driving them through the
//  Nicotine::IDynamicMixer interface each update.
//
//  LAYOUT [sizeof=564 / 0x234] from ProStreet08Milestone.pdb (NFSMixMap : public
//  MixerMemBase, polymorphic -- vtable @+0x00). The 564-byte SIZE is ARTIST-
//  confirmed (NFSMixMaster::CreateMainMainMap @0x82B45868 allocates 0x234) and key
//  member offsets spanning the whole struct are ARTIST-verified via NFSMixMap::Init
//  @0x82B47E48 (+0x04 mNumStates, +0x70 m_pNFSMixMaster, +0x98 m_pStateProcs,
//  +0xA8 m_nStateMapCount, +0x22C/+0x230 m_fDeltaTimeRatio[2]). Member names from
//  the PDB. x64 widths (real pointers); the +0xNN are the X360 (32-bit) offsets.
//
//  The ~20 stXxx record types below are the serialised/runtime mixer sub-records;
//  NFSMixMap holds them only as POINTERS, so they are forward-declared here and
//  homed when the methods that walk them (AllocateMixerMemory / AssignMixCtlDataPtrs
//  / GetNext*) are bodied.
// ===========================================================================

#include "types.hpp"
#include "SDKs/EATech/include/NFSMix/MixerMemBase.hpp"

namespace Nicotine { class IDynamicMixer; class DMixIO; }

class NFSMixMaster;
class NFSMixMapState;
struct stMixMapHeader;
struct stCurveDataProc;
struct stMixCtlSharedData;
struct stMixCtlUniqueData;
struct stMixCtlProc;
struct stEvtMixCtlProc;
struct stEvtMixCtlSharedData;
struct stEvtMixCtlUniqueData;
struct st3DMixCtlProc;
struct st3DMixCtlSharedData;
struct st3DMixCtlUniqueData;
struct stMixChSharedData;
struct stMixChUniqueData;
struct stSubMixChProc;
struct stMasterMixChSharedData;
struct stMasterMixChUniqueData;
struct stMasterMixChProc;

class NFSMixMap : public MixerMemBase
{
public:
    NFSMixMap();           // @0x82B47E38 -- install vtable only
    virtual ~NFSMixMap();  // vtable slot 0 (scalar-deleting dtor reached from NFSMixMaster::~)

    void Init(NFSMixMaster* lpMaster); // @0x82B47E48

    // ---- cursor / block-pointer helpers (ARTIST-verified) ----
    stMixCtlProc* GetProcessMixCtlPtr(char lbAdvance);     // @0x82B49500
    int*          GetMasterChannelOutputArrayPtr(int liN); // @0x82B495F0
    int*          GetMasterChannelInputPtr(int liN);       // @0x82B49618
    int*          GetSubChannelInputPtr(int liN);          // @0x82B49638
    int           GetMapStateCopies(int liState);          // @0x82B49658

    // (further methods -- ResetMapData/PreProcessMixMap/AllocateMixerMemory/
    //  AssignMixCtlDataPtrs/GetNext*/CreateMainMapState/... -- bodied next.)

    // vtable pointer occupies +0x00 (virtual dtor above).
    int                       mNumStates;                  // +0x04
    int                       m_StateRefCount[25];         // +0x08
    Nicotine::IDynamicMixer*  mpMixerInterface;            // +0x6c
    NFSMixMaster*             m_pNFSMixMaster;             // +0x70
    stMixMapHeader*           m_pMMHdr;                    // +0x74
    int                       m_MapType;                   // +0x78
    int                       m_PrevCamState;              // +0x7c
    int                       m_CurCamState;               // +0x80
    float                     m_fDeltaTime;                // +0x84
    float                     m_msDeltaTime;               // +0x88
    NFSMixMap*                m_pMasterMixMap;             // +0x8c
    int*                      m_pMixMap;                   // +0x90
    int                       m_dummyout;                  // +0x94
    NFSMixMapState**          m_pStateProcs;               // +0x98
    NFSMixMapState**          m_pStateProcMemBlock;        // +0x9c
    Nicotine::DMixIO**        m_pDMixIOObj;                // +0xa0
    Nicotine::DMixIO**        m_pDMixIOMemBlock;           // +0xa4
    int                       m_nStateMapCount;            // +0xa8
    int                       m_nTotalDMixIO;              // +0xac
    int                       m_nTotalDMix3DIO;            // +0xb0
    int                       m_nAssignedDMixIOBlocks;     // +0xb4
    int                       m_nAssignedDMix3DIOBlocks;   // +0xb8
    int                       m_nAssignedInputBlocks;      // +0xbc
    int                       m_nAssignedMixMapStates;     // +0xc0
    int                       m_SharedMixCtlCount;         // +0xc4
    int                       m_SharedMixCtlsAssigned;     // +0xc8
    int                       m_UniqueMixCtlsAssigned;     // +0xcc
    int                       m_CurveProcsAdded;           // +0xd0
    int                       m_ScaleParamsAdded;          // +0xd4
    int                       m_ScaleParamsIDCount;        // +0xd8
    int                       m_CurveProcsTotal[10][2];    // +0xdc
    int                       m_SharedSubMixCount;         // +0x12c
    int                       m_SharedMasterMixCount;      // +0x130
    int                       m_Shared3DMixCtlCount;       // +0x134
    int                       m_SharedEvtMixCtlCount;      // +0x138
    int                       m_nAssignedMixCtlProc;       // +0x13c
    int                       m_AssignedMixCtlsShared;     // +0x140
    int                       m_AssignedMixCtlsUnique;     // +0x144
    int                       m_nAssignedSubMixProc;       // +0x148
    int                       m_nAssignedSubMixShared;     // +0x14c
    int                       m_nAssignedSubMixUnique;     // +0x150
    int                       m_nAssignedMasterMixProc;    // +0x154
    int                       m_nAssignedMasterMixShared;  // +0x158
    int                       m_nAssignedMasterMixUnique;  // +0x15c
    int                       m_nAssigned3DMixCtlProc;     // +0x160
    int                       m_nAssigned3DMixCtlShared;   // +0x164
    int                       m_nAssigned3DMixCtlUnique;   // +0x168
    int                       m_nAssignedEvtMixCtlProc;    // +0x16c
    int                       m_nAssignedEvtMixCtlShared;  // +0x170
    int                       m_nAssignedEvtMixCtlUnique;  // +0x174
    int*                      m_pMasterChannelOutputArrayBlock; // +0x178
    int**                     m_pDynMixInputBlocks;        // +0x17c
    int**                     m_pScalePtrArray;            // +0x180
    stCurveDataProc*          m_pCurveDataArray;           // +0x184
    stMixCtlSharedData*       m_pMixCtlData_S;             // +0x188
    stMixCtlUniqueData*       m_pMixCtlData_U;             // +0x18c
    stMixCtlProc*             m_pMixCtlProc;               // +0x190
    stEvtMixCtlProc*          m_pEvtMixCtlProc;            // +0x194
    stEvtMixCtlSharedData*    m_pEvtMixCtlData_S;          // +0x198
    stEvtMixCtlUniqueData*    m_pEvtMixCtlData_U;          // +0x19c
    st3DMixCtlProc*           m_p3DMixCtlProc;             // +0x1a0
    st3DMixCtlSharedData*     m_p3DMixCtlData_S;           // +0x1a4
    st3DMixCtlUniqueData*     m_p3DMixCtlData_U;           // +0x1a8
    stMixChSharedData*        m_pSubChData_S;              // +0x1ac
    stMixChUniqueData*        m_pSubChData_U;              // +0x1b0
    stSubMixChProc*           m_pSubChProc;                // +0x1b4
    stMasterMixChSharedData*  m_pMasterChData_S;           // +0x1b8
    stMasterMixChUniqueData*  m_pMasterChData_U;           // +0x1bc
    stMasterMixChProc*        m_pMasterChProc;             // +0x1c0
    int                       m_SFXOBJsAdded;              // +0x1c4
    int                       m_SFXCTLsAdded;              // +0x1c8
    int                       m_DataProcsAdded;            // +0x1cc
    int                       m_MixCtlsAdded;              // +0x1d0
    int                       m_3DMixCtlsAdded;            // +0x1d4
    int                       m_SubMixChannelsAdded;       // +0x1d8
    int                       m_MasterChannelsAdded;       // +0x1dc
    int                       m_EventCtlsAdded;            // +0x1e0
    int                       m_n3DCamStatesAdded;         // +0x1e4
    int                       m_nTotalMasterChannelInputs; // +0x1e8
    int                       m_nTotalMasterChannel3DOutputs; // +0x1ec
    int                       m_nTotalSubChannelInputs;    // +0x1f0
    int                       m_nTotalSubChannel3DOutputs; // +0x1f4
    int                       m_nTotalUniqueMasterChannels;// +0x1f8
    int                       m_CurrentMasterInputOffset;  // +0x1fc
    int                       m_CurrentSubInputOffset;     // +0x200
    int*                      m_pMasterChannelInputs;      // +0x204
    int*                      m_pSubChannelInputs;         // +0x208
    int                       m_CurrentStateProcBlockOffset;      // +0x20c
    int                       m_CurrentEvtMixCtlPtrBlockOffset;   // +0x210
    int                       m_Current3DMixCtlPtrBlockOffset;    // +0x214
    int                       m_CurrentSubChannelPtrBlockOffset;  // +0x218
    int                       m_CurrentMasterChannelPtrBlockOffset;// +0x21c
    int                       m_CurrentMasterInputBlockOffset;    // +0x220
    int                       m_CurrentSubInputBlockOffset;       // +0x224
    int                       m_CurrentMasterOutputBlockOffset;   // +0x228
    float                     m_fDeltaTimeRatio[2];        // +0x22c
};
