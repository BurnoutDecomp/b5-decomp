#ifndef CGS_SOUND_LOGIC_CGSDMIXDIAG_H
#define CGS_SOUND_LOGIC_CGSDMIXDIAG_H

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY.
//
// Opt-in witness for the DYNAMIC MIXER (BRN_DMIX_DIAG=1, default OFF; =2 adds the
// per-master-channel table). Header-only so it needs no new mount line.
//
// It measures exactly what its name says: the NFS mix map the Nicotine mixer is
// running --
//   [dmix] connect  : each DMixIO endpoint as ConnectDMixIO binds it, with the id
//                     decode (state / instance / sfx / obj-vs-ctl) and the NAME of the
//                     logic effect that will write its inputs (the writer).
//   [dmix] map      : the node table once the map is built (counts per section).
//   [dmix] in       : a DMixIO's 16 INPUT words, printed when they change (at most
//                     once a second per endpoint) and in full every 10 s.
//   [dmix] out      : a DMixIO's 30 packed OUTPUT halfwords (the VOL/FREQ/DEPTH read
//                     GetDMixOutput does: (word[s>>1] >> (16*(s&1))) & 0x7FFF, /32768
//                     for the effect-side gain) + the enable word, same cadence.
//   [dmix] master   : (level 2) per master channel: state/channel, the SFXOBJ id it
//                     drives, the live MixData base (snapshot-written hundredths dB)
//                     and its summed liMix (Output); on change + every 10 s.
//   [dmix] settings : MixerControl's message-12 payload as the consumer reads it
//                     (both words) and its five published inputs once a second.
// Every line names the endpoint / channel it is about. Rate limited: change edges
// + a 10 s full dump, and a hard cap on the total lines per run (an assert/witness
// storm starves the harness).
// =============================================================================

#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace CgsSound
{
namespace Diag
{

inline int DMixDiagLevel()
{
    static int siLevel = -1;
    if (siLevel < 0)
    {
        const char* lpcEnv = std::getenv("BRN_DMIX_DIAG");
        siLevel = 0;
        if (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0')
        {
            siLevel = std::atoi(lpcEnv);
            if (siLevel <= 0)
                siLevel = 1;
        }
    }
    return siLevel;
}

inline bool DMixDiagEnabled() { return DMixDiagLevel() > 0; }

inline unsigned& DMixDiagLineCount()
{
    static unsigned suLines = 0;
    return suLines;
}

inline void DMixDiagPrintf(const char* lpcFormat, ...)
{
    if (!DMixDiagEnabled())
        return;
    unsigned& lruLines = DMixDiagLineCount();
    if (lruLines >= 12000u)
        return;
    ++lruLines;

    char lacMsg[768];
    std::va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lacMsg, sizeof(lacMsg), lpcFormat, lArgs);
    va_end(lArgs);
    CgsDev::Log::WriteToLog(lacMsg);
}

// ---- per-endpoint bookkeeping (writer name + last printed input/output words) ----
struct DMixDiagEndpoint
{
    const Nicotine::DMixIO* mpIO;
    char                    macName[48];
    int                     maiLastIn[16];
    int                     maiLastOut[16];
    float                   mfLastPrintIn;
    float                   mfLastPrintOut;
    bool                    mbSeenIn;
    bool                    mbSeenOut;
};

struct DMixDiagState
{
    DMixDiagEndpoint maEndpoints[192];
    int              miEndpoints;
    float            mfLastFullDump;
    bool             mbMapPrinted;
    int              maiLastMasterMix[256];
    int              maiLastMasterBase[256];
    float            mfLastMasterDump;
};

inline DMixDiagState& DMixDiagGetState()
{
    static DMixDiagState sState;
    static bool sbInit = false;
    if (!sbInit)
    {
        std::memset(&sState, 0, sizeof(sState));
        sState.mfLastFullDump   = -1000.0f;
        sState.mfLastMasterDump = -1000.0f;
        sbInit = true;
    }
    return sState;
}

inline DMixDiagEndpoint* DMixDiagFindEndpoint(const Nicotine::DMixIO* apIO, bool abCreate)
{
    DMixDiagState& lrState = DMixDiagGetState();
    for (int li = 0; li < lrState.miEndpoints; ++li)
        if (lrState.maEndpoints[li].mpIO == apIO)
            return &lrState.maEndpoints[li];
    if (!abCreate || lrState.miEndpoints >= 192)
        return 0;
    DMixDiagEndpoint* lpNew = &lrState.maEndpoints[lrState.miEndpoints++];
    std::memset(lpNew, 0, sizeof(*lpNew));
    lpNew->mpIO = apIO;
    std::strcpy(lpNew->macName, "?");
    lpNew->mfLastPrintIn  = -1000.0f;
    lpNew->mfLastPrintOut = -1000.0f;
    return lpNew;
}

inline const char* DMixDiagIdKind(unsigned auID)
{
    switch (auID & 0xE0000000u)
    {
    case 0x40000000u: return "SFXOBJ";
    case 0x60000000u: return "SFXCTL";
    case 0x80000000u: return "3D";
    case 0x20000000u: return "CHOUT";
    case 0xA0000000u: return "EVT";
    case 0x00000000u: return "MIXCTL";
    default:          return "?";
    }
}

// Called from CgsSound::Logic::DynamicMixer::ConnectDMixIO once the effect is resolved.
inline void DMixDiagConnect(const Nicotine::DMixIO* apIO, const char* apcEffectName,
                            int aiStateId, int aiInstance, int aiEffectId, bool abFound)
{
    if (!DMixDiagEnabled() || !apIO)
        return;
    DMixDiagEndpoint* lpEp = DMixDiagFindEndpoint(apIO, true);
    if (lpEp && apcEffectName)
    {
        std::strncpy(lpEp->macName, apcEffectName, sizeof(lpEp->macName) - 1);
        lpEp->macName[sizeof(lpEp->macName) - 1] = 0;
    }
    const unsigned luID = apIO->GetDMixID();
    DMixDiagPrintf("[dmix] connect id=0x%08X %s state=%d inst=%d sfx=%d obj=%d in=%d out=%d -> %s\n",
                   luID, DMixDiagIdKind(luID), aiStateId, aiInstance, aiEffectId,
                   (luID & 0xE0000000u) == 0x40000000u ? 1 : 0,
                   apIO->m_pDMixInputBlock ? 1 : 0, apIO->m_pDMixOutputBlock ? 1 : 0,
                   abFound ? (apcEffectName ? apcEffectName : "?") : "(no effect -- NOT connected)");
}

// The per-frame tick (called after IDynamicMixer::ProcessMixMap). afNow = logic game time.
inline void DMixDiagTick(Nicotine::IDynamicMixer& arMixer, float afNow)
{
    if (!DMixDiagEnabled())
        return;
    NFSMixMaster* lpMaster = arMixer.mMixMaster;
    if (!lpMaster || !lpMaster->m_bMapReady || !lpMaster->m_pMainMixMap)
        return;
    NFSMixMap* lpMap = lpMaster->m_pMainMixMap;
    DMixDiagState& lrState = DMixDiagGetState();

    if (!lrState.mbMapPrinted)
    {
        lrState.mbMapPrinted = true;
        DMixDiagPrintf("[dmix] map states=%d stateCopies=%d mixctls=%d curveprocs=%d subs=%d masters=%d "
                       "3d=%d events=%d dmixio=%d/%d (3d %d) inputBlocks=%d scaleIds=%d\n",
                       lpMap->m_pMMHdr ? lpMap->m_pMMHdr->NumStates : -1, lpMap->m_nStateMapCount,
                       lpMap->m_MixCtlsAdded, lpMap->m_CurveProcsAdded, lpMap->m_SubMixChannelsAdded,
                       lpMap->m_MasterChannelsAdded, lpMap->m_3DMixCtlsAdded, lpMap->m_EventCtlsAdded,
                       lpMap->m_nAssignedDMixIOBlocks, lpMap->m_nTotalDMixIO, lpMap->m_nTotalDMix3DIO,
                       lpMap->m_nAssignedInputBlocks, lpMap->m_ScaleParamsIDCount);
        for (int li = 0; li < lpMap->m_pMMHdr->NumStates; ++li)
            DMixDiagPrintf("[dmix] map state %d copies=%d\n", li, lpMap->m_StateRefCount[li]);
    }

    const bool lbFull = (afNow - lrState.mfLastFullDump) >= 10.0f;
    if (lbFull)
        lrState.mfLastFullDump = afNow;

    // ---- endpoints: inputs / outputs ----
    for (int li = 0; li < lpMap->m_nAssignedDMixIOBlocks && lpMap->m_pDMixIOObj; ++li)
    {
        Nicotine::DMixIO* lpIO = lpMap->m_pDMixIOObj[li];
        if (!lpIO)
            continue;
        DMixDiagEndpoint* lpEp = DMixDiagFindEndpoint(lpIO, true);
        if (!lpEp)
            continue;
        const unsigned luID = lpIO->GetDMixID();

        if (lpIO->m_pDMixInputBlock)
        {
            const int* lpIn = lpIO->m_pDMixInputBlock;
            const bool lbChanged = !lpEp->mbSeenIn || std::memcmp(lpEp->maiLastIn, lpIn, sizeof(lpEp->maiLastIn)) != 0;
            if ((lbChanged && (afNow - lpEp->mfLastPrintIn) >= 1.0f) || lbFull)
            {
                std::memcpy(lpEp->maiLastIn, lpIn, sizeof(lpEp->maiLastIn));
                lpEp->mbSeenIn = true;
                lpEp->mfLastPrintIn = afNow;
                DMixDiagPrintf("[dmix] in  t=%.2f id=0x%08X %s st=%d sfx=%d (%s) = [%d %d %d %d %d %d %d %d | %d %d %d %d %d %d %d %d]%s\n",
                               afNow, luID, DMixDiagIdKind(luID), (luID >> 16) & 0xFF, (luID >> 4) & 0x7F,
                               lpEp->macName,
                               lpIn[0], lpIn[1], lpIn[2], lpIn[3], lpIn[4], lpIn[5], lpIn[6], lpIn[7],
                               lpIn[8], lpIn[9], lpIn[10], lpIn[11], lpIn[12], lpIn[13], lpIn[14], lpIn[15],
                               lbFull ? " (full)" : "");
            }
        }
        if (lpIO->m_pDMixOutputBlock)
        {
            const int* lpOut = lpIO->m_pDMixOutputBlock;
            const bool lbChanged = !lpEp->mbSeenOut || std::memcmp(lpEp->maiLastOut, lpOut, sizeof(lpEp->maiLastOut)) != 0;
            if ((lbChanged && (afNow - lpEp->mfLastPrintOut) >= 1.0f) || lbFull)
            {
                std::memcpy(lpEp->maiLastOut, lpOut, sizeof(lpEp->maiLastOut));
                lpEp->mbSeenOut = true;
                lpEp->mfLastPrintOut = afNow;
                char lacSlots[400];
                int liPos = 0;
                for (int ls = 0; ls < 30 && liPos < static_cast<int>(sizeof(lacSlots)) - 16; ++ls)
                {
                    const int liV = (lpOut[ls >> 1] >> (16 * (ls & 1))) & 0xFFFF;
                    if (liV == 0)
                        continue;
                    liPos += std::snprintf(lacSlots + liPos, sizeof(lacSlots) - liPos, " %d:%d", ls, liV);
                }
                DMixDiagPrintf("[dmix] out t=%.2f id=0x%08X %s st=%d sfx=%d (%s) en=%d slots(raw hw)=[%s ]%s\n",
                               afNow, luID, DMixDiagIdKind(luID), (luID >> 16) & 0xFF, (luID >> 4) & 0x7F,
                               lpEp->macName, lpOut[15], lacSlots, lbFull ? " (full)" : "");
            }
        }
    }

    // ---- level 2: master channel table ----
    if (DMixDiagLevel() >= 2 && lpMap->m_pMasterChProc)
    {
        const bool lbMasterFull = (afNow - lrState.mfLastMasterDump) >= 10.0f;
        if (lbMasterFull)
            lrState.mfLastMasterDump = afNow;
        const int liCount = lpMap->m_MasterChannelsAdded < 256 ? lpMap->m_MasterChannelsAdded : 256;
        for (int li = 0; li < liCount; ++li)
        {
            stMasterMixChProc& lrProc = lpMap->m_pMasterChProc[li];
            stMasterMixChSharedData* lpS = lrProc.pMixChData_S;
            stMasterMixChUniqueData* lpU = lrProc.pMixChData_U;
            if (!lpS || !lpU || !lpS->pMapParams)
                continue;
            const int liBase = static_cast<short>(static_cast<unsigned>(lpS->pMapParams->MixData) >> 16);
            const int liMix  = lpU->Output;
            const int liDeltaMix  = liMix - lrState.maiLastMasterMix[li];
            const int liDeltaBase = liBase - lrState.maiLastMasterBase[li];
            if (!lbMasterFull && liDeltaMix < 25 && liDeltaMix > -25 && liDeltaBase == 0)
                continue;
            lrState.maiLastMasterMix[li]  = liMix;
            lrState.maiLastMasterBase[li] = liBase;
            char lacIn[200];
            int liPos = 0;
            const int liStateInputs = lpS->NumInputs & 0xFF;
            for (int lj = 0; lj < liStateInputs && lpU->pInputs && liPos < 180; ++lj)
                liPos += std::snprintf(lacIn + liPos, sizeof(lacIn) - liPos, " %d",
                                       lpU->pInputs[lj] ? *lpU->pInputs[lj] : -99999);
            lacIn[liPos] = 0;
            DMixDiagPrintf("[dmix] master t=%.2f #%d chid=0x%08X sfxobj=0x%08X out=0x%08X base=%d mix=%d en=%d in=[%s ]%s\n",
                           afNow, li, static_cast<unsigned>(lpS->MIXCHINID),
                           static_cast<unsigned>(lpS->pMapParams->SFXOBJID), static_cast<unsigned>(lpU->outputID),
                           liBase, liMix, lpU->pOutputs ? lpU->pOutputs[15] : -1, lacIn,
                           lbMasterFull ? " (full)" : "");
        }
    }
}

} // namespace Diag
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSDMIXDIAG_H
