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
struct stMixEvtParams;

class NFSMixMap : public MixerMemBase
{
public:
    NFSMixMap();           // @0x82B47E38 -- install vtable only
    virtual ~NFSMixMap();  // vtable slot 0 (scalar-deleting dtor reached from NFSMixMaster::~)
    // vtable slot 1 @0x82B4C258 -- bind the loaded MixMap blob + zero the refcounts, then PreProcess.
    virtual void InitMixMap(int* lpMixMap, NFSMixMap* lpMasterMixMap);
    // vtable slot 2 @0x82B4C548 -- per-frame: advance delta-time + cam-state, run the mix graph.
    virtual void ProcessMixMap(float lfDeltaTime, int liCamState);

    void Init(NFSMixMaster* lpMaster); // @0x82B47E48
    void PreProcessMixMap();           // @0x82B48498 -- parse the blob -> counts (tail-called by InitMixMap)

    // ---- cursor / block-pointer helpers (ARTIST-verified) ----
    stMixCtlProc* GetProcessMixCtlPtr(char lbAdvance);     // @0x82B49500
    int*          GetMasterChannelOutputArrayPtr(int liN); // @0x82B495F0
    int*          GetMasterChannelInputPtr(int liN);       // @0x82B49618
    int*          GetSubChannelInputPtr(int liN);          // @0x82B49638
    int           GetMapStateCopies(int liState);          // @0x82B49658

    // ---- "next slot" allocators (return &m_p<X>[m_nAssigned<X>]; bump the count if
    //      the advance flag is set). ARTIST-verified (member-name systematic mapping). ----
    stEvtMixCtlProc*       GetNextEvtMixCtlProc(char lbAdvance);       // @0x82B48F88
    stEvtMixCtlSharedData* GetNextEvtMixCtlShared(char lbAdvance);     // @0x82B48FB8
    st3DMixCtlProc*        GetNext3DMixCtlProc(char lbAdvance);        // @0x82B49048
    st3DMixCtlSharedData*  GetNext3DMixCtlShared(char lbAdvance);      // @0x82B49078
    stMasterMixChProc*     GetNextMasterMixProc(char lbAdvance);       // @0x82B490E0
    stMasterMixChSharedData* GetNextMasterMixShared(char lbAdvance);   // @0x82B49110
    stSubMixChProc*        GetNextSubMixProc(char lbAdvance);          // @0x82B49178
    stMixChSharedData*     GetNextSubMixShared(char lbAdvance);        // @0x82B491D8
    // "Next slot" UNIQUE-record allocators (twins of the shared/proc ones above; the
    // Evt one zero-inits the fresh record with flt_82001CC0 == 0.0). ARTIST-verified.
    stEvtMixCtlUniqueData*   GetNextEvtMixCtlUnique(char lbAdvance);   // @0x82B48FF0
    stMasterMixChUniqueData* GetNextMasterMixUnique(char lbAdvance);   // @0x82B49140
    stMixChUniqueData*       GetNextSubMixUnique(char lbAdvance);      // @0x82B491A8
    // GetNextMapState @0x82B49210 -- byte-offset cursor over m_pStateProcMemBlock
    // (stride 0x60 == sizeof NFSMixMapState).
    NFSMixMapState*          GetNextMapState(char lbAdvance);          // @0x82B49210

    void AssignSFXCallbacks(void* lpOwner);              // @0x82B481B8 -- mpMixerInterface = owner
    stCurveDataProc* GetCurveDataPtr(int* lpParam);      // @0x82B49238 -- find/append curve-proc slot
    int* AddScaleIDs(unsigned short* lpScaleParams, int liProcIdx); // @0x82B492F8
    int* AddEvtScaleIDs(stMixEvtParams* lpEvtParams, int liProcIdx); // @0x82B493F8 (event twin of AddScaleIDs)
    // Links the freshly-allocated shared/unique MixCtl records into lpProc (sets
    // lpProc->psdata / ->pudata) for NFSMixMapState::CreateMixCtls. Body is its own TU
    // (still un-reconstructed); signature is ARTIST-derived from the call site (r3=this,
    // r4=proc, r5=entry cursor, r6=object index, r7=proc index; return discarded).
    // Declared here so CreateMixCtls compiles against it (trap-stubbed at link).
    void AssignMixCtlDataPtrs(stMixCtlProc* lpProc, int* lpEntry, int liObjectIndex, int liProcIdx);

    // vtable-less allocation passes (build the runtime mixer graph from the parsed counts).
    int  AllocateMixerMemory();     // @0x82B48AF8 -- allocate every m_p*Block/Data via the mixer allocator
    int* AllocateInputArrays();     // @0x82B4A120 -- size + allocate the DMIX SFXOBJ/SFXCTL input block

    // per-frame update passes driven by ProcessMixMap.
    void UpdateSubChannels();       // @0x82B4AC10 -- accumulate sub-channel inputs (Q15, clamped)
    stMasterMixChProc* GetMasterMixChProc(int liPackedID); // @0x82B4A840 -- state-routed master-mix proc

    // ---- BLOCKED (declared so callers compile; bodies deferred -- see NFSMixMap.cpp notes) ----
    //   * The SFX-callback-host functions call mpMixerInterface via un-modelled vtable slots
    //     (+4 register / +8 get-state-instance-count) and Nicotine::DMixIO::GetDMixID.
    //   * The per-frame DSP updaters need X360 rodata float/int tables not in this export.
    int   SETSFXID(int liID, int* lpObj);                 // @0x82B481C0  (host slot +4)
    void* GetObjectPtr(int liID, char lbA, char lbB);     // @0x82B4A550  (host slot +4 / DMixIO)
    void  ConnectMixMap();                                // @0x82B4A8D8  (via SETSFXID/GetObjectPtr)
    int   SetupStateRefCount();                           // @0x82B48410  (host slot +8)
    void  AllocateDMixIOArrays();                         // @0x82B49750  (host debug / DMixIO)
    void  CreateMainMapState(int liState, int liCopy, int liObjIdx); // @0x82B49680 (NFSMixMapState build API)
    void  InitMainMapStates();                            // @0x82B4ABD0  (NFSMixMap::SetupStateProcArrays)
    void  Update3DMixCtls();       // @0x82B4BB98  (rodata flt_82F8795C/flt_82002138/flt_820037C8)
    void  UpdateEvtMixCtls();      // @0x82B4C2A8  (couples the 3 envelope updaters below)
    void  UpdateMasterChannels();  // @0x82B4ACD8  (rodata dword_821483D0[] preset table)
    void  UpdateADSREvent(stEvtMixCtlProc* lpProc); // @0x82B4B168 (rodata flt_821483E8/flt_821483E4)
    void  UpdateAREvent(stEvtMixCtlProc* lpProc);   // @0x82B4B948 (rodata flt_821483E8/flt_821483E4)
    void  UpdateASREvent(stEvtMixCtlProc* lpProc);  // @0x82B4B5F0 (rodata flt_821483E8/flt_821483E4)

    void ResetMapData();   // @0x82B482B0 -- zero every runtime counter/offset + block ptr

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
