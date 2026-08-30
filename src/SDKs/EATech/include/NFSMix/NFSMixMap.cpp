#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp" // stMixCtlProc / stMixMapHeader for the helpers
#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp" // GetMasterMixChProc / placement-construct
#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"    // per-frame curve / dB / Q15 conversions
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp" // g_pMixerAllocator (off_83250004)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include <new>                                            // placement new (NFSMixMapState object memory)
#include <algorithm>
#include <cstring>
#include <vector>

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
// sub / master channel passes. NFSMixShape is homed, including the ARTIST delta-time
// normalization constant, so the complete per-frame DSP path is wired.
// ---------------------------------------------------------------------------
void NFSMixMap::ProcessMixMap(float lfDeltaTime, int liCamState)
{
    m_fDeltaTimeRatio[1] = m_fDeltaTimeRatio[0];   // +0x230 = +0x22C (shift previous)
    m_PrevCamState       = m_CurCamState;          // +0x7C  = old +0x80
    // ARTIST divides by flt_82F87958 (0.0333666988f), not by one.  This is the
    // normalized frame ratio consumed by the time-dependent mixer controls.
    m_fDeltaTimeRatio[0] = lfDeltaTime / 0.033366698771715164f;
    m_CurCamState        = liCamState;             // +0x80
    m_fDeltaTime         = lfDeltaTime;            // +0x84
    m_msDeltaTime        = lfDeltaTime * 1000.0f;  // +0x88  (flt_82009E10 == 1000.0, ms/sec)

    // (1) curve pass: each curve-proc's Q15 output = curve(nINPUTID&0xF) at its live input.
    for (int li = 0; li < m_CurveProcsAdded; ++li)  // +0xD0
    {
        stCurveDataProc& lrProc = m_pCurveDataArray[li];               // +0x184, stride 16
        // ARTIST uses `lbz 0(nINPUTID); clrlwi ...,28`: on the big-endian
        // console that is the low nibble of the serialized word's HIGH byte.
        // The PC asset porter preserves the dword's numeric value, so the
        // equivalent host expression is bits [24..27], not `nINPUTID & 0xF`.
        const int liCurveType = (static_cast<u32>(lrProc.nINPUTID) >> 24) & 0xF;
        lrProc.Q15Output = NFSMixShape::GetCurveOutput(
            liCurveType, *lrProc.pInputParam, /*lbDb=*/0);             // +0xC
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

    Update3DMixCtls();      // @0x82B4BB98
    UpdateEvtMixCtls();     // @0x82B4C2A8
    UpdateSubChannels();    // @0x82B4AC10
    UpdateMasterChannels(); // @0x82B4ACD8
}

namespace
{
    // ARTIST rodata used by the three envelope processors.  Authored envelope
    // durations are stored in 1/60-second ticks and evaluated in milliseconds.
    const float KF_EVT_TICK_MS = 16.666669845581055f; // flt_821483E8
    const float KF_EVT_SHORT_MS = 16.666000366210938f; // flt_821483E4

    static float GetEnvelopeProgress(float afElapsed, float afStart, float afEnd)
    {
        float lfProgress = afElapsed - afStart;
        const float lfSpan = afEnd - afStart;
        if (lfSpan > 0.0f)
            lfProgress /= lfSpan;
        return lfProgress;
    }

    static int InterpolateEnvelope(int aiStart, int aiEnd, float afCurve)
    {
        return aiStart + static_cast<int>(afCurve * static_cast<float>(aiEnd - aiStart));
    }

    static void ResetEnvelope(stEvtMixCtlUniqueData& arData)
    {
        arData.msStageElapsed = 0.0f;
        arData.msStart = 0.0f;
        arData.qStart = 0;
        arData.eCurrentStage = eEnvelopeStage_Off;
        arData.output = 0;
        arData.qoutput = 0;
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::Update3DMixCtls @0x82B4BB98 -- select the current camera-state
// records, calculate adjacent-quadrant distance rolloff, and smooth Doppler.
// The external 3D input block is the console's sixteen-int DMix record: position
// inputs at 0/1, azimuth inputs at 2/3, velocity deltas at 13/14, flags at 15.
// ---------------------------------------------------------------------------
void NFSMixMap::Update3DMixCtls()
{
    if (m_CurCamState != m_PrevCamState)
    {
        for (int li = 0; li < m_nAssigned3DMixCtlShared; ++li)
        {
            st3DMixCtlSharedData& lrShared = m_p3DMixCtlData_S[li];
            st3DStateParams* const lpStates = &lrShared.pMapParams->StateParams;
            const int liStateCount =
                (static_cast<u32>(lrShared.pMapParams->nINPUTID) >> 24) & 0xF;
            int liWantedState = m_CurCamState;

            for (;;)
            {
                bool lbFound = false;
                for (int liState = 0; liState < liStateCount; ++liState)
                {
                    if (((static_cast<u32>(lpStates[liState].n3DSTATEINFOID) >> 24) & 0xF)
                        != liWantedState)
                        continue;

                    lrShared.pCurStateParams = &lpStates[liState];
                    lrShared.PrevCamState = m_PrevCamState;
                    lrShared.CurCamState = m_CurCamState;
                    lrShared.msSinceCamTrans = 0;
                    lbFound = true;
                    break;
                }
                if (lbFound)
                    break;
                liWantedState = 0;
            }
        }
    }

    for (int li = 0; li < m_3DMixCtlsAdded; ++li)
    {
        st3DMixCtlProc& lrProc = m_p3DMixCtlProc[li];
        st3DMixCtlUniqueData& lrUnique = *lrProc.p3DMixCtlData_U;
        int* const lpInputs = lrUnique.pInputs;

        if ((lpInputs[15] & 1) == 0)
        {
            lrUnique.dBRolloff = -10000;
            lrUnique.q15Rolloff = 0;
            lrUnique.azimuth = 0;
            lrUnique.DopplerCents = 0;
            continue;
        }

        st3DStateParams& lrState = *lrProc.p3DMixCtlData_S->pCurStateParams;
        const u32 luStateInfo = static_cast<u32>(lrState.n3DSTATEINFOID);
        const u32 luCurveDoppler = static_cast<u32>(lrState.nCURVEID_DOPPLER);
        const int liDistanceInput = (luStateInfo >> 12) & 0xF;
        const int liAzimuthInput = (luStateInfo >> 8) & 0xF;

        float lfDistance;
        if (liDistanceInput == 0)
            lfDistance = static_cast<float>(lpInputs[1]) * 0.009999999776482582f;
        else if (liDistanceInput == 1)
            lfDistance = static_cast<float>(lpInputs[0]) * 0.009999999776482582f;
        else
            lfDistance = -1.0f;
        float lfAdjacentDistance = lfDistance;

        int liAzimuth;
        if (liAzimuthInput == 0)
            liAzimuth = lpInputs[3];
        else if (liAzimuthInput == 1)
            liAzimuth = lpInputs[2];
        else
            liAzimuth = 0;
        lrUnique.azimuth = liAzimuth;

        int liBaseAzimuth;
        int liCurve0;
        int liCurve1;
        u32 luRange0;
        u32 luRange1;
        switch ((static_cast<u32>(liAzimuth) >> 14) & 3)
        {
            case 0:
                liBaseAzimuth = liAzimuth;
                luRange0 = static_cast<u32>(lrState.nQ0MinMax);
                luRange1 = static_cast<u32>(lrState.nQ1MinMax);
                liCurve0 = (luCurveDoppler >> 28) & 0xF;
                liCurve1 = (luCurveDoppler >> 16) & 0xF;
                break;
            case 1:
                liBaseAzimuth = liAzimuth - 0x4000;
                luRange0 = static_cast<u32>(lrState.nQ1MinMax);
                luRange1 = static_cast<u32>(lrState.nQ2MinMax);
                liCurve0 = (luCurveDoppler >> 16) & 0xF;
                liCurve1 = (luCurveDoppler >> 24) & 0xF;
                break;
            case 2:
                liBaseAzimuth = liAzimuth - 0x8000;
                luRange0 = static_cast<u32>(lrState.nQ2MinMax);
                luRange1 = static_cast<u32>(lrState.nQ3MinMax);
                liCurve0 = (luCurveDoppler >> 24) & 0xF;
                liCurve1 = (luCurveDoppler >> 20) & 0xF;
                break;
            default:
                liBaseAzimuth = liAzimuth - 0xC000;
                luRange0 = static_cast<u32>(lrState.nQ3MinMax);
                luRange1 = static_cast<u32>(lrState.nQ0MinMax);
                liCurve0 = (luCurveDoppler >> 20) & 0xF;
                liCurve1 = (luCurveDoppler >> 28) & 0xF;
                break;
        }

        const float lfMin0 = static_cast<float>(luRange0 & 0x7FFF);
        const float lfMax0 = static_cast<float>((luRange0 >> 16) & 0x7FFF);
        const float lfMin1 = static_cast<float>(luRange1 & 0x7FFF);
        const float lfMax1 = static_cast<float>((luRange1 >> 16) & 0x7FFF);
        if (lfDistance > lfMax0 && lfDistance > lfMax1)
        {
            lrUnique.dBRolloff = -10000;
            lrUnique.q15Rolloff = 0;
            lrUnique.DopplerCents = 0;
            continue;
        }

        if (lfDistance < lfMin0) lfDistance = lfMin0;
        if (lfAdjacentDistance < lfMin1) lfAdjacentDistance = lfMin1;
        if (lfDistance > lfMax0) lfDistance = lfMax0;
        if (lfAdjacentDistance > lfMax1) lfAdjacentDistance = lfMax1;

        int laiPositions[2];
        laiPositions[0] = static_cast<int>(
            ((lfDistance - lfMin0) / (lfMax0 - lfMin0)) * 32767.0f);
        laiPositions[1] = static_cast<int>(
            ((lfAdjacentDistance - lfMin1) / (lfMax1 - lfMin1)) * 32767.0f);
        lrUnique.q15Rolloff = NFSMixShape::GetAzimShapeOutput(
            liCurve0, liCurve1, laiPositions, liBaseAzimuth);
        lrUnique.dBRolloff = NFSMixShape::GetdBFromQ15(lrUnique.q15Rolloff);

        const int liDopplerBase = static_cast<u16>(luCurveDoppler);
        if (liDopplerBase == 0)
            continue;

        const int liPositionIndex = liDistanceInput == 1 ? 0 : 1;
        const int liDeltaIndex = liDistanceInput == 1 ? 13 : 14;
        const u32 luResetFlag = liDistanceInput == 1 ? 0x80000000u : 0x40000000u;
        const float lfCurrentDistance =
            static_cast<float>(lpInputs[liPositionIndex]) * 0.009999999776482582f;

        int liTargetCents = 0;
        if ((static_cast<u32>(lpInputs[15]) & luResetFlag) == 0)
        {
            const float lfBase = static_cast<float>(liDopplerBase);
            float lfDenominator =
                static_cast<float>(lpInputs[liDeltaIndex]) * 0.009999999776482582f + lfBase;
            if (lfDenominator <= 0.0f)
                lfDenominator = lfBase;
            liTargetCents = NFSMixShape::GetCentsFromPitchMult(lfBase / lfDenominator);
        }
        else
        {
            lpInputs[15] = static_cast<int>(static_cast<u32>(lpInputs[15]) & ~luResetFlag);
        }

        if (lrUnique.fPrevDeltaDist == 0.0f)
            lrUnique.fPrevDeltaDist = 1.0f;
        lrUnique.fPrevDeltaDist = lfCurrentDistance > lrUnique.fPrevDist
                                ? lfCurrentDistance - lrUnique.fPrevDist
                                : lrUnique.fPrevDist - lfCurrentDistance;
        lrUnique.fPrevDist = lfCurrentDistance;
        lrUnique.DopplerCents += static_cast<int>(
            static_cast<float>(liTargetCents - lrUnique.DopplerCents)
            * 0.20000000298023224f);
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::UpdateAREvent @0x82B4B948 -- attack/release envelope.
// The packed times and curve ids come from nParam_00 (attack) and nParam_02
// (release).  Bits 8/10 of nEVTCTLID select gated/retrigger behavior.  The
// stage-remapping stores preserve continuity when a live release retriggers.
// ---------------------------------------------------------------------------
void NFSMixMap::UpdateAREvent(stEvtMixCtlProc* lpProc)
{
    stMixEvtParams& lrParams = *lpProc->pData_S->pMapParms;
    stEvtMixCtlUniqueData& lrData = *lpProc->pData_U;
    const bool lbTriggered = *lrData.pTriggerPtr != 0;
    const bool lbRetrigger = (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 10)) != 0;
    const float lfAttackMs = static_cast<float>(static_cast<u32>(lrParams.nParam_00) & 0xFFFu)
                           * KF_EVT_TICK_MS;
    const float lfReleaseMs = static_cast<float>(static_cast<u32>(lrParams.nParam_02) & 0xFFFu)
                            * KF_EVT_TICK_MS;
    const int liAttackCurve = (static_cast<u32>(lrParams.nParam_00) >> 12) & 0xF;
    const int liReleaseCurve = (static_cast<u32>(lrParams.nParam_02) >> 12) & 0xF;

    for (;;)
    {
        if (lrData.eCurrentStage == eEnvelopeStage_Off)
            lrData.eCurrentStage = eEnvelopeStage_Attack;

        if (lrData.eCurrentStage == eEnvelopeStage_Attack)
        {
            if (lrData.msStageElapsed >= lfAttackMs)
            {
                lrData.msStageElapsed = 0.0f;
                lrData.eCurrentStage = eEnvelopeStage_Release;
                lrData.msStart = 0.0f;
                lrData.qStart = 0x7FFF;
                continue;
            }

            if (lfAttackMs == 0.0f)
            {
                lrData.qoutput = 0x7FFF;
                return;
            }

            const float lfProgress = GetEnvelopeProgress(
                lrData.msStageElapsed, lrData.msStart, lfAttackMs);
            lrData.qoutput = InterpolateEnvelope(
                lrData.qStart, 0x7FFF,
                NFSMixShape::GetFloatCurveOutput(liAttackCurve, lfProgress));
            return;
        }

        if (lrData.msStageElapsed >= lfReleaseMs)
        {
            ResetEnvelope(lrData);
            return;
        }

        if (lbTriggered && lbRetrigger)
        {
            lrData.eCurrentStage = eEnvelopeStage_Attack;
            lrData.qStart = lrData.qoutput;
            float lfMapped;
            if (lfReleaseMs < KF_EVT_SHORT_MS)
                lfMapped = lfAttackMs;
            else
                lfMapped = ((lfReleaseMs - lrData.msStageElapsed) / lfReleaseMs) * lfAttackMs;
            lrData.msStart = lfMapped;
            lrData.msStageElapsed = lfMapped;
            continue;
        }

        const float lfProgress = GetEnvelopeProgress(
            lrData.msStageElapsed, lrData.msStart, lfReleaseMs);
        const float lfCurve = NFSMixShape::GetFloatCurveOutput(
            liReleaseCurve, 1.0f - lfProgress);
        lrData.qoutput = lrData.qStart
                       - static_cast<int>(lfCurve * static_cast<float>(lrData.qStart));
        return;
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::UpdateASREvent @0x82B4B5F0 -- attack/sustain/release envelope.
// nParam_00/_01/_02 contain attack/sustain/release respectively.  In gated
// mode the sustain stage follows the trigger; otherwise nParam_01 is its timer.
// ---------------------------------------------------------------------------
void NFSMixMap::UpdateASREvent(stEvtMixCtlProc* lpProc)
{
    stMixEvtParams& lrParams = *lpProc->pData_S->pMapParms;
    stEvtMixCtlUniqueData& lrData = *lpProc->pData_U;
    const bool lbTriggered = *lrData.pTriggerPtr != 0;
    const bool lbGated = (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 8)) != 0;
    const bool lbRetrigger = (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 10)) != 0;
    const float lfAttackMs = static_cast<float>(static_cast<u32>(lrParams.nParam_00) & 0xFFFu)
                           * KF_EVT_TICK_MS;
    const float lfSustainMs = static_cast<float>(static_cast<u32>(lrParams.nParam_01) & 0xFFFu)
                            * KF_EVT_TICK_MS;
    const float lfReleaseMs = static_cast<float>(static_cast<u32>(lrParams.nParam_02) & 0xFFFu)
                            * KF_EVT_TICK_MS;
    const int liAttackCurve = (static_cast<u32>(lrParams.nParam_00) >> 12) & 0xF;
    const int liReleaseCurve = (static_cast<u32>(lrParams.nParam_02) >> 12) & 0xF;

    for (;;)
    {
        if (lrData.eCurrentStage == eEnvelopeStage_Off)
            lrData.eCurrentStage = eEnvelopeStage_Attack;

        if (lrData.eCurrentStage == eEnvelopeStage_Attack)
        {
            if (lbGated && !lbTriggered)
            {
                lrData.eCurrentStage = eEnvelopeStage_Release;
                lrData.qStart = lrData.qoutput;
                float lfMapped;
                if (lfAttackMs < KF_EVT_SHORT_MS)
                    lfMapped = lfReleaseMs;
                else
                    lfMapped = ((lfAttackMs - lrData.msStageElapsed) / lfAttackMs) * lfReleaseMs;
                lrData.msStart = lfMapped;
                lrData.msStageElapsed = lfMapped;
                continue;
            }

            if (lrData.msStageElapsed >= lfAttackMs)
            {
                lrData.msStageElapsed = 0.0f;
                lrData.eCurrentStage = eEnvelopeStage_Sustain;
                lrData.msStart = 0.0f;
                lrData.qStart = 0x7FFF;
                continue;
            }

            if (lfAttackMs == 0.0f)
            {
                lrData.qoutput = 0x7FFF;
                return;
            }

            const float lfProgress = GetEnvelopeProgress(
                lrData.msStageElapsed, lrData.msStart, lfAttackMs);
            lrData.qoutput = InterpolateEnvelope(
                lrData.qStart, 0x7FFF,
                NFSMixShape::GetFloatCurveOutput(liAttackCurve, lfProgress));
            return;
        }

        if (lrData.eCurrentStage == eEnvelopeStage_Sustain)
        {
            const bool lbHold = (!lbGated || lbTriggered)
                             && (lbGated || lrData.msStageElapsed <= lfSustainMs);
            if (lbHold)
            {
                lrData.qoutput = 0x7FFF;
                if (lbGated)
                    lrData.msStageElapsed = 0.0f;
                return;
            }

            lrData.msStageElapsed = 0.0f;
            lrData.eCurrentStage = eEnvelopeStage_Release;
            lrData.msStart = 0.0f;
            lrData.qStart = 0x7FFF;
            continue;
        }

        if (lrData.msStageElapsed >= lfReleaseMs)
        {
            ResetEnvelope(lrData);
            return;
        }

        if (lbTriggered && lbRetrigger)
        {
            lrData.eCurrentStage = eEnvelopeStage_Attack;
            lrData.qStart = lrData.qoutput;
            float lfMapped;
            if (lfReleaseMs < KF_EVT_SHORT_MS)
                lfMapped = lfAttackMs;
            else
                lfMapped = ((lfReleaseMs - lrData.msStageElapsed) / lfReleaseMs) * lfAttackMs;
            lrData.msStart = lfMapped;
            lrData.msStageElapsed = lfMapped;
            continue;
        }

        const float lfProgress = GetEnvelopeProgress(
            lrData.msStageElapsed, lrData.msStart, lfReleaseMs);
        const float lfCurve = NFSMixShape::GetFloatCurveOutput(
            liReleaseCurve, 1.0f - lfProgress);
        lrData.qoutput = lrData.qStart
                       - static_cast<int>((1.0f - lfCurve)
                           * static_cast<float>(lrData.qStart));
        return;
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::UpdateADSREvent @0x82B4B168 -- attack/decay/sustain/release.
// The compact record stores attack in nParam_00, sustain duration + decay
// duration/curve in nParam_01, and release duration/curve + sustain Q15 in
// nParam_02.  The switch/continue form mirrors ARTIST's stage machine.
// ---------------------------------------------------------------------------
void NFSMixMap::UpdateADSREvent(stEvtMixCtlProc* lpProc)
{
    stMixEvtParams& lrParams = *lpProc->pData_S->pMapParms;
    stEvtMixCtlUniqueData& lrData = *lpProc->pData_U;
    const bool lbTriggered = *lrData.pTriggerPtr != 0;
    const bool lbGated = (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 8)) != 0;
    const bool lbRetrigger = (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 10)) != 0;
    const float lfAttackMs = static_cast<float>(static_cast<u32>(lrParams.nParam_00) & 0xFFFu)
                           * KF_EVT_TICK_MS;
    const float lfSustainMs = static_cast<float>(static_cast<u32>(lrParams.nParam_01) & 0xFFFu)
                            * KF_EVT_TICK_MS;
    const float lfDecayMs = static_cast<float>((static_cast<u32>(lrParams.nParam_01) >> 16) & 0xFFFu)
                          * KF_EVT_TICK_MS;
    const float lfReleaseMs = static_cast<float>(static_cast<u32>(lrParams.nParam_02) & 0xFFFu)
                            * KF_EVT_TICK_MS;
    const int liAttackCurve = (static_cast<u32>(lrParams.nParam_00) >> 12) & 0xF;
    const int liDecayCurve = (static_cast<u32>(lrParams.nParam_01) >> 12) & 0xF;
    const int liReleaseCurve = (static_cast<u32>(lrParams.nParam_02) >> 12) & 0xF;
    const int liSustainQ15 = lrParams.nParam_02 >> 16;

    for (;;)
    {
        switch (lrData.eCurrentStage)
        {
            case eEnvelopeStage_Off:
                lrData.eCurrentStage = eEnvelopeStage_Attack;
                // fall through

            case eEnvelopeStage_Attack:
                if (lbGated && !lbTriggered)
                {
                    lrData.eCurrentStage = eEnvelopeStage_Release;
                    lrData.qStart = lrData.qoutput;
                    float lfMapped;
                    if (lfAttackMs < KF_EVT_SHORT_MS)
                        lfMapped = lfReleaseMs;
                    else
                        lfMapped = ((lfAttackMs - lrData.msStageElapsed) / lfAttackMs) * lfReleaseMs;
                    lrData.msStart = lfMapped;
                    lrData.msStageElapsed = lfMapped;
                    continue;
                }
                if (lrData.msStageElapsed < lfAttackMs)
                {
                    if (lfAttackMs > KF_EVT_SHORT_MS)
                    {
                        const float lfProgress = GetEnvelopeProgress(
                            lrData.msStageElapsed, lrData.msStart, lfAttackMs);
                        lrData.qoutput = InterpolateEnvelope(
                            lrData.qStart, 0x7FFF,
                            NFSMixShape::GetFloatCurveOutput(liAttackCurve, lfProgress));
                    }
                    else
                    {
                        lrData.qoutput = 0x7FFF;
                    }
                    return;
                }
                lrData.msStageElapsed = 0.0f;
                lrData.eCurrentStage = eEnvelopeStage_Decay;
                lrData.msStart = 0.0f;
                lrData.qStart = 0x7FFF;
                continue;

            case eEnvelopeStage_Decay:
                if (lbGated && !lbTriggered)
                {
                    lrData.eCurrentStage = eEnvelopeStage_Release;
                    lrData.qStart = lrData.qoutput;
                    const float lfMapped = ((lfDecayMs - lrData.msStageElapsed) / lfDecayMs)
                                         * lfReleaseMs;
                    lrData.msStart = lfMapped;
                    lrData.msStageElapsed = lfMapped;
                    continue;
                }
                if (lrData.msStageElapsed <= lfDecayMs)
                {
                    const float lfProgress = GetEnvelopeProgress(
                        lrData.msStageElapsed, lrData.msStart, lfDecayMs);
                    const float lfCurve = NFSMixShape::GetFloatCurveOutput(
                        liDecayCurve, 1.0f - lfProgress);
                    lrData.qoutput = InterpolateEnvelope(
                        lrData.qStart, liSustainQ15, 1.0f - lfCurve);
                    return;
                }
                lrData.msStageElapsed = 0.0f;
                lrData.eCurrentStage = eEnvelopeStage_Sustain;
                lrData.msStart = 0.0f;
                lrData.qStart = liSustainQ15;
                continue;

            case eEnvelopeStage_Sustain:
            {
                const bool lbHold = (!lbGated || lbTriggered)
                                 && (lbGated || lrData.msStageElapsed <= lfSustainMs);
                if (lbHold)
                {
                    lrData.qoutput = liSustainQ15;
                    if (lbGated)
                        lrData.msStageElapsed = 0.0f;
                    return;
                }
                lrData.msStageElapsed = 0.0f;
                lrData.eCurrentStage = eEnvelopeStage_Release;
                lrData.msStart = 0.0f;
                lrData.qStart = liSustainQ15;
                continue;
            }

            default:
                if (lrData.msStageElapsed >= lfReleaseMs)
                {
                    ResetEnvelope(lrData);
                    return;
                }
                if (lbTriggered && lbRetrigger)
                {
                    lrData.eCurrentStage = eEnvelopeStage_Attack;
                    lrData.qStart = lrData.qoutput;
                    float lfMapped;
                    if (lfReleaseMs < KF_EVT_SHORT_MS)
                        lfMapped = lfAttackMs;
                    else
                        lfMapped = ((lfReleaseMs - lrData.msStageElapsed) / lfReleaseMs)
                                 * lfAttackMs;
                    lrData.msStart = lfMapped;
                    lrData.msStageElapsed = lfMapped;
                    continue;
                }
                const float lfProgress = GetEnvelopeProgress(
                    lrData.msStageElapsed, lrData.msStart, lfReleaseMs);
                const float lfCurve = NFSMixShape::GetFloatCurveOutput(
                    liReleaseCurve, 1.0f - lfProgress);
                lrData.qoutput = lrData.qStart
                               - static_cast<int>((1.0f - lfCurve)
                                   * static_cast<float>(lrData.qStart));
                return;
        }
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::UpdateEvtMixCtls @0x82B4C2A8 -- advance every event envelope,
// convert its Q15 envelope through the authored swing, then compound the
// optional scale inputs.  ARTIST reads both the event type and scale count with
// byte loads from big-endian words; the host equivalents are bits [24..27] and
// [24..31] respectively.
// ---------------------------------------------------------------------------
void NFSMixMap::UpdateEvtMixCtls()
{
    for (int li = 0; li < m_EventCtlsAdded; ++li)
    {
        stEvtMixCtlProc& lrProc = m_pEvtMixCtlProc[li];
        stEvtMixCtlSharedData& lrShared = *lrProc.pData_S;
        stEvtMixCtlUniqueData& lrUnique = *lrProc.pData_U;
        stMixEvtParams& lrParams = *lrShared.pMapParms;
        const bool lbDbEnvelope =
            (static_cast<u32>(lrParams.nEVTCTLID) & (1u << 9)) != 0;

        if (lrUnique.eCurrentStage == eEnvelopeStage_Off &&
            *lrUnique.pTriggerPtr == 0)
        {
            ResetEnvelope(lrUnique);
            if (lbDbEnvelope)
                lrUnique.output = -10000;
            continue;
        }

        lrUnique.msStageElapsed += m_msDeltaTime;
        switch ((static_cast<u32>(lrParams.nEVTCTLID) >> 24) & 0xF)
        {
            case 0: UpdateAREvent(&lrProc); break;
            case 1: UpdateASREvent(&lrProc); break;
            case 3: UpdateADSREvent(&lrProc); break;
            default: break;
        }

        const int liSwing = static_cast<s16>(lrParams.nUScaleCntSwing);
        if (lbDbEnvelope)
        {
            lrUnique.output = NFSMixShape::GetdBFromQ15(lrUnique.qoutput);
        }
        else if (liSwing > 0)
        {
            const int liQ15 = ((lrUnique.qoutput * lrShared.nRatio) >> 15)
                             - lrShared.nRatio + 0x7FFF;
            lrUnique.output = lrShared.nOffset + NFSMixShape::GetdBFromQ15(liQ15);
        }
        else
        {
            const int liQ15 = 0x7FFF
                            - ((lrUnique.qoutput * lrShared.nRatio) >> 15);
            lrUnique.output = NFSMixShape::GetdBFromQ15(liQ15);
        }

        if (lrUnique.ppScaleRatios)
        {
            int liScale = 0x7FFF;
            int liCount = (static_cast<u32>(lrParams.nUScaleCntSwing) >> 24) & 0xFF;
            int** lppScale = lrUnique.ppScaleRatios;
            while (liCount-- > 0)
                liScale = (**lppScale++ * liScale) >> 15;

            if (lbDbEnvelope)
            {
                const int liQ15 = NFSMixShape::GetQ15FromHundredthsdB(lrUnique.output);
                lrUnique.output = NFSMixShape::GetdBFromQ15((liQ15 * liScale) >> 15);
            }
            else
            {
                lrUnique.output = (lrUnique.output * liScale) >> 15;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// NFSMixMap::PreProcessMixMap @0x82B48498 -- ~1.5KB blob-walk that
// reads the MixMap section headers (state/ctl/3D/event/channel) and accumulates the
// m_n* counts AllocateMixerMemory then sizes its blocks from. Bodied in the allocation
// pass (it pairs with AllocateMixerMemory + the mixer allocator).
// ---------------------------------------------------------------------------
void NFSMixMap::PreProcessMixMap()
{
    ResetMapData();
    SetupStateRefCount();
    m_nStateMapCount = 0;

    char* const lpBlob = reinterpret_cast<char*>(m_pMMHdr);
    const int* const lpStateOffsets = reinterpret_cast<const int*>(
        lpBlob + m_pMMHdr->StateTableOffset);

    for (int liState = 0; liState < m_pMMHdr->NumStates; ++liState)
    {
        if (lpStateOffsets[liState] < 0)
            continue;

        stMixMapStateHdr* const lpState = reinterpret_cast<stMixMapStateHdr*>(
            lpBlob + lpStateOffsets[liState]);
        const int liCopies = m_StateRefCount[liState];
        m_nStateMapCount += liCopies;

        if (lpState->OffsetMixCtlData >= 0)
        {
            stMixCtlHdr* const lpHdr = reinterpret_cast<stMixCtlHdr*>(
                reinterpret_cast<char*>(lpState) + lpState->OffsetMixCtlData);
            m_MixCtlsAdded += lpHdr->NumMixCtls * liCopies;
            m_SharedMixCtlCount += lpHdr->NumMixCtls;
            m_DataProcsAdded += lpHdr->NumNewMixDataProcs * liCopies;

            std::vector<int> laSeenInputs;
            int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);
            for (int liCtl = 0; liCtl < lpHdr->NumMixCtls; ++liCtl)
            {
                if (std::find(laSeenInputs.begin(), laSeenInputs.end(), lpEntry[0]) ==
                    laSeenInputs.end())
                {
                    laSeenInputs.push_back(lpEntry[0]);
                    const int liCurveType = (lpEntry[0] >> 24) & 0xF;
                    if (liCurveType < 10)
                        m_CurveProcsTotal[liCurveType][0] += liCopies;
                }

                const int liScaleCount = (lpEntry[1] >> 16) & 0xF;
                const int* lpScale = lpEntry + 2;
                int liExpanded = 0;
                for (int li = 0; li < liScaleCount; ++li)
                {
                    const int liRefState = (lpScale[li] >> 16) & 0xFF;
                    liExpanded += (liRefState == liState) ? 1 : m_StateRefCount[liRefState];
                }
                m_ScaleParamsAdded += liCopies * liExpanded;
                lpEntry += liScaleCount + 2;
            }
        }

        if (lpState->Offset3DMixCtlData >= 0)
        {
            st3DMixCtlHdr* const lpHdr = reinterpret_cast<st3DMixCtlHdr*>(
                reinterpret_cast<char*>(lpState) + lpState->Offset3DMixCtlData);
            m_Shared3DMixCtlCount += lpHdr->Num3DMixCtls;
            m_3DMixCtlsAdded += lpHdr->Num3DMixCtls * liCopies;
            const u8* lpEntry = reinterpret_cast<const u8*>(lpHdr + 1);
            for (int liCtl = 0; liCtl < lpHdr->Num3DMixCtls; ++liCtl)
            {
                // ARTIST reads the first serialized byte (`lbz 0(entry)`) and
                // keeps its low nibble. The PC dword equivalent is bits 24..27.
                const int liCameraStates =
                    (*reinterpret_cast<const u32*>(lpEntry) >> 24) & 0xF;
                m_n3DCamStatesAdded += liCameraStates;
                lpEntry += 4 + liCameraStates * static_cast<int>(sizeof(st3DStateParams));
            }
        }

        if (lpState->OffsetEventCtlData >= 0)
        {
            stMixEventHdr* const lpHdr = reinterpret_cast<stMixEventHdr*>(
                reinterpret_cast<char*>(lpState) + lpState->OffsetEventCtlData);
            m_EventCtlsAdded += lpHdr->NumEvents * liCopies;
            m_SharedEvtMixCtlCount += lpHdr->NumEvents;
            int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);
            for (int liCtl = 0; liCtl < lpHdr->NumEvents; ++liCtl)
            {
                const int liScaleCount = (lpEntry[1] >> 16) & 0xF;
                const int* lpScale = lpEntry + 6;
                int liExpanded = 0;
                for (int li = 0; li < liScaleCount; ++li)
                {
                    const int liRefState = (lpScale[li] >> 16) & 0xFF;
                    liExpanded += (liRefState == liState) ? 1 : m_StateRefCount[liRefState];
                }
                m_ScaleParamsAdded += liCopies * liExpanded;
                lpEntry += liScaleCount + 6;
            }
        }

        if (lpState->OffsetSubMixData >= 0)
        {
            stMixChHdr* const lpHdr = reinterpret_cast<stMixChHdr*>(
                reinterpret_cast<char*>(lpState) + lpState->OffsetSubMixData);
            m_SharedSubMixCount += lpHdr->NumMixChannels;
            m_SubMixChannelsAdded += lpHdr->NumMixChannels * liCopies;
            int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);
            for (int liChannel = 0; liChannel < lpHdr->NumMixChannels; ++liChannel)
            {
                const int liCount = (lpEntry[0] >> 16) & 0xFF;
                int liExpanded = 0;
                for (int li = 0; li < liCount; ++li)
                {
                    const int liRefState = (lpEntry[2 + li] >> 16) & 0xFF;
                    liExpanded += (liRefState == liState) ? 1 : m_StateRefCount[liRefState];
                }
                m_nTotalSubChannelInputs += liCopies * liExpanded;
                lpEntry += liCount + 2;
            }
        }

        if (lpState->OffsetMasterMixData >= 0)
        {
            stMixChHdr* const lpHdr = reinterpret_cast<stMixChHdr*>(
                reinterpret_cast<char*>(lpState) + lpState->OffsetMasterMixData);
            m_SharedMasterMixCount += lpHdr->NumMixChannels;
            m_MasterChannelsAdded += lpHdr->NumMixChannels * liCopies;
            m_nTotalUniqueMasterChannels += (liCopies + 1) * lpHdr->NumUniqueSFXOBJs;
            int* lpEntry = reinterpret_cast<int*>(lpHdr + 1);
            for (int liChannel = 0; liChannel < lpHdr->NumMixChannels; ++liChannel)
            {
                const int liCount = (lpEntry[0] >> 16) & 0xFF;
                int liExpanded = 0;
                for (int li = 0; li < liCount; ++li)
                {
                    const int liRefState = (lpEntry[3 + li] >> 16) & 0xFF;
                    liExpanded += (liRefState == liState) ? 1 : m_StateRefCount[liRefState];
                }
                m_nTotalMasterChannelInputs += liCopies * liExpanded;
                lpEntry += liCount + 3;
            }
        }
    }

    for (int liType = 0; liType < 10; ++liType)
        m_CurveProcsAdded += m_CurveProcsTotal[liType][0];
}

int NFSMixMap::SetupStateRefCount()
{
    char* const lpBlob = reinterpret_cast<char*>(m_pMMHdr);
    const int* const lpOffsets = reinterpret_cast<const int*>(
        lpBlob + m_pMMHdr->StateTableOffset);
    for (int liState = 0; liState < m_pMMHdr->NumStates; ++liState)
    {
        m_StateRefCount[liState] = lpOffsets[liState] < 0
            ? 0 : mpMixerInterface->GetStateCount(liState);
    }
    return 0;
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
int** NFSMixMap::GetMasterChannelInputPtr(int liN)
{
    int** lpPtr = &m_pMasterChannelInputs[m_CurrentMasterInputBlockOffset];
    m_CurrentMasterInputBlockOffset += liN;                                // (+0x220) += liN
    return lpPtr;
}

// GetSubChannelInputPtr @0x82B49638 -- &subInputBlock[cursor]; cursor += liN.
int** NFSMixMap::GetSubChannelInputPtr(int liN)
{
    int** lpPtr = &m_pSubChannelInputs[m_CurrentSubInputBlockOffset];
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
    if (lbAdvance) m_CurrentStateProcBlockOffset = liOff + static_cast<int>(sizeof(NFSMixMapState));
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
    // The porter preserves each serialized dword's numeric value. ARTIST's BE
    // `lhz 4` therefore maps to the high half of host word 1, not to the host
    // halfword physically resident at byte +4.
    int* const lpWords = reinterpret_cast<int*>(lpScaleParams);
    const int liCount = (lpWords[1] >> 16) & 0x1F;       // ARTIST lhz +4
    if (liCount == 0)
        return 0;

    int** lpArray = m_pScalePtrArray;                    // +0x180
    int** lpBase  = &lpArray[m_ScaleParamsIDCount];
    const int liSelfType = (lpWords[0] >> 16) & 0xFF;    // ARTIST lhz +0, clrlwi 24
    const int* lpEntry   = lpWords + 2;                  // scale list @ byte8

    int liAdded = 0;
    for (int li = 0; li < liCount; ++li)
    {
        const int liVal      = *lpEntry++;
        const int liEntryType = (liVal >> 16) & 0xFF;
        if (liEntryType == liSelfType)
        {
            lpArray[m_ScaleParamsIDCount + liAdded++] =
                reinterpret_cast<int*>(static_cast<intptr_t>((liProcIdx << 11) | liVal));
        }
        else
        {
            const int liCopies = m_StateRefCount[liEntryType]; // this+8+4*type
            for (int lj = 0; lj < liCopies; ++lj)
            {
                lpArray[m_ScaleParamsIDCount + liAdded++] =
                    reinterpret_cast<int*>(static_cast<intptr_t>((lj << 11) | liVal));
            }
        }
    }

    // write the produced count into the top byte of the u32 @ byte4 of the param blob.
    lpWords[1] = (liAdded << 24) | (lpWords[1] & 0xFFFFFF);
    m_ScaleParamsIDCount += liAdded;
    return reinterpret_cast<int*>(lpBase);
}

// ---------------------------------------------------------------------------
// NFSMixMap::AddEvtScaleIDs @0x82B493F8 -- the event-mix-control twin of AddScaleIDs.
// Expands an event control's u-scale input list (the packed ids that follow the
// stMixEvtParams header) into the shared packed scale-ptr array: for each entry, one
// packed id if its state matches the control's own state, else one per active copy of
// the referenced state (m_StateRefCount). Bails when the scale-ptr array is already
// full (m_ScaleParamsIDCount == m_ScaleParamsAdded) or the control has no u-scale
// inputs (nUScaleCntSwing low 5 bits == 0). Writes the produced count into the swing
// word's top byte and advances m_ScaleParamsIDCount. Returns the base of the range.
// FLAG (PC pointer-width): the packed 32-bit ids are stored through the m_pScalePtrArray
// pointer slots (reinterpret) so the stride matches ConnectMixMap's later object-ptr fill.
// ---------------------------------------------------------------------------
int* NFSMixMap::AddEvtScaleIDs(stMixEvtParams* lpEvtParams, int liProcIdx)
{
    if (m_ScaleParamsIDCount == m_ScaleParamsAdded)      // +0xD8 == +0xD4: array full
        return 0;

    const int liCount = (lpEvtParams->nUScaleCntSwing >> 16) & 0x1F; // ARTIST lhz +4
    if (liCount == 0)
        return 0;

    int** lpArray = m_pScalePtrArray;                    // +0x180
    int** lpBase  = &lpArray[m_ScaleParamsIDCount];
    const int liSelfType = (lpEvtParams->nEVTCTLID >> 16) & 0xFF; // ARTIST lhz +0
    const int* lpEntry   = reinterpret_cast<const int*>(lpEvtParams + 1); // u-scale list @ +0x18

    int liAdded = 0;
    for (int li = 0; li < liCount; ++li)
    {
        const int liVal       = *lpEntry++;
        const int liEntryType = (liVal >> 16) & 0xFF;
        if (liEntryType == liSelfType)
        {
            lpArray[m_ScaleParamsIDCount + liAdded++] =
                reinterpret_cast<int*>(static_cast<intptr_t>((liProcIdx << 11) | liVal));
        }
        else
        {
            const int liCopies = m_StateRefCount[liEntryType]; // this+8+4*type
            for (int lj = 0; lj < liCopies; ++lj)
            {
                lpArray[m_ScaleParamsIDCount + liAdded++] =
                    reinterpret_cast<int*>(static_cast<intptr_t>((lj << 11) | liVal));
            }
        }
    }

    // produced count -> top byte of the swing word (nUScaleCntSwing @ +0x04).
    lpEvtParams->nUScaleCntSwing = (liAdded << 24) | (lpEvtParams->nUScaleCntSwing & 0xFFFFFF);
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
        int** lppInputs = lpU->pInputs;
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

    m_pMasterChannelInputs = static_cast<int**>(    // +0x204 (ptr array on runtime)
        lpAlloc->Allocate(sizeof(int*) * m_nTotalMasterChannelInputs, 16, "Master Channel Input Array Block"));
    m_pSubChannelInputs = static_cast<int**>(       // +0x208 (ptr array on runtime)
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

namespace
{
    static int ClampInt(int aiValue, int aiMin, int aiMax)
    {
        return aiValue < aiMin ? aiMin : (aiValue > aiMax ? aiMax : aiValue);
    }

    static int ConvertMasterPreset(int aiValue, int aiType)
    {
        switch (aiType)
        {
            case 0:
            case 4:
                return NFSMixShape::GetQ15FromHundredthsdB(
                    ClampInt(aiValue, -10000, 0));
            case 1:
                return ClampInt(aiValue, -4800, 2400);
            case 2:
                return static_cast<int>(NFSMixShape::GetPitchMultFromCents(
                    ClampInt(aiValue, -10000, 0)) * 25000.0f);
            default:
                return ClampInt(aiValue, 0, 25000);
        }
    }

    static void StorePackedOutput(int* apOutputs, int aiSlot, int aiValue)
    {
        u32& lrWord = reinterpret_cast<u32*>(apOutputs)[aiSlot >> 1];
        const u16 luValue = static_cast<u16>(aiValue);
        if (aiSlot & 1)
            lrWord = (lrWord & 0x0000FFFFu) | (static_cast<u32>(luValue) << 16);
        else
            lrWord = (lrWord & 0xFFFF0000u) | luValue;
    }
}

void NFSMixMap::UpdateMasterChannels()
{
    for (int liChannel = 0; liChannel < m_MasterChannelsAdded; ++liChannel)
    {
        stMasterMixChProc& lrProc = m_pMasterChProc[liChannel];
        stMasterMixChSharedData* lpS = lrProc.pMixChData_S;
        stMasterMixChUniqueData* lpU = lrProc.pMixChData_U;
        int* lpOutputs = lpU->pOutputs;

        if (!lpOutputs || (lpOutputs[15] & 1) == 0)
        {
            lpU->Output = -10000;
            continue;
        }

        // ARTIST loads this with `lhz +4` from the big-endian dword, then
        // sign-extends it: the value is the serialized word's HIGH halfword.
        int liMix = static_cast<s16>(static_cast<u32>(lpS->pMapParams->MixData) >> 16);
        const int liStateInputs = lpS->NumInputs & 0xFF;
        for (int li = 0; li < liStateInputs; ++li)
        {
            const int liInput = *lpU->pInputs[li];
            if (liInput == -10000)
            {
                liMix = -10000;
                break;
            }
            liMix += liInput;
        }

        int* lpPreset = lpS->pPRESETS;
        if (!lpPreset)
        {
            lpU->Output = liMix;
            continue;
        }
        const int liPresetCount = lpPreset[0] & 0x1F;
        const int liPresetType = (lpPreset[0] >> 24) & 0xF;
        const int li3DInputs = (lpS->NumInputs >> 16) & 0x1F;
        for (int liPreset = 0; liPreset < liPresetCount; ++liPreset)
        {
            const int liDescriptor = lpPreset[liPreset + 1];
            const int li3DIndex = (liDescriptor >> 21) & 0x1F;
            const int liOffset = static_cast<s16>(liDescriptor & 0xFFFF);
            int liOutput;

            if (li3DInputs <= 0 || li3DIndex >= li3DInputs)
            {
                liOutput = ConvertMasterPreset(liMix + liOffset, liPresetType);
            }
            else
            {
                st3DMixCtlProc* lp3D = lpU->p3DData[li3DIndex];
                if (!lp3D)
                {
                    liOutput = ConvertMasterPreset(liMix + liOffset, liPresetType);
                }
                else if (liDescriptor < 0)
                {
                    liOutput = lp3D->p3DMixCtlData_U->azimuth;
                }
                else
                {
                    switch (liPresetType)
                    {
                        case 0:
                        case 4:
                            liOutput = ConvertMasterPreset(
                                lp3D->p3DMixCtlData_U->dBRolloff + liMix + liOffset,
                                liPresetType);
                            break;
                        case 1:
                            liOutput = ConvertMasterPreset(
                                lp3D->p3DMixCtlData_U->DopplerCents + liMix + liOffset,
                                liPresetType);
                            break;
                        case 2:
                            liOutput = ClampInt(liMix + liOffset, -10000, 0);
                            break;
                        default:
                            liOutput = liMix;
                            break;
                    }
                }
            }
            StorePackedOutput(lpOutputs, (liDescriptor >> 26) & 0x1F, liOutput);
        }
        lpU->Output = liMix;
    }
}

namespace
{
    static bool IsExternalDMixID(u32 auID)
    {
        const u32 luType = auID & 0xE0000000u;
        return luType == 0x40000000u || luType == 0x60000000u || luType == 0x80000000u;
    }

    static void AddUniqueDMixID(std::vector<u32>& arIDs, u32 auID)
    {
        if (!IsExternalDMixID(auID))
            return;
        auID &= 0xE0FFFFF0u;
        if (std::find(arIDs.begin(), arIDs.end(), auID) == arIDs.end())
            arIDs.push_back(auID);
    }
}

int* NFSMixMap::AllocateInputArrays()
{
    std::vector<u32> laIDs;
    for (int li = 0; li < m_CurveProcsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(m_pCurveDataArray[li].nINPUTID));
    for (int li = 0; li < m_ScaleParamsIDCount; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_pScalePtrArray[li])));
    for (int li = 0; li < m_3DMixCtlsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_p3DMixCtlData_U[li].pInputs)));
    for (int li = 0; li < m_EventCtlsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_pEvtMixCtlData_U[li].pTriggerPtr)));

    const size_t luCount = laIDs.size() * 16u;
    m_pDynMixInputBlocks = static_cast<int*>(g_pMixerAllocator->Allocate(
        static_cast<unsigned int>(luCount * sizeof(int)), 16,
        "DMIX SFXOBJ, SFXCTL Input Block"));
    if (m_pDynMixInputBlocks && luCount)
        std::memset(m_pDynMixInputBlocks, 0, luCount * sizeof(int));
    return m_pDynMixInputBlocks;
}

void NFSMixMap::AllocateDMixIOArrays()
{
    std::vector<u32> laIDs;
    for (int li = 0; li < m_MasterChannelsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(m_pMasterChData_U[li].outputID));
    for (int li = 0; li < m_CurveProcsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(m_pCurveDataArray[li].nINPUTID));
    for (int li = 0; li < m_ScaleParamsIDCount; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_pScalePtrArray[li])));
    for (int li = 0; li < m_3DMixCtlsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_p3DMixCtlData_U[li].pInputs)));
    for (int li = 0; li < m_EventCtlsAdded; ++li)
        AddUniqueDMixID(laIDs, static_cast<u32>(reinterpret_cast<uintptr_t>(m_pEvtMixCtlData_U[li].pTriggerPtr)));

    m_nTotalDMixIO = static_cast<int>(laIDs.size());
    m_nTotalDMix3DIO = 0;
    for (size_t li = 0; li < laIDs.size(); ++li)
        if ((laIDs[li] & 0xE0000000u) == 0x80000000u)
            ++m_nTotalDMix3DIO;

    if (m_nTotalDMixIO <= 0)
        return;
    m_pDMixIOObj = static_cast<Nicotine::DMixIO**>(g_pMixerAllocator->Allocate(
        sizeof(Nicotine::DMixIO*) * m_nTotalDMixIO, 16, "DMixIO Ptr Array"));
    m_pDMixIOMemBlock = static_cast<Nicotine::DMixIO*>(g_pMixerAllocator->Allocate(
        sizeof(Nicotine::DMixIO) * m_nTotalDMixIO, 16, "DMixIO Object Memory"));
    for (int li = 0; li < m_nTotalDMixIO; ++li)
    {
        Nicotine::DMixIO* lpIO = new (&m_pDMixIOMemBlock[li]) Nicotine::DMixIO();
        lpIO->SetDMixID(0xFFFFFFFFu);
        m_pDMixIOObj[li] = lpIO;
    }
}

int NFSMixMap::SETSFXID(int liID, int* lpObj)
{
    const u32 luID = static_cast<u32>(liID) & 0xE0FFFFF0u;
    int liIndex = 0;
    while (liIndex < m_nAssignedDMixIOBlocks && m_pDMixIOObj[liIndex]->GetDMixID() != luID)
        ++liIndex;

    if (liIndex < m_nAssignedDMixIOBlocks)
    {
        if (!m_pDMixIOObj[liIndex]->m_pDMixOutputBlock)
            m_pDMixIOObj[liIndex]->m_pDMixOutputBlock = lpObj;
        return 1;
    }

    CGS_ASSERT(m_nAssignedDMixIOBlocks < m_nTotalDMixIO, "m_nAssignedDMixIOBlocks < m_nTotalDMixIO");
    if (m_nAssignedDMixIOBlocks >= m_nTotalDMixIO)
        return 0;
    Nicotine::DMixIO* lpIO = m_pDMixIOObj[m_nAssignedDMixIOBlocks++];
    lpIO->m_pDMixOutputBlock = lpObj;
    lpIO->SetDMixID(luID);
    mpMixerInterface->ConnectDMixIO(lpIO);
    return 1;
}

void* NFSMixMap::GetObjectPtr(int liID, char lbDbOutput, char lbProcOutput)
{
    m_dummyout = 0;
    const u32 luID = static_cast<u32>(liID);
    const u32 luType = luID & 0xE0000000u;
    const int liState = (liID >> 16) & 0xFF;
    const int liCopy = (liID >> 11) & 0x1F;
    const unsigned char luProc = static_cast<unsigned char>(liID & 0xFF);

    if (luType == 0x20000000u)
    {
        NFSMixMapState* lpState = liState < m_pMMHdr->NumStates ? m_pStateProcs[liState] : 0;
        if (!lpState) return &m_dummyout;
        if (luID & 0x10000000u)
        {
            stSubMixChProc* lpProc = lpState->GetSubMixChProc(luProc, liCopy);
            return lpProc ? &lpProc->pMixChData_U->Output : &m_dummyout;
        }
        stMasterMixChProc* lpProc = lpState->GetMasterMixChProc(luProc, liCopy);
        return lpProc ? &lpProc->pMixChData_U->Output : &m_dummyout;
    }

    if (luType == 0x80000000u)
    {
        NFSMixMapState* lpState = liState < m_pMMHdr->NumStates ? m_pStateProcs[liState] : 0;
        st3DMixCtlProc* lpProc = lpState ? lpState->Get3DMixCtlProc(luProc, liCopy) : 0;
        if (!lpProc) return &m_dummyout;
        if (lbProcOutput) return lpProc;
        return lbDbOutput ? static_cast<void*>(&lpProc->p3DMixCtlData_U->dBRolloff)
                          : static_cast<void*>(&lpProc->p3DMixCtlData_U->q15Rolloff);
    }

    if (luType == 0xA0000000u)
    {
        NFSMixMapState* lpState = liState < m_pMMHdr->NumStates ? m_pStateProcs[liState] : 0;
        stEvtMixCtlProc* lpProc = lpState ? lpState->GetEvtMixCtlProc(luProc, liCopy) : 0;
        if (!lpProc) return &m_dummyout;
        return lbDbOutput ? static_cast<void*>(&lpProc->pData_U->output)
                          : static_cast<void*>(&lpProc->pData_U->qoutput);
    }

    if (luType == 0)
    {
        NFSMixMapState* lpState = liState < m_pMMHdr->NumStates ? m_pStateProcs[liState] : 0;
        stMixCtlProc* lpProc = lpState ? lpState->GetMixCtlProc(luProc, liCopy) : 0;
        if (!lpProc) return &m_dummyout;
        return lbDbOutput ? static_cast<void*>(&lpProc->pudata->CmpdBOut)
                          : static_cast<void*>(&lpProc->pudata->pstCurveData->Q15Output);
    }

    if (luType == 0x40000000u || luType == 0x60000000u)
    {
        const u32 luDMixID = luID & 0xE0FFFFF0u;
        int liIndex = 0;
        while (liIndex < m_nAssignedDMixIOBlocks &&
               m_pDMixIOObj[liIndex]->GetDMixID() != luDMixID)
            ++liIndex;
        if (liIndex == m_nAssignedDMixIOBlocks)
        {
            CGS_ASSERT(m_nAssignedDMixIOBlocks < m_nTotalDMixIO,
                       "m_nAssignedDMixIOBlocks < m_nTotalDMixIO");
            if (m_nAssignedDMixIOBlocks >= m_nTotalDMixIO)
                return &m_dummyout;
            Nicotine::DMixIO* lpIO = m_pDMixIOObj[m_nAssignedDMixIOBlocks++];
            lpIO->SetDMixID(luDMixID);
            lpIO->m_pDMixInputBlock = &m_pDynMixInputBlocks[m_nAssignedInputBlocks++ * 16];
            std::memset(lpIO->m_pDMixInputBlock, 0, 16 * sizeof(int));
            mpMixerInterface->ConnectDMixIO(lpIO);
        }
        Nicotine::DMixIO* lpIO = m_pDMixIOObj[liIndex];
        if (!lpIO->m_pDMixInputBlock)
        {
            lpIO->m_pDMixInputBlock = &m_pDynMixInputBlocks[m_nAssignedInputBlocks++ * 16];
            std::memset(lpIO->m_pDMixInputBlock, 0, 16 * sizeof(int));
        }
        return &lpIO->m_pDMixInputBlock[liID & 0xF];
    }

    return &m_dummyout;
}

void NFSMixMap::ConnectMixMap()
{
    for (int li = 0; li < m_MasterChannelsAdded; ++li)
    {
        stMasterMixChUniqueData& lrData = m_pMasterChData_U[li];
        const int liConnected = SETSFXID(lrData.outputID, lrData.pOutputs);
        std::memset(lrData.pOutputs, 0, 16 * sizeof(int));
        lrData.pOutputs[15] = liConnected != 0;
    }
    for (int li = 0; li < m_CurveProcsAdded; ++li)
        m_pCurveDataArray[li].pInputParam = static_cast<int*>(
            GetObjectPtr(m_pCurveDataArray[li].nINPUTID, 0, 0));
    for (int li = 0; li < m_ScaleParamsIDCount; ++li)
        m_pScalePtrArray[li] = static_cast<int*>(GetObjectPtr(
            static_cast<int>(reinterpret_cast<intptr_t>(m_pScalePtrArray[li])), 0, 0));
    for (int li = 0; li < m_3DMixCtlsAdded; ++li)
        m_p3DMixCtlData_U[li].pInputs = static_cast<int*>(GetObjectPtr(
            static_cast<int>(reinterpret_cast<intptr_t>(m_p3DMixCtlData_U[li].pInputs)), 0, 0));
    for (int li = 0; li < m_EventCtlsAdded; ++li)
        m_pEvtMixCtlData_U[li].pTriggerPtr = static_cast<int*>(GetObjectPtr(
            static_cast<int>(reinterpret_cast<intptr_t>(m_pEvtMixCtlData_U[li].pTriggerPtr)), 0, 0));

    for (int li = 0; li < m_nTotalMasterChannelInputs; ++li)
        m_pMasterChannelInputs[li] = static_cast<int*>(GetObjectPtr(
            static_cast<int>(reinterpret_cast<intptr_t>(m_pMasterChannelInputs[li])), 1, 1));

    for (int li = 0; li < m_nTotalSubChannelInputs; ++li)
        m_pSubChannelInputs[li] = static_cast<int*>(GetObjectPtr(
            static_cast<int>(reinterpret_cast<intptr_t>(m_pSubChannelInputs[li])), 1, 0));

}

void NFSMixMap::SetupStateProcArrays()
{
    for (int liState = 0; liState < m_pMMHdr->NumStates; ++liState)
    {
        NFSMixMapState* lpState = m_pStateProcs[liState];
        if (!lpState)
            continue;
        const int liCopies = lpState->GetStateRefCount();
        for (int liCopy = 0; liCopy < liCopies; ++liCopy)
        {
            NFSMixMapState* lpCopy = lpState->GetMixMapProc(liCopy);
            lpCopy->InitializeSubChannels();
            lpCopy->InitializeMasterChannels();
        }
    }
}

void NFSMixMap::InitMainMapStates()
{
    SetupStateProcArrays();
    AllocateDMixIOArrays();
    ConnectMixMap();
}

// ---- RELOCATED HOME (2026-08-25, audio-faithfulness wave 2; from AptRenderLinkStubs.cpp,
// a 2026-08-07 targeted-export placement artifact) ----
// NFSMixMap::CreateMainMapState @0x82B49680 (targeted export 2026-08-07) -- build /
// extend the per-state NFSMixMapState and wire its serialized state header, then run
// the builder passes on the state copy for this object index:
//   * first touch of liState: carve the next NFSMixMapState from the state object
//     block (m_pStateProcMemBlock + the m_CurrentStateProcBlockOffset byte cursor;
//     console step 0x60 == X360 sizeof) and virtual-Initialize it (vtbl+4).
//   * AddMixState(liObjIdx, state) @0x82B4D660 (registers/creates the copy).
//   * the copy's m_pMMStateHdr = blob + stateOffsetTable[liState].
//   * CreateMixCtls @0x82B4C890 / [Create3DMixCtls -- FLAG below] / CreateEvtMixCtls
//     @0x82B4CE00 on the copy.
void NFSMixMap::CreateMainMapState(int liState, int liCopy, int liObjIdx)
{
    if (!m_pStateProcs[liState])                                  // +0x98 (lwzx)
    {
        // x64 carve stride = sizeof(NFSMixMapState) (the console's raw +0x60 step on
        // the x64 host is the recurring console-stride corruption class).
        const int liOffset = m_CurrentStateProcBlockOffset;       // +0x20C (byte cursor)
        NFSMixMapState* const lpFresh = reinterpret_cast<NFSMixMapState*>(
            reinterpret_cast<char*>(m_pStateProcMemBlock) + liOffset);   // +0x9C
        m_CurrentStateProcBlockOffset = liOffset + static_cast<int>(sizeof(NFSMixMapState));
        m_pStateProcs[liState] = lpFresh;
        m_pStateProcs[liState]->Initialize(this, liState, liCopy, liObjIdx);   // vtbl+4
    }

    NFSMixMapState* const lpState = m_pStateProcs[liState];
    lpState->AddMixState(liObjIdx, lpState);                      // @0x82B4D660

    // The per-state header record inside the loaded MixMap blob: the state-offset
    // table sits at blob + StateTableOffset (blob word 2); entry liState is the
    // record's own blob offset.
    char* const lpBlob = reinterpret_cast<char*>(m_pMMHdr);       // +0x74 (the serialized MixMap blob)
    const int liStateOffset =
        reinterpret_cast<const int*>(lpBlob + m_pMMHdr->StateTableOffset)[liState];   // serialized blob state table
    NFSMixMapState* const lpProc = lpState->GetMixMapProc(liObjIdx);   // @0x82B4D648
    lpProc->m_pMMStateHdr =
        reinterpret_cast<stMixMapStateHdr*>(lpBlob + liStateOffset);   // +0x1C (serialized blob record)

    lpProc->CreateMixCtls();      // @0x82B4C890
    lpProc->Create3DMixCtls();    // @0x82B4D100
    lpProc->CreateEvtMixCtls();   // @0x82B4CE00
}

void NFSMixMap::AssignMixCtlDataPtrs(stMixCtlProc* lpProc, int* lpEntry,
                                     int liObjectIndex, int liProcIdx)
{
    if (liObjectIndex)
    {
        const int liObjectId = ((lpEntry[0] >> 16) & 0xE000)
                             | (m_MapType << 8)
                             | (lpEntry[0] & 0x0FFF0000)
                             | liProcIdx;
        lpProc->psdata = 0;
        for (int li = 0; li < m_AssignedMixCtlsShared; ++li)
        {
            if (m_pMixCtlData_S[li].MIXCTLOBJID == liObjectId)
            {
                lpProc->psdata = &m_pMixCtlData_S[li];
                break;
            }
        }
        CGS_ASSERT(lpProc->psdata != 0, "lpProc->psdata");
    }
    else
    {
        lpProc->psdata = &m_pMixCtlData_S[m_AssignedMixCtlsShared++];
    }
    lpProc->pudata = &m_pMixCtlData_U[m_AssignedMixCtlsUnique++];
}
st3DMixCtlUniqueData* NFSMixMap::GetNext3DMixCtlUnique(char lbAdvance)
{
    st3DMixCtlUniqueData* lp = &m_p3DMixCtlData_U[m_nAssigned3DMixCtlUnique];
    if (lbAdvance) ++m_nAssigned3DMixCtlUnique;
    return lp;
}
