#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp" // stMixCtlProc / stMixMapHeader for the helpers
#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp" // GetMasterMixChProc / placement-construct
#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"    // per-frame curve / dB / Q15 conversions
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp" // g_pMixerAllocator (off_83250004)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include <new>                                            // placement new (NFSMixMapState object memory)

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
// Delta-time / cam-state bookkeeping, then the per-frame DSP: (1) evaluate every
// curve-proc into its Q15 output via NFSMixShape::GetCurveOutput; (2) compound each
// mix-control's dB/scale ratios (NFSMixShape::GetdBFromQ15); (3) drive the 3D / event /
// sub / master channel passes. NFSMixShape is now homed, so the DSP is wired.
// FLAG (RODATA): the delta-ratio store still uses the raw dt -- the X360 divides by
// flt_82F87958 (an unrecovered rodata const, not in this per-function export). Kept as a
// faithful-shape store with the divisor FLAGged (matches the committed treatment).
// ---------------------------------------------------------------------------
void NFSMixMap::ProcessMixMap(float lfDeltaTime, int liCamState)
{
    m_fDeltaTimeRatio[1] = m_fDeltaTimeRatio[0];   // +0x230 = +0x22C (shift previous)
    m_PrevCamState       = m_CurCamState;          // +0x7C  = old +0x80
    m_fDeltaTimeRatio[0] = lfDeltaTime;            // +0x22C  FLAG: X360 = dt / flt_82F87958 (rodata)
    m_CurCamState        = liCamState;             // +0x80
    m_fDeltaTime         = lfDeltaTime;            // +0x84
    m_msDeltaTime        = lfDeltaTime * 1000.0f;  // +0x88  (flt_82009E10 == 1000.0, ms/sec)

    // (1) curve pass: each curve-proc's Q15 output = curve(nINPUTID&0xF) at its live input.
    for (int li = 0; li < m_CurveProcsAdded; ++li)  // +0xD0
    {
        stCurveDataProc& lrProc = m_pCurveDataArray[li];               // +0x184, stride 16
        lrProc.Q15Output = NFSMixShape::GetCurveOutput(
            lrProc.nINPUTID & 0xF, *lrProc.pInputParam, /*lbDb=*/0);   // +0xC
    }

    // (2) mix-control pass: compound curve dB + shared offset, scaled by the unique
    //     scale-ratio product, into each mix-control's CmpdBOut.
    for (int li = 0; li < m_MixCtlsAdded; ++li)                        // +0x1D0
    {
        stMixCtlProc&       lrProc = m_pMixCtlProc[li];                // +0x190, stride 8
        stMixCtlUniqueData* lpU    = lrProc.pudata;                    // +4
        stMixCtlSharedData* lpS    = lrProc.psdata;                    // +0

        const int liDb = NFSMixShape::GetdBFromQ15(
            0x7FFF - (((0x7FFF - lpU->pstCurveData->Q15Output) * lpS->nRatio) >> 15));

        int** lppScale = reinterpret_cast<int**>(lpU->ppScaleRatios);  // int** (double-deref)
        int liScale = 0x7FFF;
        if (lppScale)
        {
            // X360 lbz at the int's byte-0 == the big-endian MSB (the scale count).
            int liN = (lpS->pstMixCtlParms->nUScaleCntSwing >> 24) & 0xFF;
            while (liN) { --liN; const int liV = **lppScale++; liScale = (liV * liScale) >> 15; }
        }
        lpU->CmpdBOut = (liScale * (liDb + lpS->nOffset)) >> 15;       // +8
    }

    Update3DMixCtls();      // @0x82B4BB98 (blocked -- declared)
    UpdateEvtMixCtls();     // @0x82B4C2A8 (blocked -- declared)
    UpdateSubChannels();    // @0x82B4AC10
    UpdateMasterChannels(); // @0x82B4ACD8 (blocked -- declared)
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

// ===========================================================================
//  Wave-F1 additions -- the remaining faithfully-recoverable NFSMixMap bodies.
//  Store-for-store from BURNOUT_X360_ARTIST.XEX. See NFSMixMap.hpp for the layout.
// ===========================================================================

// ---------------------------------------------------------------------------
// NFSMixMap::AssignSFXCallbacks @0x82B481B8 -- *(this+0x6C) = a2 (the driving host).
// ---------------------------------------------------------------------------
void NFSMixMap::AssignSFXCallbacks(void* lpOwner)
{
    mpMixerInterface = reinterpret_cast<Nicotine::IDynamicMixer*>(lpOwner); // +0x6C
}

// ---------------------------------------------------------------------------
// "Next slot" UNIQUE-record allocators (twins of the committed shared/proc getters).
// ---------------------------------------------------------------------------

// GetNextEvtMixCtlUnique @0x82B48FF0 -- zero the fresh record (flt_82001CC0 == 0.0),
// return &m_pEvtMixCtlData_U[m_nAssignedEvtMixCtlUnique]; bump the count if advancing.
stEvtMixCtlUniqueData* NFSMixMap::GetNextEvtMixCtlUnique(char lbAdvance)
{
    stEvtMixCtlUniqueData* lp = &m_pEvtMixCtlData_U[m_nAssignedEvtMixCtlUnique]; // +0x19C / +0x174
    lp->msStageElapsed = 0.0f;              // +0x04
    lp->qStart         = 0;                 // +0x08
    lp->msStart        = 0.0f;              // +0x0C
    lp->eCurrentStage  = eEnvelopeStage_Off;// +0x00 (== 0)
    lp->qoutput        = 0;                 // +0x1C
    lp->output         = 0;                 // +0x18
    if (lbAdvance) ++m_nAssignedEvtMixCtlUnique;
    return lp;
}

// GetNextMasterMixUnique @0x82B49140 -- &m_pMasterChData_U[m_nAssignedMasterMixUnique].
stMasterMixChUniqueData* NFSMixMap::GetNextMasterMixUnique(char lbAdvance)
{
    stMasterMixChUniqueData* lp = &m_pMasterChData_U[m_nAssignedMasterMixUnique]; // +0x1BC / +0x15C
    if (lbAdvance) ++m_nAssignedMasterMixUnique;
    return lp;
}

// GetNextSubMixUnique @0x82B491A8 -- &m_pSubChData_U[m_nAssignedSubMixUnique].
stMixChUniqueData* NFSMixMap::GetNextSubMixUnique(char lbAdvance)
{
    stMixChUniqueData* lp = &m_pSubChData_U[m_nAssignedSubMixUnique]; // +0x1B0 / +0x150
    if (lbAdvance) ++m_nAssignedSubMixUnique;
    return lp;
}

// GetNextMapState @0x82B49210 -- byte-offset cursor over the NFSMixMapState object memory
// (m_pStateProcMemBlock, +0x9C). FLAG (PC pointer-width): the X360 cursor steps 0x60
// (== X360 sizeof NFSMixMapState); the object block is allocated at PC sizeof stride, so
// the raw 0x60 step diverges on x64. Kept X360-faithful; reconciled with AllocateMixerMemory's
// stride when the (currently absent) NFSMixMapState build chain lands.
NFSMixMapState* NFSMixMap::GetNextMapState(char lbAdvance)
{
    const int liOff = m_CurrentStateProcBlockOffset;   // +0x20C (byte offset)
    NFSMixMapState* lp = reinterpret_cast<NFSMixMapState*>(
        reinterpret_cast<char*>(m_pStateProcMemBlock) + liOff);
    if (lbAdvance) m_CurrentStateProcBlockOffset = liOff + 0x60;
    return lp;
}

// ---------------------------------------------------------------------------
// NFSMixMap::GetCurveDataPtr @0x82B49238 -- find (or append) the curve-proc slot for a
// mix-control parameter. The curve type is bits[24..27] of the param id; the slot lives
// in the m_pCurveDataArray sub-range for that type (offset = sum of earlier types' curve
// counts, held in m_CurveProcsTotal[type][0]); m_CurveProcsTotal[type][1] tracks how many
// are already present. On a miss it appends a fresh proc (Q15Output = 0x7FFF).
// ---------------------------------------------------------------------------
stCurveDataProc* NFSMixMap::GetCurveDataPtr(int* lpParam)
{
    stCurveDataProc* lpBase = m_pCurveDataArray;   // +0x184
    if (!lpBase)
        return 0;

    const int liType = (*lpParam >> 24) & 0xF;
    int liOffset = 0;
    for (int li = 0; li < liType; ++li)
        liOffset += m_CurveProcsTotal[li][0];      // +0xDC..: sum earlier types' counts

    stCurveDataProc* lpProc = &lpBase[liOffset];
    const int liAdded = m_CurveProcsTotal[liType][1];
    bool lbAppend = (liAdded <= 0);
    if (!lbAppend)
    {
        int li = 0;
        while (lpProc->nINPUTID != *lpParam)
        {
            ++li;
            ++lpProc;
            if (li >= m_CurveProcsTotal[liType][1]) { lbAppend = true; break; }
        }
    }
    if (lbAppend)
    {
        m_CurveProcsTotal[liType][1] = liAdded + 1;
        lpProc->Q15Output   = 0x7FFF;   // +0xC
        lpProc->pInputParam = 0;        // +0x4
        lpProc->nINPUTID    = *lpParam; // +0x0
    }
    return lpProc;
}

// ---------------------------------------------------------------------------
// NFSMixMap::AddScaleIDs @0x82B492F8 -- expand a mix-control's scale-input list into the
// packed scale-ptr array. For each scale entry: if its state matches the control's own
// state, one packed id is written; otherwise one packed id per active copy of that state
// (m_StateRefCount[state]). The produced count is written back into the param blob's
// count byte and added to m_ScaleParamsIDCount. Returns the base of the written range.
// FLAG (PC pointer-width): the packed 32-bit ids are stored through the m_pScalePtrArray
// pointer slots (reinterpret) so the stride matches ConnectMixMap's later object-ptr fill.
// ---------------------------------------------------------------------------
int* NFSMixMap::AddScaleIDs(unsigned short* lpScaleParams, int liProcIdx)
{
    const int liCount = lpScaleParams[2] & 0x1F;         // u16 @ byte4, low 5 bits
    if (liCount == 0)
        return 0;

    int** lpArray = m_pScalePtrArray;                    // +0x180
    int** lpBase  = &lpArray[m_ScaleParamsIDCount];
    const int liSelfType = lpScaleParams[0] & 0xFF;      // low byte of u16 @ byte0
    const int* lpEntry   = reinterpret_cast<const int*>(lpScaleParams + 4); // scale list @ byte8

    int liAdded = 0;
    for (int li = 0; li < liCount; ++li)
    {
        const int liVal      = *lpEntry++;
        const int liEntryType = (liVal >> 16) & 0xFF;
        if (liEntryType == liSelfType)
        {
            ++liAdded;
            lpArray[m_ScaleParamsIDCount + li] =
                reinterpret_cast<int*>(static_cast<intptr_t>((liProcIdx << 11) | liVal));
        }
        else
        {
            const int liCopies = m_StateRefCount[liEntryType]; // this+8+4*type
            for (int lj = 0; lj < liCopies; ++lj)
            {
                ++liAdded;
                lpArray[m_ScaleParamsIDCount + lj + li] =
                    reinterpret_cast<int*>(static_cast<intptr_t>((lj << 11) | liVal));
            }
        }
    }

    // write the produced count into the top byte of the u32 @ byte4 of the param blob.
    int* lpCountWord = reinterpret_cast<int*>(lpScaleParams + 2); // byte4
    *lpCountWord = (liAdded << 24) | (*lpCountWord & 0xFFFFFF);
    m_ScaleParamsIDCount += liAdded;
    return reinterpret_cast<int*>(lpBase);
}

// ---------------------------------------------------------------------------
// NFSMixMap::GetMasterMixChProc @0x82B4A840 -- route a packed master-channel id to its
// per-state proc: state = bits[16..23], copy = bits[11..15], proc = bits[0..7]. The X360
// host-assert ("State out of bounds.") collapses to CGS_ASSERT.
// ---------------------------------------------------------------------------
stMasterMixChProc* NFSMixMap::GetMasterMixChProc(int liPackedID)
{
    const int liState = (liPackedID >> 16) & 0xFF;
    const int liCopy  = (liPackedID >> 11) & 0x1F;

    const bool lbInBounds = (m_pMMHdr != 0) && (liState < m_pMMHdr->NumStates); // +0x74 -> NumStates
    CGS_ASSERT(lbInBounds, "State out of bounds.");

    NFSMixMapState* lpState = m_pStateProcs[liState];  // +0x98
    if (lpState)
        return lpState->GetMasterMixChProc(
            static_cast<unsigned char>(liPackedID & 0xFF), liCopy);
    return 0;
}

// ---------------------------------------------------------------------------
// NFSMixMap::UpdateSubChannels @0x82B4AC10 -- per-frame: for each active sub-channel, sum
// its input Q15 values into Output, then clamp to the shared param's [lower,upper] swing.
// FLAG (PC pointer-width): the input array holds pointers-to-values on the runtime side
// (double-deref), matching ConnectMixMap's object-ptr fill; modelled with int** casts.
// ---------------------------------------------------------------------------
void NFSMixMap::UpdateSubChannels()
{
    stSubMixChProc* lpProc = m_pSubChProc;             // +0x1B4
    for (int li = 0; li < m_SubMixChannelsAdded; ++li, ++lpProc) // +0x1D8, stride 8
    {
        stMixChUniqueData* lpU = lpProc->pMixChData_U; // +0x4
        stMixChSharedData* lpS = lpProc->pMixChData_S; // +0x0
        int** lppInputs = reinterpret_cast<int**>(lpU->pInputs);
        if (!lppInputs)
            continue;

        int liN = lpS->NumInputs & 0xFF;               // +0x8, low byte
        lpU->Output = 0;                               // +0x4
        int** lpp = lppInputs;
        while (liN) { --liN; const int liV = **lpp++; lpU->Output += liV; }

        const int liSwing = lpS->pMapParams->UpperLowerSwing; // *pMapParams -> +0x4
        const int liMin = liSwing | static_cast<int>(0xFFFF0000);
        const int liMax = (liSwing >> 16) & 0x7FFF;
        if (lpU->Output > liMax) lpU->Output = liMax;
        if (lpU->Output < liMin) lpU->Output = liMin;
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::AllocateMixerMemory @0x82B48AF8 -- allocate every runtime block of the mixer
// graph (proc arrays + shared/unique record arrays + input/output blocks) from the mixer
// allocator, sized from the counts PreProcessMixMap accumulated, and construct the
// NFSMixMapState object memory. FLAG (PC pointer-width): the X360 sizes each block with
// 32-bit element strides (4/8/12/16/20/32/64/96); the PC reconstruction sizes them with
// sizeof(element) so the typed accessors stay self-consistent on x64. Blocks the runtime
// fills with pointers (state procs, input arrays, scale array) use sizeof(void*).
// ---------------------------------------------------------------------------
int NFSMixMap::AllocateMixerMemory()
{
    MixerAllocator* lpAlloc = g_pMixerAllocator;
    const int liNumStates = m_pMMHdr->NumStates;   // +0x74 -> NumStates

    if (!m_pStateProcs)                             // +0x98
        m_pStateProcs = static_cast<NFSMixMapState**>(
            lpAlloc->Allocate(sizeof(NFSMixMapState*) * liNumStates, 16, "Dyn Mix Proc Array"));
    for (int li = 0; li < liNumStates; ++li)
        m_pStateProcs[li] = 0;

    m_pMasterChannelInputs = static_cast<int*>(     // +0x204 (ptr array on runtime)
        lpAlloc->Allocate(sizeof(int*) * m_nTotalMasterChannelInputs, 16, "Master Channel Input Array Block"));
    m_pSubChannelInputs = static_cast<int*>(        // +0x208 (ptr array on runtime)
        lpAlloc->Allocate(sizeof(int*) * m_nTotalSubChannelInputs, 16, "Sub Channel Input Array Block"));
    m_pMasterChannelOutputArrayBlock = static_cast<int*>( // +0x178 (16 int slots per channel)
        lpAlloc->Allocate(64 * m_nTotalUniqueMasterChannels, 16, "Master Channel Output Array Block"));

    NFSMixMapState* lpStateMem = static_cast<NFSMixMapState*>( // +0x9C object memory
        lpAlloc->Allocate(sizeof(NFSMixMapState) * m_nStateMapCount, 16, "NFSMixMapState Object Memory"));
    m_pStateProcMemBlock = reinterpret_cast<NFSMixMapState**>(lpStateMem);
    for (int li = 0; li < m_nStateMapCount; ++li)   // +0xA8
        new (&lpStateMem[li]) NFSMixMapState();

    m_pScalePtrArray = static_cast<int**>(          // +0x180 (packed ids / object ptrs)
        lpAlloc->Allocate(sizeof(int*) * m_ScaleParamsAdded, 16, "Scale Input Ptr Array Block"));
    m_pCurveDataArray = static_cast<stCurveDataProc*>( // +0x184
        lpAlloc->Allocate(sizeof(stCurveDataProc) * m_CurveProcsAdded, 16, "Curve Proc Data Array"));

    m_pMixCtlData_S = static_cast<stMixCtlSharedData*>(  // +0x188
        lpAlloc->Allocate(sizeof(stMixCtlSharedData) * m_SharedMixCtlCount, 16, "NFSMixCtl Shared Data Array"));
    m_pMixCtlData_U = static_cast<stMixCtlUniqueData*>(  // +0x18C
        lpAlloc->Allocate(sizeof(stMixCtlUniqueData) * m_MixCtlsAdded, 16, "NFSMixCtl Unique Data Array"));
    m_pMixCtlProc = static_cast<stMixCtlProc*>(          // +0x190
        lpAlloc->Allocate(sizeof(stMixCtlProc) * m_MixCtlsAdded, 16, "NFSMixCtl Process Data Array"));

    m_pSubChData_S = static_cast<stMixChSharedData*>(    // +0x1AC
        lpAlloc->Allocate(sizeof(stMixChSharedData) * m_SharedSubMixCount, 16, "SubMix Shared Data Block"));
    m_pSubChData_U = static_cast<stMixChUniqueData*>(    // +0x1B0
        lpAlloc->Allocate(sizeof(stMixChUniqueData) * m_SubMixChannelsAdded, 16, "SubMix Unique Data Block"));
    m_pSubChProc = static_cast<stSubMixChProc*>(         // +0x1B4
        lpAlloc->Allocate(sizeof(stSubMixChProc) * m_SubMixChannelsAdded, 16, "SubMix Proc Data Block"));

    m_pMasterChData_S = static_cast<stMasterMixChSharedData*>( // +0x1B8
        lpAlloc->Allocate(sizeof(stMasterMixChSharedData) * m_SharedMasterMixCount, 16, "MasterMix Shared Data Block"));
    m_pMasterChData_U = static_cast<stMasterMixChUniqueData*>( // +0x1BC
        lpAlloc->Allocate(sizeof(stMasterMixChUniqueData) * m_MasterChannelsAdded, 16, "MasterMix Unique Data Block"));
    m_pMasterChProc = static_cast<stMasterMixChProc*>(        // +0x1C0
        lpAlloc->Allocate(sizeof(stMasterMixChProc) * m_MasterChannelsAdded, 16, "MasterMix Proc Data Block"));

    m_p3DMixCtlData_S = static_cast<st3DMixCtlSharedData*>(   // +0x1A4
        lpAlloc->Allocate(sizeof(st3DMixCtlSharedData) * m_3DMixCtlsAdded, 16, "3DMixCtl Shared Data Block"));
    m_p3DMixCtlData_U = static_cast<st3DMixCtlUniqueData*>(   // +0x1A8
        lpAlloc->Allocate(sizeof(st3DMixCtlUniqueData) * m_3DMixCtlsAdded, 16, "3DMixCtl Unique Data Block"));
    m_p3DMixCtlProc = static_cast<st3DMixCtlProc*>(          // +0x1A0
        lpAlloc->Allocate(sizeof(st3DMixCtlProc) * m_3DMixCtlsAdded, 16, "3DMixCtl Proc Data Block"));

    m_pEvtMixCtlData_S = static_cast<stEvtMixCtlSharedData*>( // +0x198
        lpAlloc->Allocate(sizeof(stEvtMixCtlSharedData) * m_EventCtlsAdded, 16, "EvtMixCtl Shared Data Block"));
    m_pEvtMixCtlData_U = static_cast<stEvtMixCtlUniqueData*>( // +0x19C
        lpAlloc->Allocate(sizeof(stEvtMixCtlUniqueData) * m_EventCtlsAdded, 16, "EvtMixCtl Unique Data Block"));
    m_pEvtMixCtlProc = static_cast<stEvtMixCtlProc*>(        // +0x194
        lpAlloc->Allocate(sizeof(stEvtMixCtlProc) * m_EventCtlsAdded, 16, "EvtMixCtl Proc Data Block"));
    return 0;
}
