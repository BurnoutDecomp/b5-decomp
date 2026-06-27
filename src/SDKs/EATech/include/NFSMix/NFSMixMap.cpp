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
