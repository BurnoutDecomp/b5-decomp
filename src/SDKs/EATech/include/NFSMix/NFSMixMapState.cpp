#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp" // complete proc records for the accessors
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"     // owning map: Get* allocators + m_StateRefCount

// ===========================================================================
//  NFSMixMapState -- ctor/dtor + the ARTIST-verified Initialize / GetStateRefCount
//  and the GetXxxProc accessors. Store-for-store from BURNOUT_X360_ARTIST.XEX
//  (see NFSMixMapState.hpp).
// ===========================================================================

NFSMixMapState::NFSMixMapState() {}   // vtable install (reproduced by the virtual dtor)
NFSMixMapState::~NFSMixMapState() {}  // vtable slot 0

// ---------------------------------------------------------------------------
// NFSMixMapState::Initialize @0x82B4C6B0 -- Initialize(this, lpMap, stateIdx, copies, objIdx)
//   m_pNFSMixMap = lpMap;        (+0x04)   m_StateIndex     = stateIdx; (+0x38)
//   m_NumStateCopies = copies;   (+0x44)   m_ObjectIndex    = objIdx;   (+0x40)
//   if (objIdx == 0) { m_ThisStateRefCnt = 1; return; }   (+0x3C)
//   else { ++m_pFirstInstance->m_ThisStateRefCnt;          (the "master" copy)
//          for (i=0; i<objIdx; ++i)                          (propagate to all copies,
//              m_pFirstInstance[i].m_ThisStateRefCnt = m_pFirstInstance->m_ThisStateRefCnt; }
//                                                            X360 instance stride 0x60)
// ---------------------------------------------------------------------------
void NFSMixMapState::Initialize(NFSMixMap* lpMap, int liStateIndex,
                                int liNumStateCopies, int liObjectIndex)
{
    m_pNFSMixMap     = lpMap;            // +0x04
    m_StateIndex     = liStateIndex;    // +0x38
    m_NumStateCopies = liNumStateCopies;// +0x44
    m_ObjectIndex    = liObjectIndex;   // +0x40

    if (liObjectIndex == 0)
    {
        m_ThisStateRefCnt = 1;          // +0x3C
        return;
    }

    // A copy: bump the master instance's ref-count, then stamp it onto every copy.
    ++m_pFirstInstance->m_ThisStateRefCnt;
    for (int li = 0; li < liObjectIndex; ++li)
        m_pFirstInstance[li].m_ThisStateRefCnt = m_pFirstInstance->m_ThisStateRefCnt;
}

// ---------------------------------------------------------------------------
// NFSMixMapState::GetStateRefCount @0x82B4C718
//   return m_pFirstInstance->m_ThisStateRefCnt;   (lwz 0x5C ; lwz 0x3C)
// ---------------------------------------------------------------------------
int NFSMixMapState::GetStateRefCount()
{
    return m_pFirstInstance->m_ThisStateRefCnt;
}

// ---------------------------------------------------------------------------
// GetXxxProc accessors @0x82B4C728 / 0x770 / 0x7B8 / 0x800 / 0x848.
// Each: state = &m_pFirstInstance[copyIdx] (X360 stride 0x60); if the proc index is
// within that copy's count, return &<paramBase>[procIdx]; else 0. The X360 stride for
// the proc array is 8 (a {shared*,unique*} pair); modelled as the real x64 array index.
// ---------------------------------------------------------------------------
stMixCtlProc* NFSMixMapState::GetMixCtlProc(unsigned char procIdx, int copyIdx)
{
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (!st || static_cast<int>(procIdx) >= st->m_MixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.pMixCtlProcs[procIdx];
}

st3DMixCtlProc* NFSMixMapState::Get3DMixCtlProc(unsigned char procIdx, int copyIdx)
{
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (!st || static_cast<int>(procIdx) >= st->m_3DMixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.p3DMixCtlProc[procIdx];
}

stEvtMixCtlProc* NFSMixMapState::GetEvtMixCtlProc(unsigned char procIdx, int copyIdx)
{
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (!st || static_cast<int>(procIdx) >= st->m_EvtMixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.pEvtMixCtlProc[procIdx];
}

stSubMixChProc* NFSMixMapState::GetSubMixChProc(unsigned char procIdx, int copyIdx)
{
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (!st || static_cast<int>(procIdx) >= st->m_SubMixChannelsAdded)
        return 0;
    return &st->m_MixStateParams.pSubMixChProcs[procIdx];
}

stMasterMixChProc* NFSMixMapState::GetMasterMixChProc(unsigned char procIdx, int copyIdx)
{
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (!st || static_cast<int>(procIdx) >= st->m_MasterChannelsAdded)
        return 0;
    return &st->m_MixStateParams.pMasterMixChProcs[procIdx];
}

// ---------------------------------------------------------------------------
// NFSMixMapState::GetMixMapProc @0x82B4D648
//   return &m_pFirstInstance[liIndex];   (96*idx + *(this+0x5C), stride 0x60)
// ---------------------------------------------------------------------------
NFSMixMapState* NFSMixMapState::GetMixMapProc(int liIndex)
{
    return &m_pFirstInstance[liIndex];
}

// ---------------------------------------------------------------------------
// NFSMixMapState::AddMixState @0x82B4D660
//   this->m_pFirstInstance = lpFirstInstance;
//   if (liObjectIndex) { target = map->GetNextMapState(1); target->m_pFirstInstance = lpFirstInstance; }
//   else               { target = this; }
//   target->Initialize(map, m_StateIndex, /*copies*/ 1, liObjectIndex ? liObjectIndex : 0);  (virtual)
// The virtual dispatches through target's vtable slot 1 (== Initialize).
// ---------------------------------------------------------------------------
NFSMixMapState* NFSMixMapState::AddMixState(int liObjectIndex, NFSMixMapState* lpFirstInstance)
{
    m_pFirstInstance = lpFirstInstance;

    NFSMixMapState* lpTarget;
    int liObjIdx;
    if (liObjectIndex)
    {
        lpTarget = m_pNFSMixMap->GetNextMapState(1);
        lpTarget->m_pFirstInstance = lpFirstInstance;
        liObjIdx = liObjectIndex;
    }
    else
    {
        lpTarget = this;
        liObjIdx = 0;
    }

    lpTarget->Initialize(m_pNFSMixMap, m_StateIndex, 1, liObjIdx);
    return lpTarget;
}

// ---------------------------------------------------------------------------
// NFSMixMapState::CreateSubMixChannels @0x82B4CB00
//   Build the per-state sub-mix channel procs from the serialised MixMap sub-mix
//   section (m_pMMStateHdr blob + OffsetSubMixData). The section is a stMixChHdr
//   followed by variable-stride channel entries. The "master" state (m_ObjectIndex==0)
//   allocates fresh shared records; a copy reuses the first-instance's shared record.
// ---------------------------------------------------------------------------
void NFSMixMapState::CreateSubMixChannels()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    int liOffset = m_pMMStateHdr->OffsetSubMixData;
    m_SubMixChannelsAdded = 0;
    if (liOffset < 0)
        return;

    m_SubMixChannelsAdded = 0;
    stMixChHdr* lpHdr = reinterpret_cast<stMixChHdr*>(
        reinterpret_cast<char*>(m_pMMStateHdr) + liOffset);
    m_pSubChHdr = lpHdr;
    if (lpHdr->NumMixChannels <= 0)
        return;

    m_MixStateParams.pSubMixChProcs = lpMap->GetNextSubMixProc(0);

    // Serialised channel entries follow the 16-byte section header (external blob;
    // stride = (entry[0]&0xFF)+2 dwords).
    int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);

    int liChannel = 0;
    do
    {
        stMixChSharedData* lpShared;
        stSubMixChProc*    lpProc;
        stMixChUniqueData* lpUnique;

        if (m_ObjectIndex)
        {
            lpShared = m_pFirstInstance->m_MixStateParams.pSubMixChProcs[liChannel].pMixChData_S;
            lpProc   = lpMap->GetNextSubMixProc(1);
            lpUnique = lpMap->GetNextSubMixUnique(1);
        }
        else
        {
            lpShared = lpMap->GetNextSubMixShared(1);
            lpProc   = lpMap->GetNextSubMixProc(1);
            lpUnique = lpMap->GetNextSubMixUnique(1);
            lpShared->MIXCHINID  = ((lpEntry[0] << 8) & 0xFF0000) | 0x20000000
                                 | (lpEntry[0] & 0x10000000) | liChannel;
            lpShared->pMapParams = reinterpret_cast<stSubMixChParams*>(lpEntry);
            lpShared->NumInputs  = lpEntry[0] & 0xFF;
        }

        lpProc->pMixChData_S = lpShared;
        lpUnique->Output     = 0;
        lpUnique->pInputs    = 0;
        lpProc->pMixChData_U = lpUnique;

        ++liChannel;
        ++m_SubMixChannelsAdded;
        lpEntry += (lpEntry[0] & 0xFF) + 2;
    }
    while (liChannel < m_pSubChHdr->NumMixChannels);
}

// ---------------------------------------------------------------------------
// NFSMixMapState::CreateMasterMixChannels @0x82B4CC48
//   Build the per-state master-mix channel procs from the serialised master-mix
//   section (m_pMMStateHdr blob + OffsetMasterMixData; a stMixChHdr + variable-stride
//   entries). Also grabs the shared master channel OUTPUT array and hands each unique
//   record a 64-byte-strided slice of it (rewinding when two adjacent channels target
//   the same SFXOBJID).
// ---------------------------------------------------------------------------
void NFSMixMapState::CreateMasterMixChannels()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    int liOffset = m_pMMStateHdr->OffsetMasterMixData;
    m_MasterChannelsAdded = 0;
    if (liOffset < 0)
        return;

    stMixChHdr* lpHdr = reinterpret_cast<stMixChHdr*>(
        reinterpret_cast<char*>(m_pMMStateHdr) + liOffset);
    m_pMixChHdr    = lpHdr;
    m_pChOutArrays = lpMap->GetMasterChannelOutputArrayPtr(lpHdr->NumUniqueSFXOBJs);
    m_MasterChannelsAdded = 0;
    if (lpHdr->NumMixChannels <= 0)
        return;

    m_MixStateParams.pMasterMixChProcs = lpMap->GetNextMasterMixProc(0);

    int* lpEntry        = reinterpret_cast<int*>(lpHdr + 1);
    int  liPrevSFXOBJID = 0;
    int  liOutOffset    = 0;   // byte offset into m_pChOutArrays (64-byte channel stride)
    int  liChannel      = 0;
    do
    {
        stMasterMixChSharedData* lpShared;
        stMasterMixChProc*       lpProc;
        stMasterMixChUniqueData* lpUnique;

        if (m_ObjectIndex)
        {
            lpShared = m_pFirstInstance->m_MixStateParams.pMasterMixChProcs[liChannel].pMixChData_S;
            lpProc   = lpMap->GetNextMasterMixProc(1);
            lpUnique = lpMap->GetNextMasterMixUnique(1);
        }
        else
        {
            lpShared = lpMap->GetNextMasterMixShared(1);
            lpProc   = lpMap->GetNextMasterMixProc(1);
            lpUnique = lpMap->GetNextMasterMixUnique(1);
            lpShared->pMapParams = reinterpret_cast<stMasterMixChParams*>(lpEntry);
            lpShared->pPRESETS   = 0;
            lpShared->MIXCHINID  = ((lpEntry[0] << 8) & 0xFF0000) | 0x20000000
                                 | (lpEntry[0] & 0x10000000) | liChannel;
            lpShared->NumInputs  = lpEntry[0] & 0xFF;
        }

        lpProc->pMixChData_S = lpShared;
        lpUnique->Output   = -10000;
        lpUnique->p3DData  = 0;
        lpUnique->pInputs  = 0;

        int liSFXOBJID = lpShared->pMapParams->SFXOBJID;
        lpUnique->outputID = liSFXOBJID | (m_ObjectIndex << 11);

        int* lpOut = reinterpret_cast<int*>(
            reinterpret_cast<char*>(m_pChOutArrays) + liOutOffset);
        if (liPrevSFXOBJID == liSFXOBJID)
            lpOut = reinterpret_cast<int*>(reinterpret_cast<char*>(lpOut) - 64);
        else
            liOutOffset += 64;
        lpUnique->pOutputs = lpOut;

        ++liChannel;
        lpProc->pMixChData_U = lpUnique;
        liPrevSFXOBJID = liSFXOBJID;
        ++m_MasterChannelsAdded;
        lpEntry += (lpEntry[0] & 0xFF) + 3;
    }
    while (liChannel < m_pMixChHdr->NumMixChannels);
}

// ---------------------------------------------------------------------------
// NFSMixMapState::InitializeSubChannels @0x82B4D2A8
//   For each sub-mix channel proc, walk its serialised input-descriptor list
//   (shared->pMapParams: dword[0] low byte = count; descriptors from dword[2]).
//   Count the (copy-expanded) inputs, allocate the input array, then emit the id
//   list. A descriptor whose state-id byte (bits 16..23) matches m_StateIndex maps
//   to one entry tagged with m_ObjectIndex; a foreign state-id expands into that
//   state's copy count (tag = copy index).
// ---------------------------------------------------------------------------
void NFSMixMapState::InitializeSubChannels()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    for (int liChannel = 0; liChannel < m_SubMixChannelsAdded; ++liChannel)
    {
        stSubMixChProc*    lpProc   = &m_MixStateParams.pSubMixChProcs[liChannel];
        stMixChSharedData* lpShared = lpProc->pMixChData_S;

        const int* lpParams = reinterpret_cast<const int*>(lpShared->pMapParams);
        int liCount = lpParams[0] & 0xFF;

        // Pass 1: total (expanded) input count.
        int liNumInputs = 0;
        for (int li = 0; li < liCount; ++li)
        {
            int liStateId = (lpParams[2 + li] >> 16) & 0xFF;
            if (liStateId == m_StateIndex)
                ++liNumInputs;
            else
                liNumInputs += lpMap->m_StateRefCount[liStateId];
        }

        int* lpInputs = lpMap->GetSubChannelInputPtr(liNumInputs);
        lpProc->pMixChData_U->pInputs = lpInputs;
        lpShared->NumInputs = liNumInputs;

        // Pass 2: emit the (expanded) input id list.
        const int* lpSrc = &lpParams[2];
        int* lpDst = lpInputs;
        for (int li = 0; li < liNumInputs; ++li)
        {
            int liDesc    = *lpSrc++;
            int liMasked  = liDesc & 0xFFFF07FF;
            int liStateId = (liDesc >> 16) & 0xFF;
            if (liStateId == m_StateIndex)
            {
                *lpDst++ = (m_ObjectIndex << 11) | liMasked;
            }
            else
            {
                --li;
                int liCopies = lpMap->m_StateRefCount[liStateId];
                if (liCopies > 0)
                {
                    li += liCopies;
                    for (int liCopy = 0; liCopy < liCopies; ++liCopy)
                        *lpDst++ = (liCopy << 11) | liMasked;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// NFSMixMapState::InitializeMasterChannels @0x82B4D420
//   As InitializeSubChannels, but the master-channel descriptor list (from
//   shared->pMapParams dword[3]) splits into two groups: SFXOBJ-flagged inputs
//   (top 3 bits == 0x80000000) and copy-expanded state inputs. The allocated array
//   holds [state inputs][master inputs]; pInputs points at the state block, p3DData
//   at the master block. shared->NumInputs packs (masterCount<<16)|stateCount, and
//   shared->pPRESETS points at the per-channel preset stream (blob + OffsetPresetData,
//   advanced by (preset[0]&0xFF)+1 dwords per channel).
// ---------------------------------------------------------------------------
void NFSMixMapState::InitializeMasterChannels()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    const int* lpPresets = reinterpret_cast<const int*>(
        reinterpret_cast<char*>(m_pMMStateHdr) + m_pMMStateHdr->OffsetPresetData);

    for (int liChannel = 0; liChannel < m_MasterChannelsAdded; ++liChannel)
    {
        stMasterMixChProc*       lpProc   = &m_MixStateParams.pMasterMixChProcs[liChannel];
        stMasterMixChSharedData* lpShared = lpProc->pMixChData_S;
        stMasterMixChUniqueData* lpUnique = lpProc->pMixChData_U;

        lpShared->pPRESETS = const_cast<int*>(lpPresets);

        const int* lpParams   = reinterpret_cast<const int*>(lpShared->pMapParams);
        int liCount     = lpParams[0]   & 0xFF;
        int liPresetCnt = lpPresets[0]  & 0xFF;

        // Pass 1: split into master (SFXOBJ-flagged) and (copy-expanded) state inputs.
        int liStateInputs  = 0;
        int liMasterInputs = 0;
        for (int li = 0; li < liCount; ++li)
        {
            int liDesc = lpParams[3 + li];
            if ((liDesc & 0xE0000000) == 0x80000000)
            {
                ++liMasterInputs;
            }
            else
            {
                int liStateId = (liDesc >> 16) & 0xFF;
                if (liStateId == m_StateIndex)
                    ++liStateInputs;
                else
                    liStateInputs += lpMap->m_StateRefCount[liStateId];
            }
        }

        int* lpInputs = lpMap->GetMasterChannelInputPtr(liMasterInputs + liStateInputs);
        lpUnique->pInputs = lpInputs;
        if (liMasterInputs > 0)
            lpUnique->p3DData = reinterpret_cast<st3DMixCtlProc**>(lpInputs + liStateInputs);
        else
            lpUnique->p3DData = 0;
        lpShared->NumInputs = (liMasterInputs << 16) | liStateInputs;

        // Pass 2 (shared descriptor cursor): master group first, then state group.
        const int* lpSrc = &lpParams[3];

        int* lpMaster = reinterpret_cast<int*>(lpUnique->p3DData);
        for (int li = 0; li < liMasterInputs; ++li)
        {
            int liMasked = *lpSrc++ & 0xFFFF07FF;
            *lpMaster++ = liMasked | (m_ObjectIndex << 11);
        }

        int* lpDst = lpUnique->pInputs;
        for (int li = 0; li < liStateInputs; ++li)
        {
            int liDesc    = *lpSrc++;
            int liMasked  = liDesc & 0xFFFF07FF;
            int liStateId = (liDesc >> 16) & 0xFF;
            if (liStateId == m_StateIndex)
            {
                *lpDst++ = (m_ObjectIndex << 11) | liMasked;
            }
            else
            {
                --li;
                int liCopies = lpMap->m_StateRefCount[liStateId];
                if (liCopies > 0)
                {
                    li += liCopies;
                    for (int liCopy = 0; liCopy < liCopies; ++liCopy)
                        *lpDst++ = (liCopy << 11) | liMasked;
                }
            }
        }

        lpPresets += liPresetCnt + 1;
    }
}
