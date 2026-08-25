#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp" // complete proc records for the accessors
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"     // owning map: Get* allocators + m_StateRefCount
#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"   // GetQ15FromHundredthsdB (dB->Q15, word_82F8677A antilog)

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
    // X360 null test is on the COMPUTED &base[copyIdx] (base null + idx 0 -> 0); on the
    // host that indexing is UB-on-null and the compiler deletes a post-index !st check --
    // so the guard is hoisted onto the base pointer itself (fixed 2026-08-25, wave 1).
    if (m_pFirstInstance == 0)
        return 0;
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (static_cast<int>(procIdx) >= st->m_MixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.pMixCtlProcs[procIdx];
}

st3DMixCtlProc* NFSMixMapState::Get3DMixCtlProc(unsigned char procIdx, int copyIdx)
{
    // X360 null test is on the COMPUTED &base[copyIdx] (base null + idx 0 -> 0); on the
    // host that indexing is UB-on-null and the compiler deletes a post-index !st check --
    // so the guard is hoisted onto the base pointer itself (fixed 2026-08-25, wave 1).
    if (m_pFirstInstance == 0)
        return 0;
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (static_cast<int>(procIdx) >= st->m_3DMixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.p3DMixCtlProc[procIdx];
}

stEvtMixCtlProc* NFSMixMapState::GetEvtMixCtlProc(unsigned char procIdx, int copyIdx)
{
    // X360 null test is on the COMPUTED &base[copyIdx] (base null + idx 0 -> 0); on the
    // host that indexing is UB-on-null and the compiler deletes a post-index !st check --
    // so the guard is hoisted onto the base pointer itself (fixed 2026-08-25, wave 1).
    if (m_pFirstInstance == 0)
        return 0;
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (static_cast<int>(procIdx) >= st->m_EvtMixCtlsAdded)
        return 0;
    return &st->m_MixStateParams.pEvtMixCtlProc[procIdx];
}

stSubMixChProc* NFSMixMapState::GetSubMixChProc(unsigned char procIdx, int copyIdx)
{
    // X360 null test is on the COMPUTED &base[copyIdx] (base null + idx 0 -> 0); on the
    // host that indexing is UB-on-null and the compiler deletes a post-index !st check --
    // so the guard is hoisted onto the base pointer itself (fixed 2026-08-25, wave 1).
    if (m_pFirstInstance == 0)
        return 0;
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (static_cast<int>(procIdx) >= st->m_SubMixChannelsAdded)
        return 0;
    return &st->m_MixStateParams.pSubMixChProcs[procIdx];
}

stMasterMixChProc* NFSMixMapState::GetMasterMixChProc(unsigned char procIdx, int copyIdx)
{
    // X360 null test is on the COMPUTED &base[copyIdx] (base null + idx 0 -> 0); on the
    // host that indexing is UB-on-null and the compiler deletes a post-index !st check --
    // so the guard is hoisted onto the base pointer itself (fixed 2026-08-25, wave 1).
    if (m_pFirstInstance == 0)
        return 0;
    NFSMixMapState* st = &m_pFirstInstance[copyIdx];
    if (static_cast<int>(procIdx) >= st->m_MasterChannelsAdded)
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
// NFSMixMapState::CreateMixCtls @0x82B4C890
//   Build the per-state mix-control procs from the serialised MixMap mix-control
//   section (m_pMMStateHdr blob + OffsetMixCtlData: a stMixCtlHdr followed by
//   variable-stride serialised mix-control entries). For each control it grabs a
//   curve-proc slot + a scale-id slot from the owning map, allocates a proc (whose
//   shared/unique records NFSMixMap::AssignMixCtlDataPtrs links in), then fills the
//   shared record's packed MIXCTLOBJID + params ptr + offset/ratio, and the unique
//   record's curve/scale pointers.
//   * The offset/ratio come from the entry's dB "swing" word (entry[1]): ratio =
//     0x7FFF - GetQ15FromHundredthsdB(dB). The X360 inlined that helper as a direct
//     word_82F8677A antilog-table read; it is de-inlined here to the (now-homed)
//     NFSMixShape::GetQ15FromHundredthsdB call.
//   * Entry stride = ((entry[1] >> 16) & 0x1F) + 2 dwords.
// ---------------------------------------------------------------------------
void NFSMixMapState::CreateMixCtls()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    int liOffset = m_pMMStateHdr->OffsetMixCtlData;
    m_MixCtlsAdded = 0;
    if (liOffset < 0)
        return;

    stMixCtlHdr* lpHdr = reinterpret_cast<stMixCtlHdr*>(
        reinterpret_cast<char*>(m_pMMStateHdr) + liOffset);
    m_pMixCtlHdr = lpHdr;
    if (lpHdr->NumMixCtls <= 0)
        return;

    m_MixStateParams.pMixCtlProcs = lpMap->GetProcessMixCtlPtr(0);

    // Serialised mix-control entries follow the 16-byte section header (external blob).
    int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);

    int liChannel = 0;
    do
    {
        // Curve-proc key: the input-id word with this state's object index folded in.
        // (The X360 builds this on the stack and hands its address to GetCurveDataPtr.)
        int laParam[2];
        laParam[0] = (lpEntry[0] & 0xFFFF07FF) | (m_ObjectIndex << 11);
        laParam[1] = lpEntry[1];

        stCurveDataProc* lpCurve = lpMap->GetCurveDataPtr(laParam);
        int*             lpScale = lpMap->AddScaleIDs(reinterpret_cast<unsigned short*>(lpEntry), m_ObjectIndex);
        stMixCtlProc*    lpProc  = lpMap->GetProcessMixCtlPtr(1);

        // AssignMixCtlDataPtrs links the freshly-allocated shared+unique records into lpProc.
        lpMap->AssignMixCtlDataPtrs(lpProc, lpEntry, m_ObjectIndex, liChannel);

        // FLAG (stub interaction guard, 2026-08-25 wave 1): AssignMixCtlDataPtrs is
        // currently an empty link-stub (NFSMixMapLinkStubs.cpp @0x82B4A1D8 pending),
        // so lpProc->psdata stays null and the stores below would null-deref the
        // moment CreateMixCtls first runs. Bail out until the real body lands (the
        // X360 has no such branch -- remove the guard with the stub; `break`, not
        // `continue`, because the entry-stride advance sits at the loop bottom).
        if (lpProc == 0 || lpProc->psdata == 0)
            break;

        stMixCtlSharedData* lpShared = lpProc->psdata;
        lpShared->MIXCTLOBJID    = (lpMap->m_MapType << 8)
                                 | ((lpEntry[0] >> 16) & 0xE000)
                                 | (lpEntry[0] & 0x0FFF0000)
                                 | liChannel;
        lpShared->pstMixCtlParms = reinterpret_cast<stMixCtlParams*>(lpEntry);
        lpShared->nOffset        = 0;
        lpShared->nRatio         = 0;

        // Bit 15 set => the swing word is a signed 16-bit dB (nOffset stays 0);
        // else the low 15 bits are a positive attenuation in hundredths-dB.
        if (lpEntry[1] & 0x8000)
        {
            lpShared->nRatio = 0x7FFF - NFSMixShape::GetQ15FromHundredthsdB(static_cast<short>(lpEntry[1]));
        }
        else
        {
            lpShared->nOffset = lpEntry[1] & 0x7FFF;
            if (lpShared->nOffset <= 0)
                lpShared->nRatio = 0;
            else
                lpShared->nRatio = 0x7FFF - NFSMixShape::GetQ15FromHundredthsdB(-lpShared->nOffset);
        }

        stMixCtlUniqueData* lpUnique = lpProc->pudata;
        lpUnique->CmpdBOut      = 0;
        lpUnique->pstCurveData  = lpCurve;
        lpUnique->ppScaleRatios = reinterpret_cast<int**>(lpScale);

        ++liChannel;
        ++m_MixCtlsAdded;
        lpEntry += ((laParam[1] >> 16) & 0x1F) + 2;
    }
    while (liChannel < m_pMixCtlHdr->NumMixCtls);
}

// ---------------------------------------------------------------------------
// NFSMixMapState::CreateEvtMixCtls @0x82B4CE00
//   Build this state's event mix-control procs from the serialised MixMap event
//   section (m_pMMStateHdr blob + OffsetEventCtlData: a stMixEventHdr followed by
//   variable-stride stMixEvtParams entries). The "master" state (m_ObjectIndex==0)
//   allocates fresh shared records; a copy reuses the first-instance's shared record.
//   For each event control it:
//     * defaults the per-envelope curve-id fields (nParam_00/_01/_02) to id 1 when
//       their low-12-bit id is unset. Which fields depend on the event-type nibble
//       ((nEVTCTLID >> 24) & 0xF, the top byte): types 0/2 default _00 + _02; type 1 also defaults _01;
//       type >= 3 defaults none;
//     * derives the shared offset/ratio from the swing word (nUScaleCntSwing) exactly
//       as CreateMixCtls (dB->Q15 via NFSMixShape::GetQ15FromHundredthsdB -- the X360
//       inlined word_82F8677A antilog-table read, de-inlined to the homed helper);
//     * zero-inits the unique envelope record, stamps its trigger id
//       ((m_ObjectIndex<<11)|nTriggerID) and its scale-ratios pointer via the
//       NFSMixMap::AddEvtScaleIDs helper (the event twin of AddScaleIDs).
//   Entry stride = (((nUScaleCntSwing >> 16) & 0xF) + 6) dwords.
// ---------------------------------------------------------------------------
void NFSMixMapState::CreateEvtMixCtls()
{
    NFSMixMap* lpMap = m_pNFSMixMap;

    int liOffset = m_pMMStateHdr->OffsetEventCtlData;   // +0x18
    m_EvtMixCtlsAdded = 0;
    if (liOffset < 0)
        return;

    stMixEventHdr* lpHdr = reinterpret_cast<stMixEventHdr*>(
        reinterpret_cast<char*>(m_pMMStateHdr) + liOffset);
    m_pEvtMixCtlHdr = lpHdr;
    if (lpHdr->NumEvents <= 0)
        return;

    m_MixStateParams.pEvtMixCtlProc = lpMap->GetNextEvtMixCtlProc(0);

    // Serialised event entries follow the 16-byte section header (external blob).
    stMixEvtParams* lpEntry = reinterpret_cast<stMixEvtParams*>(lpHdr + 1);

    int liChannel = 0;
    do
    {
        stEvtMixCtlSharedData* lpShared;
        if (m_ObjectIndex)
        {
            lpShared = m_pFirstInstance->m_MixStateParams.pEvtMixCtlProc[liChannel].pData_S;
        }
        else
        {
            lpShared = lpMap->GetNextEvtMixCtlShared(1);
            lpShared->pMapParms = lpEntry;
        }

        stEvtMixCtlProc*       lpProc   = lpMap->GetNextEvtMixCtlProc(1);
        stEvtMixCtlUniqueData* lpUnique = lpMap->GetNextEvtMixCtlUnique(1);
        lpProc->pData_S = lpShared;

        stMixEvtParams* lpParams = lpShared->pMapParms;

        // ---- envelope curve-id defaulting (per event-type nibble) ----
        // The type nibble is the TOP byte of nEVTCTLID (asm lbz r11,0(params) -> bits
        // [24..27]), matching committed GetCurveDataPtr's "type = bits[24..27]".
        int liType = (lpParams->nEVTCTLID >> 24) & 0xF;
        if (liType < 3)
        {
            if ((lpParams->nParam_00 & 0xFFF) == 0)
                lpParams->nParam_00 |= 1;
            if (liType == 1)
            {
                if ((lpParams->nParam_01 & 0xFFF) == 0)
                    lpParams->nParam_01 |= 1;
            }
            if ((lpParams->nParam_02 & 0xFFF) == 0)
                lpParams->nParam_02 |= 1;
        }

        // ---- shared offset/ratio from the swing word (same shape as CreateMixCtls) ----
        lpShared->nOffset = 0;
        lpShared->nRatio  = 0;
        if (lpParams->nUScaleCntSwing & 0x8000)
        {
            lpShared->nRatio = 0x7FFF - NFSMixShape::GetQ15FromHundredthsdB(
                static_cast<short>(lpParams->nUScaleCntSwing));
        }
        else
        {
            lpShared->nOffset = lpParams->nUScaleCntSwing & 0x7FFF;
            if (lpShared->nOffset <= 0)
                lpShared->nRatio = 0;
            else
                lpShared->nRatio = 0x7FFF - NFSMixShape::GetQ15FromHundredthsdB(-lpShared->nOffset);
        }

        // ---- zero-init the unique envelope record + stamp trigger / scale ids ----
        lpProc->pData_U          = lpUnique;
        lpUnique->msStageElapsed = 0.0f;
        lpUnique->msStart        = 0.0f;
        lpUnique->qStart         = 0;
        lpUnique->eCurrentStage  = eEnvelopeStage_Off;
        lpUnique->qoutput        = 0;
        lpUnique->output         = 0;
        lpUnique->pTriggerPtr    = reinterpret_cast<int*>(
            static_cast<intptr_t>((m_ObjectIndex << 11) | lpParams->nTriggerID));
        lpUnique->ppScaleRatios  = reinterpret_cast<int**>(
            lpMap->AddEvtScaleIDs(lpParams, m_ObjectIndex));

        ++liChannel;
        ++m_EvtMixCtlsAdded;
        // variable stride: 6-dword stMixEvtParams header + N u-scale ids, where N is the
        // HIGH 16 bits' low nibble of nUScaleCntSwing (asm lhz r11,4(params) + clrlwi ,28
        // -> (swing >> 16) & 0xF); the LOW 16 bits carry the dB (& 0x8000 / (short) / & 0x7FFF).
        lpEntry = reinterpret_cast<stMixEvtParams*>(
            reinterpret_cast<int*>(lpEntry) + (((lpParams->nUScaleCntSwing >> 16) & 0xF) + 6));
    }
    while (liChannel < m_pEvtMixCtlHdr->NumEvents);
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
