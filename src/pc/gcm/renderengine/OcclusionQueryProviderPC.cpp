#include "pc/gcm/renderengine/OcclusionQueryProviderPC.h"

#include <vector>
#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace renderengine
{
    namespace
    {
        constexpr u32 KU_OCCLUSION_QUERY_COUNT = 64;
        constexpr u32 KU_NO_OCCLUSION_ID = 0xFFFFFFFFu;

        enum class EOcclusionQueryStatePC : u8
        {
            E_UNAVAILABLE,
            E_BUILDING,
            E_ENDED,
            E_READY,
            E_CONSUMED
        };

        struct OcclusionQuerySlotPC
        {
            IDirect3DQuery9* mpQuery = nullptr;
            u64 muSurveySerial = 0;
            u32 muDeviceGeneration = 0;
            u32 muUnavailableGeneration = 0;
            DWORD muSampleCount = 0;
            EOcclusionQueryStatePC meState = EOcclusionQueryStatePC::E_UNAVAILABLE;
        };

        struct OcclusionConditionalScopePC
        {
            u32 muIdentifier = KU_NO_OCCLUSION_ID;
            u64 muSurveySerial = 0;
            u32 muDeviceGeneration = 0;
            bool mbHasSurvey = false;
        };

        struct OcclusionSurveyFramePC
        {
            u32 muIdentifier = KU_NO_OCCLUSION_ID;
            u64 muSurveySerial = 0;
            u32 muDeviceGeneration = 0;
            u32 muSubmissionAttempts = 0;
            bool mbQueryBegan = false;
            bool mbSingleSubmissionSucceeded = false;
        };

        struct OcclusionQueryProviderStatePC
        {
            IDirect3DDevice9* mpDevice = nullptr;
            u32 muDeviceGeneration = 1;
            u64 muNextSurveySerial = 0;
            std::vector<OcclusionSurveyFramePC> maSurveys;
            std::vector<OcclusionConditionalScopePC> maConditionalScopes;
            OcclusionQuerySlotPC maSlots[KU_OCCLUSION_QUERY_COUNT] = {};
        };

        OcclusionQueryProviderStatePC sOcclusionQueryProviderPC;

        // FLAG PC-platform leaf: bounded passive visibility evidence, explicitly opted in.
        // This is called only by the existing render-thread query/conditional path. It adds
        // no Issue/GetData/draw call and never modifies provider slots, tokens or scopes.
        void TraceOcclusionQueryPC(const char* lpStage, u32 luIdentifier, u64 luSerial, u32 luDetail)
        {
            static const bool sbEnabled = []()
            {
                const char* lpValue = std::getenv("BRN_OCCLUSION_DIAG");
                return lpValue != nullptr && lpValue[0] == '1' && lpValue[1] == '\0';
            }();
            static u32 suLines = 0;
            if (!sbEnabled || suLines >= 512u)
                return;
            char lacTrace[192];
            std::snprintf(lacTrace, sizeof(lacTrace),
                "[occ-provider] stage=%s id=%u serial=%llu detail=%u\n", lpStage,
                static_cast<unsigned>(luIdentifier), static_cast<unsigned long long>(luSerial),
                static_cast<unsigned>(luDetail));
            CgsDev::Log::WriteToLog(lacTrace);
            ++suLines;
        }

        void OcclusionQueryProviderPCBumpGeneration()
        {
            ++sOcclusionQueryProviderPC.muDeviceGeneration;
            if (sOcclusionQueryProviderPC.muDeviceGeneration == 0)
                ++sOcclusionQueryProviderPC.muDeviceGeneration;
        }

        bool OcclusionQueryProviderPCDeviceLost(HRESULT lResult)
        {
            return lResult == D3DERR_DEVICELOST || lResult == D3DERR_DEVICEHUNG
                || lResult == D3DERR_DEVICEREMOVED;
        }

        void OcclusionQueryProviderPCReleaseSlot(OcclusionQuerySlotPC& lrSlot)
        {
            if (lrSlot.mpQuery != nullptr)
            {
                lrSlot.mpQuery->Release();
                lrSlot.mpQuery = nullptr;
            }
            lrSlot.muSurveySerial = 0;
            lrSlot.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            lrSlot.muUnavailableGeneration = 0;
            lrSlot.muSampleCount = 0;
            lrSlot.meState = EOcclusionQueryStatePC::E_UNAVAILABLE;
        }

        void OcclusionQueryProviderPCClear(bool lbForgetDevice)
        {
            for (u32 li = 0; li < KU_OCCLUSION_QUERY_COUNT; ++li)
                OcclusionQueryProviderPCReleaseSlot(sOcclusionQueryProviderPC.maSlots[li]);
            sOcclusionQueryProviderPC.maSurveys.clear();
            sOcclusionQueryProviderPC.maConditionalScopes.clear();
            if (lbForgetDevice)
                sOcclusionQueryProviderPC.mpDevice = nullptr;
            OcclusionQueryProviderPCBumpGeneration();
        }

        bool OcclusionQueryProviderPCBindDevice(IDirect3DDevice9* lpDevice)
        {
            if (sOcclusionQueryProviderPC.mpDevice != lpDevice)
            {
                OcclusionQueryProviderPCClear(false);
                sOcclusionQueryProviderPC.mpDevice = lpDevice;
            }
            return lpDevice != nullptr;
        }

        bool OcclusionQueryProviderPCCreateSlotQuery(OcclusionQuerySlotPC& lrSlot,
                                                      IDirect3DDevice9* lpDevice)
        {
            if (lrSlot.mpQuery != nullptr
                && lrSlot.muDeviceGeneration == sOcclusionQueryProviderPC.muDeviceGeneration)
                return true;
            if (lrSlot.muUnavailableGeneration == sOcclusionQueryProviderPC.muDeviceGeneration)
                return false;

            OcclusionQueryProviderPCReleaseSlot(lrSlot);
            IDirect3DQuery9* lpQuery = nullptr;
            const HRESULT lResult = lpDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, &lpQuery);
            if (FAILED(lResult) || lpQuery == nullptr)
            {
                if (lpQuery != nullptr)
                    lpQuery->Release();
                if (OcclusionQueryProviderPCDeviceLost(lResult))
                {
                    OcclusionQueryProviderPCClear(false);
                    return false;
                }
                lrSlot.muUnavailableGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
                return false;
            }

            lrSlot.mpQuery = lpQuery;
            lrSlot.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            lrSlot.muUnavailableGeneration = 0;
            lrSlot.muSampleCount = 0;
            lrSlot.meState = EOcclusionQueryStatePC::E_UNAVAILABLE;
            return true;
        }

        u64 OcclusionQueryProviderPCNextSurveySerial()
        {
            ++sOcclusionQueryProviderPC.muNextSurveySerial;
            if (sOcclusionQueryProviderPC.muNextSurveySerial == 0)
                ++sOcclusionQueryProviderPC.muNextSurveySerial;
            return sOcclusionQueryProviderPC.muNextSurveySerial;
        }

        void OcclusionQueryProviderPCBeginSurveyInternal(IDirect3DDevice9* lpDevice,
                                                          u32 luIdentifier)
        {
            OcclusionSurveyFramePC lFrame;
            if (luIdentifier >= KU_OCCLUSION_QUERY_COUNT
                || !OcclusionQueryProviderPCBindDevice(lpDevice))
            {
                sOcclusionQueryProviderPC.maSurveys.push_back(lFrame);
                return;
            }

            OcclusionQuerySlotPC& lrSlot = sOcclusionQueryProviderPC.maSlots[luIdentifier];
            const u64 luSurveySerial = OcclusionQueryProviderPCNextSurveySerial();
            lFrame.muIdentifier = luIdentifier;
            lFrame.muSurveySerial = luSurveySerial;
            lFrame.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            lrSlot.muSurveySerial = luSurveySerial;
            lrSlot.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            lrSlot.muSampleCount = 0;
            lrSlot.meState = EOcclusionQueryStatePC::E_UNAVAILABLE;

            const bool lbHasQuery = OcclusionQueryProviderPCCreateSlotQuery(lrSlot, lpDevice);
            // Query creation may clear slot metadata; restore this survey's identity afterward.
            lrSlot.muSurveySerial = luSurveySerial;
            lrSlot.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            lFrame.muDeviceGeneration = sOcclusionQueryProviderPC.muDeviceGeneration;
            if (lbHasQuery)
            {
                const HRESULT lResult = lrSlot.mpQuery->Issue(D3DISSUE_BEGIN);
                if (SUCCEEDED(lResult))
                {
                    lrSlot.meState = EOcclusionQueryStatePC::E_BUILDING;
                    lFrame.mbQueryBegan = true;
                }
                else if (OcclusionQueryProviderPCDeviceLost(lResult))
                {
                    OcclusionQueryProviderPCClear(false);
                }
                else
                {
                    OcclusionQueryProviderPCReleaseSlot(lrSlot);
                }
            }
            sOcclusionQueryProviderPC.maSurveys.push_back(lFrame);
        }

        void OcclusionQueryProviderPCEndSurveyInternal()
        {
            if (sOcclusionQueryProviderPC.maSurveys.empty())
                return;
            const OcclusionSurveyFramePC lFrame = sOcclusionQueryProviderPC.maSurveys.back();
            sOcclusionQueryProviderPC.maSurveys.pop_back();
            if (!lFrame.mbQueryBegan || lFrame.muIdentifier >= KU_OCCLUSION_QUERY_COUNT
                || lFrame.muDeviceGeneration != sOcclusionQueryProviderPC.muDeviceGeneration)
                return;

            OcclusionQuerySlotPC& lrSlot = sOcclusionQueryProviderPC.maSlots[lFrame.muIdentifier];
            if (lrSlot.mpQuery == nullptr
                || lrSlot.muDeviceGeneration != lFrame.muDeviceGeneration
                || lrSlot.muSurveySerial != lFrame.muSurveySerial
                || lrSlot.meState != EOcclusionQueryStatePC::E_BUILDING)
                return;

            const HRESULT lResult = lrSlot.mpQuery->Issue(D3DISSUE_END);
            if (SUCCEEDED(lResult) && lFrame.muSubmissionAttempts == 1
                && lFrame.mbSingleSubmissionSucceeded)
            {
                lrSlot.meState = EOcclusionQueryStatePC::E_ENDED;
                TraceOcclusionQueryPC("ended", lFrame.muIdentifier, lFrame.muSurveySerial, lFrame.muSubmissionAttempts);
            }
            else if (SUCCEEDED(lResult))
            {
                // An empty or rejected box draw can produce a real S_OK/zero query. Do not
                // publish that count as visibility evidence for a mesh survey.
                OcclusionQueryProviderPCReleaseSlot(lrSlot);
            }
            else if (OcclusionQueryProviderPCDeviceLost(lResult))
            {
                OcclusionQueryProviderPCClear(false);
            }
            else
            {
                OcclusionQueryProviderPCReleaseSlot(lrSlot);
            }
        }

        void OcclusionQueryProviderPCBeginConditionalInternal(IDirect3DDevice9* lpDevice,
                                                               u32 luIdentifier)
        {
            OcclusionConditionalScopePC lScope;
            lScope.muIdentifier = luIdentifier;
            if (luIdentifier < KU_OCCLUSION_QUERY_COUNT
                && OcclusionQueryProviderPCBindDevice(lpDevice))
            {
                const OcclusionQuerySlotPC& lrSlot = sOcclusionQueryProviderPC.maSlots[luIdentifier];
                if (lrSlot.mpQuery != nullptr
                    && lrSlot.muDeviceGeneration == sOcclusionQueryProviderPC.muDeviceGeneration
                    && lrSlot.meState == EOcclusionQueryStatePC::E_ENDED)
                {
                    lScope.muSurveySerial = lrSlot.muSurveySerial;
                    lScope.muDeviceGeneration = lrSlot.muDeviceGeneration;
                    lScope.mbHasSurvey = true;
                }
            }
            sOcclusionQueryProviderPC.maConditionalScopes.push_back(lScope);
        }

        void OcclusionQueryProviderPCEndConditionalInternal()
        {
            if (!sOcclusionQueryProviderPC.maConditionalScopes.empty())
            {
                const OcclusionConditionalScopePC lScope =
                    sOcclusionQueryProviderPC.maConditionalScopes.back();
                sOcclusionQueryProviderPC.maConditionalScopes.pop_back();
                if (lScope.mbHasSurvey && lScope.muIdentifier < KU_OCCLUSION_QUERY_COUNT)
                {
                    OcclusionQuerySlotPC& lrSlot =
                        sOcclusionQueryProviderPC.maSlots[lScope.muIdentifier];
                    if (lrSlot.muDeviceGeneration == lScope.muDeviceGeneration
                        && lrSlot.muSurveySerial == lScope.muSurveySerial
                        && (lrSlot.meState == EOcclusionQueryStatePC::E_ENDED
                            || lrSlot.meState == EOcclusionQueryStatePC::E_READY))
                        lrSlot.meState = EOcclusionQueryStatePC::E_CONSUMED;
                }
            }
        }

        bool OcclusionQueryProviderPCShouldDrawInternal(IDirect3DDevice9* lpDevice)
        {
            if (sOcclusionQueryProviderPC.maConditionalScopes.empty())
                return true;
            if (!OcclusionQueryProviderPCBindDevice(lpDevice))
                return true;
            if (sOcclusionQueryProviderPC.maConditionalScopes.empty())
                return true;

            OcclusionConditionalScopePC& lrScope =
                sOcclusionQueryProviderPC.maConditionalScopes.back();
            if (!lrScope.mbHasSurvey || lrScope.muIdentifier >= KU_OCCLUSION_QUERY_COUNT
                || lrScope.muDeviceGeneration != sOcclusionQueryProviderPC.muDeviceGeneration)
            {
                TraceOcclusionQueryPC("unavailable", lrScope.muIdentifier, lrScope.muSurveySerial, 1u);
                return true;
            }

            OcclusionQuerySlotPC& lrSlot = sOcclusionQueryProviderPC.maSlots[lrScope.muIdentifier];
            if (lrSlot.mpQuery == nullptr
                || lrSlot.muDeviceGeneration != lrScope.muDeviceGeneration
                || lrSlot.muSurveySerial != lrScope.muSurveySerial
                || (lrSlot.meState != EOcclusionQueryStatePC::E_ENDED
                    && lrSlot.meState != EOcclusionQueryStatePC::E_READY))
                return true;

            if (lrSlot.meState == EOcclusionQueryStatePC::E_ENDED)
            {
                // This PC leaf waits only for the current survey bound to this scope. It never
                // substitutes a previous ID/batch/frame result; the synchronous wait is costly.
                for (;;)
                {
                    DWORD luSampleCount = 0;
                    const HRESULT lResult = lrSlot.mpQuery->GetData(
                        &luSampleCount, sizeof(luSampleCount), D3DGETDATA_FLUSH);
                    if (lResult == S_OK)
                    {
                        lrSlot.muSampleCount = luSampleCount;
                        lrSlot.meState = EOcclusionQueryStatePC::E_READY;
                        TraceOcclusionQueryPC("result", lrScope.muIdentifier, lrScope.muSurveySerial, static_cast<u32>(luSampleCount));
                        break;
                    }
                    if (lResult == S_FALSE)
                        continue;

                    // Errors, including device loss, are unavailable; never reinterpret them as
                    // an occluded (zero-count) result. The next survey may create a fresh query.
                    if (OcclusionQueryProviderPCDeviceLost(lResult))
                    {
                        OcclusionQueryProviderPCClear(false);
                        return true;
                    }
                    OcclusionQueryProviderPCReleaseSlot(lrSlot);
                    lrScope.mbHasSurvey = false;
                    return true;
                }
            }

            // Native D3D9 policy: nonzero passed samples draw. ARTIST does not pin Xenos polarity.
            TraceOcclusionQueryPC("decision", lrScope.muIdentifier, lrScope.muSurveySerial, lrSlot.muSampleCount != 0 ? 1u : 0u);
            return lrSlot.muSampleCount != 0;
        }
    }

    void OcclusionQueryProviderPCBeginSurvey(IDirect3DDevice9* lpDevice, u32 luIdentifier)
    {
        OcclusionQueryProviderPCBeginSurveyInternal(lpDevice, luIdentifier);
    }

    void OcclusionQueryProviderPCEndSurvey()
    {
        OcclusionQueryProviderPCEndSurveyInternal();
    }

    void OcclusionQueryProviderPCBeginConditional(IDirect3DDevice9* lpDevice, u32 luIdentifier)
    {
        OcclusionQueryProviderPCBeginConditionalInternal(lpDevice, luIdentifier);
    }

    void OcclusionQueryProviderPCEndConditional()
    {
        OcclusionQueryProviderPCEndConditionalInternal();
    }

    bool OcclusionQueryProviderPCShouldDraw(IDirect3DDevice9* lpDevice)
    {
        return OcclusionQueryProviderPCShouldDrawInternal(lpDevice);
    }

    OcclusionSurveySubmissionTokenPC OcclusionQueryProviderPCBeginSurveySubmission()
    {
        if (sOcclusionQueryProviderPC.maSurveys.empty())
            return 0;

        OcclusionSurveyFramePC& lrFrame = sOcclusionQueryProviderPC.maSurveys.back();
        if (!lrFrame.mbQueryBegan || lrFrame.muIdentifier >= KU_OCCLUSION_QUERY_COUNT
            || lrFrame.muDeviceGeneration != sOcclusionQueryProviderPC.muDeviceGeneration)
            return 0;

        OcclusionQuerySlotPC& lrSlot = sOcclusionQueryProviderPC.maSlots[lrFrame.muIdentifier];
        if (lrSlot.mpQuery == nullptr
            || lrSlot.muDeviceGeneration != lrFrame.muDeviceGeneration
            || lrSlot.muSurveySerial != lrFrame.muSurveySerial
            || lrSlot.meState != EOcclusionQueryStatePC::E_BUILDING)
            return 0;

        ++lrFrame.muSubmissionAttempts;
        if (lrFrame.muSubmissionAttempts != 1)
            lrFrame.mbSingleSubmissionSucceeded = false;
        return lrFrame.muSurveySerial;
    }

    void OcclusionQueryProviderPCFinishSurveySubmission(
        OcclusionSurveySubmissionTokenPC luToken, bool lbSubmitted)
    {
        if (luToken == 0 || sOcclusionQueryProviderPC.maSurveys.empty())
            return;

        OcclusionSurveyFramePC& lrFrame = sOcclusionQueryProviderPC.maSurveys.back();
        if (lrFrame.muSurveySerial == luToken && lrFrame.mbQueryBegan
            && lrFrame.muDeviceGeneration == sOcclusionQueryProviderPC.muDeviceGeneration
            && lrFrame.muSubmissionAttempts == 1)
            lrFrame.mbSingleSubmissionSucceeded = lbSubmitted;
    }

    void OcclusionQueryProviderPCInvalidate(IDirect3DDevice9* /*lpDevice*/)
    {
        OcclusionQueryProviderPCClear(true);
    }

    void OcclusionQueryProviderPCResume(IDirect3DDevice9* lpDevice)
    {
        if (lpDevice != nullptr)
            OcclusionQueryProviderPCBindDevice(lpDevice);
    }

    void OcclusionQueryLifecyclePC::Invalidate(IDirect3DDevice9Ex* lpDevice)
    {
        OcclusionQueryProviderPCInvalidate(lpDevice);
    }

    void OcclusionQueryLifecyclePC::Resume(IDirect3DDevice9Ex* lpDevice)
    {
        OcclusionQueryProviderPCResume(lpDevice);
    }
}
