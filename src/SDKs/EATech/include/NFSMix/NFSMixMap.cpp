#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp" // stMixCtlProc / stMixMapHeader for the helpers

// ===========================================================================
//  NFSMixMap -- ctor / Init / dtor. Store-for-store from BURNOUT_X360_ARTIST.XEX.
//  See NFSMixMap.hpp for the (ARTIST-size-confirmed, Init-verified) layout.
// ===========================================================================

// ---------------------------------------------------------------------------
// NFSMixMap::NFSMixMap @0x82B47E38
//   *this = &off_82147D50;   (install the vtable -- nothing else)
// The vtable install is reproduced by the compiler-generated prologue of this
// polymorphic class (virtual dtor), so the body is empty -- exactly the X360 ctor.
// ---------------------------------------------------------------------------
NFSMixMap::NFSMixMap()
{
}

// ---------------------------------------------------------------------------
// NFSMixMap::Init @0x82B47E48 -- Init(this, NFSMixMaster* a2)
//   *(this+0x70) = a2;                 m_pNFSMixMaster   (stw r4,0x70)
//   *(this+0x230) = 1.0; *(this+0x22C) = 1.0;  m_fDeltaTimeRatio[1]/[0] (stfs flt_82001C98)
//   *(this+0x98) = 0;  m_pStateProcs   (stw 0,0x98)
//   *(this+0xA8) = 0;  m_nStateMapCount(stw 0,0xA8)
//   *(this+0x04) = *(a2+8);  mNumStates = a2->mNumStates  (lwz 8(r4) -> stw 4)
// (flt_82001C98 == 1.0, cross-confirmed across the codebase.)
// ---------------------------------------------------------------------------
void NFSMixMap::Init(NFSMixMaster* lpMaster)
{
    m_pNFSMixMaster      = lpMaster;            // +0x70
    m_fDeltaTimeRatio[1] = 1.0f;               // +0x230
    m_fDeltaTimeRatio[0] = 1.0f;               // +0x22C
    m_pStateProcs        = 0;                  // +0x98
    m_nStateMapCount     = 0;                  // +0xA8
    mNumStates           = lpMaster->mNumStates; // +0x04 = *(a2+8)
}

// ---------------------------------------------------------------------------
// NFSMixMap::~NFSMixMap  (vtable slot 0)
// FLAG: the X360 scalar-deleting dtor frees the mixer-memory blocks allocated by
// AllocateMixerMemory/AllocateDMixIOArrays/AllocateInputArrays (the m_p*Block /
// m_p*Data_S/_U pointers). Those allocation methods are not yet bodied in this
// slice, so nothing is allocated here yet and the dtor is a no-op (matches the
// "no buffers allocated" state). The frees are wired in alongside the allocators.
// ---------------------------------------------------------------------------
NFSMixMap::~NFSMixMap()
{
}

// ---------------------------------------------------------------------------
// NFSMixMap::InitMixMap @0x82B4C258 (vtable slot 1) -- bind the loaded MixMap blob.
//   m_pMasterMixMap = lpMasterMixMap; (+0x8C)   m_pMixMap = lpMixMap; (+0x90)
//   m_pMMHdr = lpMixMap; (+0x74, the blob start IS the header)
//   m_MapType = lpMixMap[0]; (+0x78 = blob.MixMapID)
//   for (i=0; i<m_pMMHdr->NumStates; ++i) m_StateRefCount[i] = 0;  (+0x08..)
//   PreProcessMixMap();   (tail-call)
// ---------------------------------------------------------------------------
void NFSMixMap::InitMixMap(int* lpMixMap, NFSMixMap* lpMasterMixMap)
{
    m_pMasterMixMap = lpMasterMixMap;                              // +0x8C
    m_pMixMap       = lpMixMap;                                    // +0x90
    m_pMMHdr        = reinterpret_cast<stMixMapHeader*>(lpMixMap); // +0x74 (blob == header)
    m_MapType       = lpMixMap[0];                                 // +0x78 = blob.MixMapID

    for (int li = 0; li < m_pMMHdr->NumStates; ++li)              // *(blob+4) = NumStates
        m_StateRefCount[li] = 0;

    PreProcessMixMap();                                            // @0x82B48498
}

// ---------------------------------------------------------------------------
// NFSMixMap::ProcessMixMap @0x82B4C548 (vtable slot 2) -- per-frame drive.
// The delta-time / cam-state bookkeeping (head of the function) is reproduced faithfully.
// FLAG (deferred, RODATA-BLOCKED): the per-frame mix graph below the bookkeeping (curve
// eval over m_pCurveDataArray via NFSMixShape::GetCurveOutput, then mix-ctl/channel
// accumulation via NFSMixShape::GetdBFromQ15) needs the NFSMixShape conversion tables,
// which are not in the X360 export. flt_82F87958 (the delta-ratio divisor) is likewise an
// unrecovered rodata const. The bookkeeping is faithful; the DSP loop is wired once the
// NFSMixShape rodata is recovered (ProStreet .exe PE extraction).
// ---------------------------------------------------------------------------
void NFSMixMap::ProcessMixMap(float lfDeltaTime, int liCamState)
{
    m_fDeltaTimeRatio[1] = m_fDeltaTimeRatio[0];   // +0x230 = +0x22C (shift previous)
    m_PrevCamState       = m_CurCamState;          // +0x7C  = old +0x80
    m_fDeltaTimeRatio[0] = lfDeltaTime;            // +0x22C  FLAG: X360 = dt / flt_82F87958 (rodata)
    m_CurCamState        = liCamState;             // +0x80
    m_fDeltaTime         = lfDeltaTime;            // +0x84
    m_msDeltaTime        = lfDeltaTime * 1000.0f;  // +0x88  (flt_82009E10 == 1000.0, ms/sec)

    // FLAG: per-frame mix graph deferred (rodata-blocked NFSMixShape) -- see header note.
}

// ---------------------------------------------------------------------------
// NFSMixMap::PreProcessMixMap @0x82B48498 -- FLAG (deferred): ~1.5KB blob-walk that
// reads the MixMap section headers (state/ctl/3D/event/channel) and accumulates the
// m_n* counts AllocateMixerMemory then sizes its blocks from. Bodied in the allocation
// pass (it pairs with AllocateMixerMemory + the mixer allocator).
// ---------------------------------------------------------------------------
void NFSMixMap::PreProcessMixMap()
{
}

// ---------------------------------------------------------------------------
// Cursor / block-pointer helpers used by the allocate/assign passes.
// Each returns the current slot and advances its allocation cursor. ARTIST-verified.
// ---------------------------------------------------------------------------

// GetProcessMixCtlPtr @0x82B49500 -- next mix-ctl proc slot; bump the assigned count
// only when lbAdvance != 0 (X360 stride 8 == sizeof stMixCtlProc -> x64 array index).
stMixCtlProc* NFSMixMap::GetProcessMixCtlPtr(char lbAdvance)
{
    stMixCtlProc* lpProc = &m_pMixCtlProc[m_nAssignedMixCtlProc]; // r10*8 + (+0x190)
    if (lbAdvance)
        ++m_nAssignedMixCtlProc;                                  // *(+0x13C) += 1
    return lpProc;
}

// GetMasterChannelOutputArrayPtr @0x82B495F0 -- &outputBlock[cursor]; cursor += liN*16.
int* NFSMixMap::GetMasterChannelOutputArrayPtr(int liN)
{
    int* lpPtr = &m_pMasterChannelOutputArrayBlock[m_CurrentMasterOutputBlockOffset]; // off*4 + (+0x178)
    m_CurrentMasterOutputBlockOffset += liN * 16;                                     // (+0x228) += 16*liN
    return lpPtr;
}

// GetMasterChannelInputPtr @0x82B49618 -- &inputBlock[cursor]; cursor += liN.
int* NFSMixMap::GetMasterChannelInputPtr(int liN)
{
    int* lpPtr = &m_pMasterChannelInputs[m_CurrentMasterInputBlockOffset]; // off*4 + (+0x204)
    m_CurrentMasterInputBlockOffset += liN;                                // (+0x220) += liN
    return lpPtr;
}

// GetSubChannelInputPtr @0x82B49638 -- &subInputBlock[cursor]; cursor += liN.
int* NFSMixMap::GetSubChannelInputPtr(int liN)
{
    int* lpPtr = &m_pSubChannelInputs[m_CurrentSubInputBlockOffset]; // off*4 + (+0x208)
    m_CurrentSubInputBlockOffset += liN;                             // (+0x224) += liN
    return lpPtr;
}

// GetMapStateCopies @0x82B49658 -- the state's ref-count, or 0 if out of range.
int NFSMixMap::GetMapStateCopies(int liState)
{
    if (liState >= m_pMMHdr->NumStates)   // *(+0x74) -> NumStates (+0x04)
        return 0;
    return m_StateRefCount[liState];      // *(this + 8 + 4*liState)
}

// ---------------------------------------------------------------------------
// "Next slot" allocators @0x82B48F88 / 0x48FB8 / 0x49048 / 0x49078 / 0x490E0 /
// 0x49110 / 0x49178 / 0x491D8. Uniform: return &m_p<X>[m_nAssigned<X>]; if the
// advance flag is set, ++m_nAssigned<X>. X360 indexes with the record's byte stride;
// modelled as the real x64 array index. (Member-name mapping ARTIST-verified, e.g.
// GetNextMasterMixProc: counter +0x154, array +0x1C0.)
// ---------------------------------------------------------------------------
stEvtMixCtlProc* NFSMixMap::GetNextEvtMixCtlProc(char lbAdvance)
{
    stEvtMixCtlProc* lp = &m_pEvtMixCtlProc[m_nAssignedEvtMixCtlProc];
    if (lbAdvance) ++m_nAssignedEvtMixCtlProc;
    return lp;
}
stEvtMixCtlSharedData* NFSMixMap::GetNextEvtMixCtlShared(char lbAdvance)
{
    stEvtMixCtlSharedData* lp = &m_pEvtMixCtlData_S[m_nAssignedEvtMixCtlShared];
    if (lbAdvance) ++m_nAssignedEvtMixCtlShared;
    return lp;
}
st3DMixCtlProc* NFSMixMap::GetNext3DMixCtlProc(char lbAdvance)
{
    st3DMixCtlProc* lp = &m_p3DMixCtlProc[m_nAssigned3DMixCtlProc];
    if (lbAdvance) ++m_nAssigned3DMixCtlProc;
    return lp;
}
st3DMixCtlSharedData* NFSMixMap::GetNext3DMixCtlShared(char lbAdvance)
{
    st3DMixCtlSharedData* lp = &m_p3DMixCtlData_S[m_nAssigned3DMixCtlShared];
    if (lbAdvance) ++m_nAssigned3DMixCtlShared;
    return lp;
}
stMasterMixChProc* NFSMixMap::GetNextMasterMixProc(char lbAdvance)
{
    stMasterMixChProc* lp = &m_pMasterChProc[m_nAssignedMasterMixProc]; // +0x1C0 / +0x154
    if (lbAdvance) ++m_nAssignedMasterMixProc;
    return lp;
}
stMasterMixChSharedData* NFSMixMap::GetNextMasterMixShared(char lbAdvance)
{
    stMasterMixChSharedData* lp = &m_pMasterChData_S[m_nAssignedMasterMixShared];
    if (lbAdvance) ++m_nAssignedMasterMixShared;
    return lp;
}
stSubMixChProc* NFSMixMap::GetNextSubMixProc(char lbAdvance)
{
    stSubMixChProc* lp = &m_pSubChProc[m_nAssignedSubMixProc];
    if (lbAdvance) ++m_nAssignedSubMixProc;
    return lp;
}
stMixChSharedData* NFSMixMap::GetNextSubMixShared(char lbAdvance)
{
    stMixChSharedData* lp = &m_pSubChData_S[m_nAssignedSubMixShared];
    if (lbAdvance) ++m_nAssignedSubMixShared;
    return lp;
}

// ---------------------------------------------------------------------------
// NFSMixMap::ResetMapData @0x82B482B0 -- zero every runtime accumulator counter,
// allocation cursor, and allocated-block pointer (called before (re)building the map).
// Store-for-store in the ARTIST order (all writes are 0). Preserves the identity fields
// (vtable / mNumStates / m_StateRefCount / mpMixerInterface / m_pNFSMixMaster / m_pMMHdr /
//  m_MapType / m_fDeltaTime* / m_pMasterMixMap / m_pMixMap / m_nStateMapCount / DMixIO
//  arrays / m_fDeltaTimeRatio).
// ---------------------------------------------------------------------------
void NFSMixMap::ResetMapData()
{
    m_nAssignedMixMapStates = 0;         // +0xC0
    m_MixCtlsAdded = 0;                  // +0x1D0
    m_SharedMixCtlCount = 0;             // +0xC4
    m_nAssignedMixCtlProc = 0;           // +0x13C
    m_AssignedMixCtlsShared = 0;         // +0x140
    m_AssignedMixCtlsUnique = 0;         // +0x144
    m_ScaleParamsAdded = 0;              // +0xD4
    m_ScaleParamsIDCount = 0;            // +0xD8
    for (int li = 0; li < 20; ++li)      // +0xDC loop: m_CurveProcsTotal[10][2]
        reinterpret_cast<int*>(m_CurveProcsTotal)[li] = 0;
    m_3DMixCtlsAdded = 0;                // +0x1D4
    m_EventCtlsAdded = 0;               // +0x1E0
    m_nAssignedDMixIOBlocks = 0;         // +0xB4
    m_nAssignedDMix3DIOBlocks = 0;       // +0xB8
    m_nAssignedInputBlocks = 0;          // +0xBC
    m_nAssigned3DMixCtlProc = 0;         // +0x160
    m_nAssigned3DMixCtlShared = 0;       // +0x164
    m_nAssigned3DMixCtlUnique = 0;       // +0x168
    m_nAssignedEvtMixCtlProc = 0;        // +0x16C
    m_nAssignedEvtMixCtlShared = 0;      // +0x170
    m_nAssignedEvtMixCtlUnique = 0;      // +0x174
    m_PrevCamState = 0;                  // +0x7C
    m_CurCamState = 0;                   // +0x80
    m_Shared3DMixCtlCount = 0;           // +0x134
    m_SharedEvtMixCtlCount = 0;          // +0x138
    m_SubMixChannelsAdded = 0;           // +0x1D8
    m_SharedSubMixCount = 0;             // +0x12C
    m_nAssignedSubMixProc = 0;           // +0x148
    m_nAssignedSubMixShared = 0;         // +0x14C
    m_nAssignedSubMixUnique = 0;         // +0x150
    m_MasterChannelsAdded = 0;           // +0x1DC
    m_SharedMasterMixCount = 0;          // +0x130
    m_nAssignedMasterMixProc = 0;        // +0x154
    m_nAssignedMasterMixShared = 0;      // +0x158
    m_nAssignedMasterMixUnique = 0;      // +0x15C
    m_CurrentStateProcBlockOffset = 0;   // +0x20C
    m_nTotalMasterChannelInputs = 0;     // +0x1E8
    m_CurrentMasterInputBlockOffset = 0; // +0x220
    m_CurrentSubInputBlockOffset = 0;    // +0x224
    m_CurrentMasterOutputBlockOffset = 0;// +0x228
    m_CurrentMasterChannelPtrBlockOffset = 0; // +0x21C
    m_CurrentSubChannelPtrBlockOffset = 0;    // +0x218
    m_Current3DMixCtlPtrBlockOffset = 0;      // +0x214
    m_CurrentEvtMixCtlPtrBlockOffset = 0;     // +0x210
    m_nTotalMasterChannel3DOutputs = 0;  // +0x1EC
    m_nTotalSubChannelInputs = 0;        // +0x1F0
    m_nTotalSubChannel3DOutputs = 0;     // +0x1F4
    m_nTotalUniqueMasterChannels = 0;    // +0x1F8
    m_SFXOBJsAdded = 0;                  // +0x1C4
    m_SFXCTLsAdded = 0;                  // +0x1C8
    m_DataProcsAdded = 0;               // +0x1CC
    m_n3DCamStatesAdded = 0;             // +0x1E4
    m_SharedMixCtlsAssigned = 0;         // +0xC8
    m_UniqueMixCtlsAssigned = 0;         // +0xCC
    m_CurveProcsAdded = 0;               // +0xD0
    m_CurrentMasterInputOffset = 0;      // +0x1FC
    m_CurrentSubInputOffset = 0;         // +0x200
    m_pStateProcMemBlock = 0;            // +0x9C
    m_pDynMixInputBlocks = 0;            // +0x17C
    m_pScalePtrArray = 0;                // +0x180
    m_pCurveDataArray = 0;               // +0x184
    m_pMixCtlData_S = 0;                 // +0x188
    m_pMixCtlData_U = 0;                 // +0x18C
    m_pMixCtlProc = 0;                   // +0x190
    m_pEvtMixCtlProc = 0;                // +0x194
    m_pEvtMixCtlData_S = 0;              // +0x198
    m_pEvtMixCtlData_U = 0;              // +0x19C
    m_p3DMixCtlProc = 0;                 // +0x1A0
    m_p3DMixCtlData_S = 0;               // +0x1A4
    m_p3DMixCtlData_U = 0;               // +0x1A8
    m_pSubChData_S = 0;                  // +0x1AC
    m_pSubChData_U = 0;                  // +0x1B0
    m_pSubChProc = 0;                    // +0x1B4
    m_pMasterChData_S = 0;               // +0x1B8
    m_pMasterChData_U = 0;               // +0x1BC
    m_pMasterChProc = 0;                 // +0x1C0
    m_pMasterChannelInputs = 0;          // +0x204
    m_pSubChannelInputs = 0;             // +0x208
    m_pMasterChannelOutputArrayBlock = 0;// +0x178
}
