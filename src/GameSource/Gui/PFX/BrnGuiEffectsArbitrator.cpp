#include "GameSource/Gui/PFX/BrnGuiEffectsArbitrator.h"
#include "GameSource/Gui/Events/BrnGuiPFXEvents.h"                    // the 495..501 events
#include "GameSource/Gui/BrnGuiCache.h"                               // BrnGui::GuiCache::GetTime
#include "GameSource/Graphics/BrnRendererModuleIO.h"                  // RendererIO::OutputBuffer::GetFXEventsEffectsFrame
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                // CgsGuiModuleIO::InputBuffer / OutputBuffer
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"        // ModelIO::InputBuffer / OutputBuffer
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // GuiEventLoadRequest / GuiEventLoadNotification
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::NULLResourceHandle
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf
#include "SharedClasses/Graphics/BrnEffectsData.h"                    // BrnEffectsFrame
#include <cstring>                                                    // _strnicmp
#include <cfloat>                                                     // FLT_MAX
#include <cstdlib>                                                    // getenv (BRN_PFX_DIAG)

// =============================================================================
// BrnGui::EffectsArbitrator -- reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                 @0x825175A0     ResourceUpdate            @0x825177E8
//   EventUpdate               @0x825125C0     GenerateEffectFrameEvents @0x82503060
//   GetHookFromName           @0x82502C78     GetHookFromGUID           @0x82502D70
//   StartHook                 @0x8250B618     StopHook                  @0x824FB398
//   StartBackgroundHook       @0x8250B1F0     StopBackgroundHook        @0x8250B478
//   StopMenuHook              @0x824FB478     UpdateHooks               @0x82502E38
//   Acquire3dTints            @0x82512AA8     LookupColourCube          @0x824F5B98
//   Debug_Start/StopHook      @0x82512550 / @0x824FB330
//   Debug_Start/StopBackground@0x825124F8 / @0x824FB2C8
// Console asserts cite "..\\..\\..\\GameSource\\Gui/PFX/BrnGuiEffectsArbitrator.cpp".
//
// The console's "is this blender active" test is inlined at every site as the loop over the
// node faders' state words (`cmpwi 4`); it is PFXHookNodeBlender::IsActive here.
// =============================================================================

namespace BrnGui
{
    namespace
    {
        // The load-request ids this class owns: the hook bundle rides request 228 (the boot
        // preload's "pfxhooks" entry), the colour cubes 229..234 (KU_MAX_NUM_COLOURCUBES).
        const u32 KU_PFX_HOOK_BUNDLE_REQUEST_ID  = 228;

        // THE QUEUE RECORD IS THE PAYLOAD. CgsGui::GuiModule::AddGuiEvent<T> pushes the event
        // MINUS its 12-byte GuiEvent<N> header (the console record: GUID at +0, the six maxima
        // at +4, the name at +28 -- exactly what StartHook @0x8250B618 reads), and
        // VariableEventQueue::GetFirstEvent hands back a pointer to that payload. Viewing it
        // through the header-bearing PC struct therefore means stepping BACK over the header:
        // the 12 bytes before the payload are the queue entry's own size/pad words, never
        // read through this view. (Dispatch of 64 / 501 keeps its own +0 convention.)
        template <class T>
        const T* PayloadAs(const CgsModule::Event* lpEvent)
        {
            const s32 KI_GUI_EVENT_HEADER_SIZE = 12;
            return reinterpret_cast<const T*>(reinterpret_cast<const u8*>(lpEvent) - KI_GUI_EVENT_HEADER_SIZE);
        }
        const u32 KU_PFX_COLOURCUBE_REQUEST_BASE = 229;
        // ARTIST's E_GUI_RESOURCETYPE_PFX_COLOURCUBE. The DecFIGS-derived enum in
        // CgsGuiResourceModuleIO.h ends one entry early (its 22 is DONE); the loader's switch in
        // CgsGuiResourceModule.cpp is keyed on the ARTIST numbering (`case 22: // pfx colour
        // cube -> the colour-cube dictionary`), which is what this request has to hit.
        const s32 KI_ARTIST_RESOURCETYPE_PFX_COLOURCUBE = 22;
        const s32 KI_LOAD_NOTIFICATION_EVENT_ID = 14;

        inline bool PfxDiag()
        {
            static const bool sbDiag = (getenv("BRN_PFX_DIAG") != 0);
            return sbDiag && CgsDev::Log::gpDebugPrint != 0;
        }
    }

    // ------------------------------------------------------------------------------------
    // Construct @0x825175A0
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::Construct()
    {
        mpGuiCache = 0;
        mpHookInfo = CgsResource::ResourcePtr<PFXHookBundle>(CgsResource::NULLResourceHandle);   // CreateFromHandle(&dword_82FB55C4)
        muColourCubeCount = 0;
        mpCurrent            = &mBlender1;
        mpOutgoing           = &mBlender2;
        mpBackgroundCurrent  = &mBackgroundBlender1;
        mpBackgroundOutgoing = &mBackgroundBlender2;
        mpMenu               = &mMenuBlender;
        mBlender1.Construct();
        mBlender2.Construct();
        mMenuBlender.Construct();
        mBackgroundBlender1.Construct();
        mBackgroundBlender2.Construct();
        mfLastTime         = 0.0f;
        mfForcedTime       = 0.0f;
        meHookState        = E_NORMAL;
        mfBackgroundWeight = 0.2f;
        mbForceTime        = false;
        miPfxHookIndex     = 0;
        // The console leaves these to the zeroed carve it lives in; spelled so the PC object
        // does not depend on its allocator.
        meStateBeforeMenu           = E_NORMAL;
        meBackgroundHookState       = E_BACKGROUND_INACTIVE;
        mfMenuPhaseTimeExpired      = 0.0f;
        mfMenuPhaseTimeToEnd        = 0.0f;
        mfCrossFadeTimeExpired      = 0.0f;
        mfCrossFadeTimeToEnd        = 0.0f;
        mfBackgroundFadeTimeExpired = 0.0f;
        mfBackgroundFadeTimeToEnd   = 0.0f;
        for (u32 luCube = 0; luCube < KU_MAX_NUM_COLOURCUBES; ++luCube)
        {
            mauColourCubeRequestIds[luCube] = 0;
            maColourCubes[luCube].muResourceId = 0;
        }
        for (s32 liHook = 0; liHook < 100; ++liHook)
        {
            maPfxHookList[liHook].miValue = 0;
            maPfxHookList[liHook].mpcName = 0;
        }

        // The "PFX" debug-menu group, in the console's registration order.
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.RegisterVariable(&mbForceTime, "PFX", "ForceTime");
        lDebugInterface.RegisterVariable(&mfForcedTime, "PFX", "Time");
        lDebugInterface.SetRange(&mfForcedTime, 0.0f, 100.0f);
        lDebugInterface.SetStep(&mfForcedTime, 0.0099999998f);
        lDebugInterface.RegisterVariable(&mfBackgroundWeight, "PFX", "Background Weight");
        lDebugInterface.SetRange(&mfBackgroundWeight, 0.0f, 1.0f);
        lDebugInterface.SetStep(&mfBackgroundWeight, 0.1f);
        lDebugInterface.RegisterVariable(&miPfxHookIndex, "PFX", "Hook");
        lDebugInterface.SetOptions(&miPfxHookIndex, maPfxHookList);
        lDebugInterface.SetRange(&miPfxHookIndex, 0, 0);
        lDebugInterface.RegisterFunction(&EffectsArbitrator::Debug_StartHook, this, "PFX", "Start Hook");
        lDebugInterface.RegisterFunction(&EffectsArbitrator::Debug_StopHook, this, "PFX", "Stop Hook");
        lDebugInterface.RegisterFunction(&EffectsArbitrator::Debug_StartBackgroundHook, this, "PFX", "Start As Background Hook");
        lDebugInterface.RegisterFunction(&EffectsArbitrator::Debug_StopBackgroundHook, this, "PFX", "Stop Background Hook");
    }

    void EffectsArbitrator::Destruct()
    {
    }

    // [FLAG PC bring-up] GUI event 64 (the GuiCache pointer) is what the console's EventUpdate
    // latches mpGuiCache from. The PC GuiModule::Update synthesises that event and routes it to
    // its subscribers directly rather than through the module-input queue EventUpdate walks, so
    // the module hands the cache over here instead -- the same store, the same one-shot guard.
    void EffectsArbitrator::SetGuiCache(GuiCache* lpGuiCache)
    {
        if (mpGuiCache == 0)
        {
            mpGuiCache = lpGuiCache;
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                    // :280
        }
    }

    // ------------------------------------------------------------------------------------
    // ResourceUpdate @0x825177E8 (BrnGuiEffectsArbitrator.cpp:171)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::ResourceUpdate(CgsGui::ModelIO::InputBuffer* lpGuiModelInputBuffer,
                                           const CgsGui::ModelIO::OutputBuffer* lpGuiModelOutputBuffer)
    {
        CGS_ASSERT(lpGuiModelInputBuffer != 0, "lpGuiModelInputBuffer != NULL");     // :171
        CGS_ASSERT(lpGuiModelOutputBuffer != 0, "lpGuiModelOutputBuffer != NULL");   // :172

        const CgsGui::ModelIO::OutputBuffer::GuiNotificationQueue* lpNotifications =
            lpGuiModelOutputBuffer->GetLoadNotifications();
        CGS_ASSERT(lpNotifications != 0, "Invalid queue in EffectsArbitrator::Update");   // :177
        if (lpNotifications == 0)
        {
            return;
        }

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpNotifications->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liId == KI_LOAD_NOTIFICATION_EVENT_ID)
            {
                const CgsGui::GuiEventLoadNotification* lpNotification =
                    static_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
                const u32 luRequestId = lpNotification->muLoadRequestId;
                if (PfxDiag())
                {
                    *CgsDev::Log::gpDebugPrint << "[pfx] load notification request " << luRequestId
                                               << " type " << static_cast<s32>(lpNotification->meRequestType) << "\n";
                }
                if (luRequestId == KU_PFX_HOOK_BUNDLE_REQUEST_ID)
                {
                    mpHookInfo = CgsResource::ResourcePtr<PFXHookBundle>(lpNotification->mResourceHandle);
                    CGS_ASSERT(!mpHookInfo.IsEqual(&CgsResource::NULLResourcePtr),
                               "mpHookInfo != CgsResource::NULLResourcePtr");           // :198
                    if (PfxDiag() && !mpHookInfo.IsEqual(&CgsResource::NULLResourcePtr))
                    {
                        *CgsDev::Log::gpDebugPrint << "[pfx] hook bundle bound: " << mpHookInfo->miHookCount
                                                   << " hooks, " << mpHookInfo->miGroupCount << " groups\n";
                    }
                    Acquire3dTints(lpGuiModelInputBuffer);
                }
                else if (luRequestId - KU_PFX_COLOURCUBE_REQUEST_BASE <= 5u)
                {
                    const u32 luCount = muColourCubeCount;
                    if (luCount != 0)
                    {
                        u32 luIndex = 0;
                        while (mauColourCubeRequestIds[luIndex] != luRequestId)
                        {
                            if (++luIndex >= luCount)
                            {
                                luIndex = luCount;   // not one of ours after all
                                break;
                            }
                        }
                        if (luIndex < luCount)
                        {
                            maColourCubes[luIndex].mpColourCube =
                                CgsResource::ResourcePtr<rw::graphics::postfx::ColourCube>(lpNotification->mResourceHandle);
                            if (PfxDiag())
                            {
                                *CgsDev::Log::gpDebugPrint << "[pfx] colour cube " << luIndex << " bound (request "
                                                           << luRequestId << ")\n";
                            }
                        }
                    }
                }
            }
            liId = lpNotifications->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ------------------------------------------------------------------------------------
    // Acquire3dTints @0x82512AA8 (BrnGuiEffectsArbitrator.cpp:477)
    // Every group that tints in 3D names a colour cube by resource id; each id not yet in the
    // table gets a slot, a request id and a model load request. Then the debug menu's hook
    // list is (re)built from the bundle's names.
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::Acquire3dTints(CgsGui::ModelIO::InputBuffer* lpGuiModelInputBuffer)
    {
        const PFXHookBundle* lpBundle = mpHookInfo.operator->();
        for (s32 liGroup = 0; liGroup < lpBundle->miGroupCount; ++liGroup)
        {
            const PFXGroup* lpGroup = lpBundle->GetGroup(liGroup);
            if (lpGroup->mbUseTint3D && LookupColourCube(lpGroup->muTint3DResourceId) == 0)
            {
                CGS_ASSERT(muColourCubeCount < KU_MAX_NUM_COLOURCUBES,
                           "muColourCubeCount < KU_MAX_NUM_COLOURCUBES");             // :477
                CgsGui::GuiEventLoadRequest lRequest;
                lRequest.meRequestType  = static_cast<CgsGui::ResourceRequestTypes>(KI_ARTIST_RESOURCETYPE_PFX_COLOURCUBE);
                lRequest.meLoadUnload   = CgsGui::E_GUI_RESOURCEREQUEST_LOAD;
                lRequest.mpacFileToLoad = 0;
                maColourCubes[muColourCubeCount].muResourceId = lpGroup->muTint3DResourceId;
                mauColourCubeRequestIds[muColourCubeCount]    = KU_PFX_COLOURCUBE_REQUEST_BASE + muColourCubeCount;
                lRequest.muLoadRequestId = mauColourCubeRequestIds[muColourCubeCount];
                lRequest.muResourceId    = lpGroup->muTint3DResourceId;
                lpGuiModelInputBuffer->AddResourceRequests(lRequest);
                if (PfxDiag())
                {
                    *CgsDev::Log::gpDebugPrint << "[pfx] colour cube " << muColourCubeCount << " requested (request "
                                               << lRequest.muLoadRequestId << ")\n";
                }
                ++muColourCubeCount;
            }
        }

        // The debug menu's "Hook" option list: {index, name} per hook, capped at the list.
        s32 liListed = 0;
        for (s32 liHook = 0; liHook < lpBundle->miHookCount && liListed < 100; ++liHook, ++liListed)
        {
            maPfxHookList[liListed].miValue = liListed;
            maPfxHookList[liListed].mpcName = lpBundle->GetHook(liHook)->macName;
        }
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.SetRange(&miPfxHookIndex, 0, lpBundle->miHookCount - 1);
    }

    // ------------------------------------------------------------------------------------
    // GetHookFromName @0x82502C78 (:527) / GetHookFromGUID @0x82502D70 (:554)
    // ------------------------------------------------------------------------------------
    const PFXHook* EffectsArbitrator::GetHookFromName(const char* lpcName)
    {
        CGS_ASSERT(!mpHookInfo.IsEqual(&CgsResource::NULLResourcePtr),
                   "mpHookInfo != CgsResource::NULLResourcePtr");                   // :527
        CGS_ASSERT(lpcName != 0, "lpcName != NULL");                                // :528
        const PFXHookBundle* lpBundle = mpHookInfo.operator->();
        for (s32 liHook = 0; liHook < lpBundle->miHookCount; ++liHook)
        {
            const PFXHook* lpHook = lpBundle->GetHook(liHook);
            if (_strnicmp(lpHook->macName, lpcName, KI_MAX_PFX_ID_LENGTH) == 0)
            {
                return lpHook;
            }
        }
        return 0;
    }

    const PFXHook* EffectsArbitrator::GetHookFromGUID(u32 luGuid)
    {
        CGS_ASSERT(!mpHookInfo.IsEqual(&CgsResource::NULLResourcePtr),
                   "mpHookInfo != CgsResource::NULLResourcePtr");                   // :554
        const PFXHookBundle* lpBundle = mpHookInfo.operator->();
        for (s32 liHook = 0; liHook < lpBundle->miHookCount; ++liHook)
        {
            const PFXHook* lpHook = lpBundle->GetHook(liHook);
            if (lpHook->muId == luGuid)
            {
                return lpHook;
            }
        }
        return 0;
    }

    // The three-step lookup the console inlines at StartHook (:596), EventUpdate's 496 arm
    // (:313) and the background pair (:392 / :443): by GUID, then by name, then the "2dflash"
    // fallback with a filtered log line. The background pair has NO fallback -- a miss there
    // is the assert -- so it passes a null fallback caller and asserts itself.
    const PFXHook* EffectsArbitrator::ResolveHook(u32 luGuid, const char* lpcName, const char* lpcCaller)
    {
        const PFXHook* lpHook = GetHookFromGUID(luGuid);
        if (lpHook != 0)
        {
            return lpHook;
        }
        lpHook = GetHookFromName(lpcName);
        if (lpHook != 0)
        {
            return lpHook;
        }
        if (lpcCaller == 0)
        {
            return 0;
        }
        lpHook = GetHookFromName("2dflash");
        if (PfxDiag())
        {
            *CgsDev::Log::gpDebugPrint << "[pfx] " << lpcCaller << ": '" << lpcName << "' / id " << luGuid
                                       << " not in the bundle -> 2dflash fallback\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "EffectsArbitrator::" << lpcCaller << ": Effect name: \""
                                       << lpcName << "\" or ID \"" << luGuid << "\" not found in bundle\n";
        }
        CGS_ASSERT(lpHook != 0, "2dflash effect is missing!");                    // :596 / :313
        return lpHook;
    }

    // ------------------------------------------------------------------------------------
    // StartHook @0x8250B618 (BrnGuiEffectsArbitrator.cpp:592)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::StartHook(const GuiPFXHookEvent* lpGuiPFXHookEvent)
    {
        const PFXHook* lpHook = ResolveHook(lpGuiPFXHookEvent->muGuid, lpGuiPFXHookEvent->macName, "StartHook");
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");                         // :598
        if (lpHook == 0)
        {
            return;
        }
        if (PfxDiag())
        {
            *CgsDev::Log::gpDebugPrint << "[pfx] StartHook '" << lpHook->macName << "' priority " << lpHook->miPriority
                                       << " menu " << (lpHook->IsMenu() ? 1 : 0) << " mode " << lpHook->meTransitionMode << "\n";
            const PFXHookBundle* lpDiagBundle = mpHookInfo.operator->();
            for (s32 liNode = 0; liNode < lpHook->miNodeCount; ++liNode)
            {
                const PFXHookNode* lpNode  = lpDiagBundle->GetHookNode(*lpHook, liNode);
                const PFXGroup*    lpGroup = (lpNode != 0) ? lpDiagBundle->GetNodeGroup(*lpNode) : 0;
                if (lpGroup == 0)
                    continue;
                *CgsDev::Log::gpDebugPrint << "[pfx]   node " << liNode << " group " << lpGroup->miID
                                           << " use b/v/d/bl/t2/t3 " << static_cast<s32>(lpGroup->mbUseBloom)
                                           << static_cast<s32>(lpGroup->mbUseVignette) << static_cast<s32>(lpGroup->mbUseDepthOfField)
                                           << static_cast<s32>(lpGroup->mbUseBlur) << static_cast<s32>(lpGroup->mbUseTint2D)
                                           << static_cast<s32>(lpGroup->mbUseTint3D)
                                           << " fade " << lpGroup->mfFadeIn << "/" << lpGroup->mfFadeOut
                                           << " dur " << lpGroup->mfDuration << "\n";
            }
        }
        const PFXHookBundle* lpBundle = mpHookInfo.operator->();
        PFXHookNodeBlender* lpCurrent  = mpCurrent;
        const bool          lbActive   = lpCurrent->IsActive();

        if (!lpHook->IsMenu())
        {
            // A running hook with a stronger (numerically lower) priority keeps the screen.
            if (lpCurrent->IsActive() && lpHook->miPriority > lpCurrent->GetHook()->miPriority)
            {
                return;
            }
            mpOutgoing = lpCurrent;
            mpCurrent  = (lpCurrent == &mBlender1) ? &mBlender2 : &mBlender1;   // the swap @0x8250B6F8
            if (lbActive)
            {
                const u32 luMode = lpHook->meTransitionMode;
                if (luMode == 1)
                {
                    mfCrossFadeTimeExpired = 0.0f;
                    mfCrossFadeTimeToEnd   = lpHook->mfTransitionTime;
                    meHookState            = E_CROSSFADE;
                    mpCurrent->Initialise(lpHook, lpBundle, maColourCubes, muColourCubeCount,
                                          lpGuiPFXHookEvent->mfMaximumBloomWeight,
                                          lpGuiPFXHookEvent->mfMaximumVignetteWeight,
                                          lpGuiPFXHookEvent->mfMaximumBlurWeight,
                                          lpGuiPFXHookEvent->mfMaximumDepthOfFieldWeight,
                                          lpGuiPFXHookEvent->mfMaximum2DTintWeight,
                                          lpGuiPFXHookEvent->mfMaximum3DTintWeight);
                    return;
                }
                CGS_ASSERT(luMode == 0, "Bad transition mode in Hook");            // :669
            }
            meHookState = E_NORMAL;
            mpCurrent->Initialise(lpHook, lpBundle, maColourCubes, muColourCubeCount,
                                  lpGuiPFXHookEvent->mfMaximumBloomWeight,
                                  lpGuiPFXHookEvent->mfMaximumVignetteWeight,
                                  lpGuiPFXHookEvent->mfMaximumBlurWeight,
                                  lpGuiPFXHookEvent->mfMaximumDepthOfFieldWeight,
                                  lpGuiPFXHookEvent->mfMaximum2DTintWeight,
                                  lpGuiPFXHookEvent->mfMaximum3DTintWeight);
            return;
        }

        // ---- a MENU hook ------------------------------------------------------------------
        mpMenu->Initialise(lpHook, lpBundle, maColourCubes, muColourCubeCount,
                           lpGuiPFXHookEvent->mfMaximumBloomWeight,
                           lpGuiPFXHookEvent->mfMaximumVignetteWeight,
                           lpGuiPFXHookEvent->mfMaximumBlurWeight,
                           lpGuiPFXHookEvent->mfMaximumDepthOfFieldWeight,
                           lpGuiPFXHookEvent->mfMaximum2DTintWeight,
                           lpGuiPFXHookEvent->mfMaximum3DTintWeight);
        mfMenuPhaseTimeExpired = 0.0f;
        const u32 luMode = lpHook->meTransitionMode;
        if (luMode == 1)
        {
            f32 lfTime = lpHook->mfTransitionTime;
            if (lfTime < 0.0099999998f && lfTime > -0.0099999998f)
            {
                CGS_ASSERT(lpHook->miNodeCount > 0, "lpHook->mpaNodes[0]");         // :620
                const PFXHookNode* lpNode  = lpBundle->GetHookNode(*lpHook, 0);
                const PFXGroup*    lpGroup = lpBundle->GetNodeGroup(*lpNode);
                CGS_ASSERT(lpGroup != 0, "lpHook->mpaNodes[0]->mpGroup");           // :621
                lfTime = lpGroup->mfFadeIn;
            }
            const EHookState leState = meHookState;
            mfMenuPhaseTimeToEnd = lfTime;
            if (leState != E_FADE_TO_MENU && leState != E_FULL_MENU && leState != E_FADE_FROM_MENU)
            {
                meStateBeforeMenu = leState;
            }
            meHookState = E_FADE_TO_MENU;
        }
        else if (luMode == 0)
        {
            const EHookState leState = meHookState;
            if (leState != E_FADE_TO_MENU && leState != E_FULL_MENU && leState != E_FADE_FROM_MENU)
            {
                meStateBeforeMenu = leState;
            }
            meHookState = E_FULL_MENU;
        }
    }

    // ------------------------------------------------------------------------------------
    // StopHook @0x824FB398 (:702)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::StopHook(const PFXHook* lpHook)
    {
        CGS_ASSERT(lpHook != 0, "lpHook != NULL");                                  // :702
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");                          // :703
        if (mpCurrent->IsActive() && mpCurrent->GetHook() == lpHook)
        {
            mpCurrent->StartFadeOut(0.0f);
        }
    }

    // ------------------------------------------------------------------------------------
    // StopMenuHook @0x824FB478 (:727)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::StopMenuHook()
    {
        if (meHookState != E_FADE_TO_MENU && meHookState != E_FULL_MENU)
        {
            return;
        }
        CGS_ASSERT(mpMenu->IsActive(), "mpMenu->IsActive()");                       // :727
        if (mpMenu->GetHook()->meTransitionMode == 1)
        {
            mpMenu->StartFadeOut(0.0f);
            mfMenuPhaseTimeExpired = 0.0f;
            meHookState            = E_FADE_FROM_MENU;
            const PFXHookBundle* lpBundle = mpHookInfo.operator->();
            mfMenuPhaseTimeToEnd = lpBundle->GetNodeGroup(*lpBundle->GetHookNode(*mpMenu->GetHook(), 0))->mfFadeOut;
        }
        else
        {
            mpMenu->StartFadeOut(0.0f);
            meHookState = E_FULL_MENU;
        }
    }

    // ------------------------------------------------------------------------------------
    // StartBackgroundHook @0x8250B1F0 (:392) / StopBackgroundHook @0x8250B478 (:443)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::StartBackgroundHook(const GuiPFXStartBackgroundHookEvent* lpEvent)
    {
        const PFXHook* lpHook = ResolveHook(lpEvent->muGuid, lpEvent->macName, 0);
        CGS_ASSERT(lpHook != 0, "EffectsArbitrator::EventUpdate: Effect name not found in bundle");   // :392
        if (lpHook == 0)
        {
            return;
        }
        const PFXHookBundle* lpBundle  = mpHookInfo.operator->();
        PFXHookNodeBlender*  lpCurrent = mpBackgroundCurrent;
        if (lpCurrent->IsActive())
        {
            mpBackgroundOutgoing = lpCurrent;
            mpBackgroundCurrent  = (lpCurrent == &mBackgroundBlender1) ? &mBackgroundBlender2 : &mBackgroundBlender1;
            if (lpHook->meTransitionMode == 1)
            {
                f32 lfTime = lpHook->mfTransitionTime;
                if (lfTime < 0.0099999998f && lfTime > -0.0099999998f)
                {
                    CGS_ASSERT(lpHook->miNodeCount > 0, "lpHook->mpaNodes[0]");     // :404
                    const PFXHookNode* lpNode  = lpBundle->GetHookNode(*lpHook, 0);
                    const PFXGroup*    lpGroup = lpBundle->GetNodeGroup(*lpNode);
                    CGS_ASSERT(lpGroup != 0, "lpHook->mpaNodes[0]->mpGroup");       // :405
                    lfTime = lpGroup->mfFadeIn;
                }
                mfBackgroundFadeTimeToEnd = lfTime;
            }
            mfBackgroundFadeTimeExpired = 0.0f;
            meBackgroundHookState       = E_BACKGROUND_CROSSFADING;
        }
        else
        {
            meBackgroundHookState = E_BACKGROUND_RUNNING;
        }
        const f32 lfWeight = lpEvent->mfMaximumWeight;
        mpBackgroundCurrent->Initialise(lpHook, lpBundle, maColourCubes, muColourCubeCount,
                                        lfWeight, lfWeight, lfWeight, lfWeight, lfWeight, lfWeight);
    }

    void EffectsArbitrator::StopBackgroundHook(const GuiPFXStopBackgroundHookEvent* lpEvent)
    {
        const PFXHook* lpHook = ResolveHook(lpEvent->muGuid, lpEvent->macName, 0);
        CGS_ASSERT(lpHook != 0, "EffectsArbitrator::EventUpdate: Effect name not found in bundle");   // :443
        if (mpBackgroundCurrent->IsActive() && mpBackgroundCurrent->GetHook() == lpHook)
        {
            mpBackgroundCurrent->StartFadeOut(0.0f);
        }
    }

    // ------------------------------------------------------------------------------------
    // UpdateHooks @0x82502E38
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::UpdateHooks()
    {
        const f32 lfTimeNow = mpGuiCache->GetTime();
        CGS_ASSERT(lfTimeNow != -FLT_MAX, "mfTimeNow!=-FLT_MAX");                    // CgsGuiEventTypeDefs.h:250
        const f32 lfTimestep = lfTimeNow - mfLastTime;
        mfLastTime = lfTimeNow;
        if (PfxDiag())
        {
            static u32 suUpdateDiagCalls = 0;
            if (mpCurrent != 0 && mpCurrent->IsActive() && (suUpdateDiagCalls++ % 60u) == 0)
            {
                *CgsDev::Log::gpDebugPrint << "[pfx] UpdateHooks state " << static_cast<s32>(meHookState)
                                           << " current " << (mpCurrent->GetHook() != 0 ? mpCurrent->GetHook()->macName : "-")
                                           << " dt " << lfTimestep << " now " << lfTimeNow
                                           << " hookTime " << mpCurrent->GetTime() << "\n";
            }
        }

        if (mbForceTime)
        {
            mpCurrent->ForceTime(mfForcedTime);
        }

        if (meHookState <= E_CROSSFADE)
        {
            const EBackgroundHookState leBackground = meBackgroundHookState;
            if (leBackground == E_BACKGROUND_RUNNING)
            {
                mpBackgroundCurrent->Update(lfTimestep);
                if (!mpBackgroundCurrent->IsActive())
                {
                    meBackgroundHookState = E_BACKGROUND_INACTIVE;
                }
            }
            else if (leBackground == E_BACKGROUND_CROSSFADING)
            {
                mfBackgroundFadeTimeExpired += lfTimestep;
                mpBackgroundCurrent->Update(lfTimestep);
                if (mfBackgroundFadeTimeExpired >= mfBackgroundFadeTimeToEnd)
                {
                    meBackgroundHookState = E_BACKGROUND_RUNNING;
                }
            }
        }

        switch (meHookState)
        {
        case E_NORMAL:
            if (mpCurrent->IsActive())
            {
                mpCurrent->Update(lfTimestep);
            }
            break;
        case E_CROSSFADE:
            mfCrossFadeTimeExpired += lfTimestep;
            mpCurrent->Update(lfTimestep);
            if (mfCrossFadeTimeExpired >= mfCrossFadeTimeToEnd)
            {
                meHookState = E_NORMAL;
            }
            break;
        case E_FADE_TO_MENU:
            mfMenuPhaseTimeExpired += lfTimestep;
            mpMenu->Update(lfTimestep);
            if (mfMenuPhaseTimeExpired >= mfMenuPhaseTimeToEnd)
            {
                mfMenuPhaseTimeExpired = 0.0f;
                meHookState            = E_FULL_MENU;
                mfMenuPhaseTimeToEnd   = 0.0f;
            }
            break;
        case E_FADE_FROM_MENU:
            mfMenuPhaseTimeExpired += lfTimestep;
            // fall through -- the console's LABEL_20
        case E_FULL_MENU:
            mpMenu->Update(lfTimestep);
            if (!mpMenu->IsActive())
            {
                meHookState = meStateBeforeMenu;
            }
            break;
        default:
            return;
        }
    }

    // ------------------------------------------------------------------------------------
    // EventUpdate @0x825125C0 (:256)
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::EventUpdate(BrnUpdateSet leUpdateSet,
                                        const CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                        CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue)
    {
        CGS_ASSERT(lpGuiInputBuffer != 0, "lpGuiInputBuffer != NULL");              // :256
        CGS_ASSERT(lpGuiOutQueue != 0, "lpGuiOutputBuffer != NULL");                // :257

        const CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpEventQueue = lpGuiInputBuffer->GetGuiEvents();
        CGS_ASSERT(lpEventQueue != 0, "lpEventQueue != NULL");                      // :265

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpEventQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            switch (liId)
            {
            case 64:
                // GuiEventCache: the GuiCache pointer, latched once.
                if (mpGuiCache == 0)
                {
                    mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                      // :280
                }
                break;
            case 495:
                CGS_ASSERT(lpEvent != 0, "Invalid hook event message");             // :288
                StartHook(PayloadAs<GuiPFXHookEvent>(lpEvent));
                break;
            case 496:
            {
                CGS_ASSERT(lpEvent != 0, "lpGuiPFXHookEvent != NULL");              // :297
                const GuiPFXHookStopEvent* lpStop = PayloadAs<GuiPFXHookStopEvent>(lpEvent);
                const PFXHook* lpHook = ResolveHook(lpStop->muGuid, lpStop->macName, "EventUpdate");
                if (lpHook != 0)
                {
                    if (PfxDiag())
                    {
                        *CgsDev::Log::gpDebugPrint << "[pfx] StopHook '" << lpHook->macName << "'\n";
                    }
                    if (!lpHook->IsMenu())
                    {
                        StopHook(lpHook);
                    }
                    StopMenuHook();
                }
                break;
            }
            case 497:
                StopMenuHook();
                break;
            case 498:
                CGS_ASSERT(lpEvent != 0, "Invalid start background hook event.");   // :334
                StartBackgroundHook(PayloadAs<GuiPFXStartBackgroundHookEvent>(lpEvent));
                break;
            case 499:
                CGS_ASSERT(lpEvent != 0, "Invalid stop background hook event.");    // :343
                StopBackgroundHook(PayloadAs<GuiPFXStopBackgroundHookEvent>(lpEvent));
                break;
            case 500:
            {
                GuiPFXHookEnumeration lEnumeration;
                lEnumeration.miHookNameCount = -1;
                if (!mpHookInfo.IsEqual(&CgsResource::NULLResourcePtr))
                {
                    const PFXHookBundle* lpBundle = mpHookInfo.operator->();
                    lEnumeration.miHookNameCount = lpBundle->miHookCount;
                    for (s32 liHook = 0; liHook < lpBundle->miHookCount && liHook < 100; ++liHook)
                    {
                        lEnumeration.mapHookNames[liHook] = lpBundle->GetHook(liHook)->macName;
                    }
                }
                if (PfxDiag())
                {
                    *CgsDev::Log::gpDebugPrint << "[pfx] enumeration answered: " << lEnumeration.miHookNameCount << " hooks\n";
                }
                lpGuiOutQueue->AddEvent(&lEnumeration, 501, static_cast<s32>(sizeof(lEnumeration)));   // AddGuiOutEvent<GuiPFXHookEnumeration>
                break;
            }
            default:
                break;
            }
            liId = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        if (mpGuiCache != 0 && (leUpdateSet & 8) != 0)
        {
            UpdateHooks();
        }
    }

    // ------------------------------------------------------------------------------------
    // LookupColourCube @0x824F5B98 (:1214)
    // ------------------------------------------------------------------------------------
    ColourCubeInfo* EffectsArbitrator::LookupColourCube(u64 luResourceId)
    {
        CGS_ASSERT(muColourCubeCount <= KU_MAX_NUM_COLOURCUBES, "muColourCubeCount <= KU_MAX_NUM_COLOURCUBES");   // :1214
        for (u32 luCube = 0; luCube < muColourCubeCount; ++luCube)
        {
            if (maColourCubes[luCube].muResourceId == luResourceId)
            {
                return &maColourCubes[luCube];
            }
        }
        return 0;
    }

    // ------------------------------------------------------------------------------------
    // GenerateEffectFrameEvents @0x82503060 (:860)
    //
    // Frame 0 (lpFrame1) carries the current hook, crossfaded against the outgoing one; frame
    // 1 (lpFrame2) carries the menu hook while a menu phase runs, else the background hook
    // (crossfaded against the outgoing background one), weighted by what frame 0 left over.
    // Each of the six effect types walks the same three arms; the 3D tint is a pointer, not a
    // blend, so it is set outright and its two "frame has a cube" flags clear the slots left
    // unwritten at the end.
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::GenerateEffectFrameEvents(RendererIO::OutputBuffer* lpRenderOutput)
    {
        CGS_ASSERT(lpRenderOutput != 0, "lpRenderOutput != NULL");                  // :860
        GenerateEffectFrameEvents(lpRenderOutput->GetFXEventsEffectsFrame(0),
                                  lpRenderOutput->GetFXEventsEffectsFrame(1));
    }

    // The frame-pointer form (see the header): everything past the two frame reads is the
    // console body of @0x82503060 unchanged.
    void EffectsArbitrator::GenerateEffectFrameEvents(BrnEffectsFrame* lpFrame1, BrnEffectsFrame* lpFrame2)
    {
        if (PfxDiag())
        {
            static bool sbSaidFrames = false;
            if (!sbSaidFrames)
            {
                sbSaidFrames = true;
                *CgsDev::Log::gpDebugPrint << "[pfx] GenerateEffectFrameEvents live: frames "
                                           << (lpFrame1 != 0 ? 1 : 0) << " " << (lpFrame2 != 0 ? 1 : 0) << "\n";
            }
        }
        if (lpFrame1 == 0 || lpFrame2 == 0)
        {
            return;
        }

        BloomBlend        lBloom;        lBloom.Construct();
        VignetteBlend     lVignette;     lVignette.Construct();
        BlurBlend         lBlur;         lBlur.Construct();
        DepthOfFieldBlend lDepthOfField; lDepthOfField.Construct();
        TintData2dBlend   lTint2d;       lTint2d.Construct();
        bool lbFrame1HasTint = false;
        bool lbFrame2HasTint = false;

        const EHookState leState = meHookState;
        f32 lfFrame1Weight = 0.0f;
        f32 lfFrame2Weight = 0.0f;
        switch (leState)
        {
        case E_NORMAL:
        case E_CROSSFADE:
            lfFrame1Weight = 1.0f;
            lfFrame2Weight = 0.0f;
            break;
        case E_FADE_TO_MENU:
            lfFrame2Weight = mfMenuPhaseTimeExpired / mfMenuPhaseTimeToEnd;
            lfFrame1Weight = 1.0f - lfFrame2Weight;
            break;
        case E_FULL_MENU:
            lfFrame1Weight = 0.0f;
            lfFrame2Weight = 1.0f;
            break;
        case E_FADE_FROM_MENU:
            lfFrame1Weight = mfMenuPhaseTimeExpired / mfMenuPhaseTimeToEnd;
            lfFrame2Weight = 1.0f - lfFrame1Weight;
            break;
        case E_PAUSED:
            lfFrame1Weight = 0.0f;
            lfFrame2Weight = 0.0f;
            break;
        default:
            break;
        }

        // ---- frame 1: the current hook, crossfaded against the outgoing one ---------------
        f32 lfFrame1TintWeight = 0.0f;
        if (leState != E_FULL_MENU && mpCurrent->IsActive())
        {
            const PFXHookNodeBlender& lrCurrent  = *mpCurrent;
            const PFXHookNodeBlender& lrOutgoing = *mpOutgoing;
            const f32 lfCrossFade = mfCrossFadeTimeExpired / mfCrossFadeTimeToEnd;
            const bool lbCrossFading = (meHookState == E_CROSSFADE);

            if (lrCurrent.GetBloom().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.GetBloom().GetCount() > 0.0f)
                {
                    lBloom.Add(&lrCurrent.GetBloom().GetData(), lfCrossFade * lrCurrent.GetBloom().GetWeight());
                    lBloom.Add(&lrOutgoing.GetBloom().GetData(), (1.0f - lfCrossFade) * lrOutgoing.GetBloom().GetWeight());
                }
                else
                {
                    lBloom.Add(&lrCurrent.GetBloom().GetData(), lrCurrent.GetBloom().GetWeight());
                }
            }
            if (lrCurrent.GetVignette().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.GetVignette().GetCount() > 0.0f)
                {
                    lVignette.Add(&lrCurrent.GetVignette().GetData(), lfCrossFade * lrCurrent.GetVignette().GetWeight());
                    lVignette.Add(&lrOutgoing.GetVignette().GetData(), (1.0f - lfCrossFade) * lrOutgoing.GetVignette().GetWeight());
                }
                else
                {
                    lVignette.Add(&lrCurrent.GetVignette().GetData(), lrCurrent.GetVignette().GetWeight());
                }
            }
            if (lrCurrent.GetBlur().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.GetBlur().GetCount() > 0.0f)
                {
                    lBlur.Add(&lrCurrent.GetBlur().GetData(), lfCrossFade * lrCurrent.GetBlur().GetWeight());
                    lBlur.Add(&lrOutgoing.GetBlur().GetData(), (1.0f - lfCrossFade) * lrOutgoing.GetBlur().GetWeight());
                }
                else
                {
                    lBlur.Add(&lrCurrent.GetBlur().GetData(), lrCurrent.GetBlur().GetWeight());
                }
            }
            if (lrCurrent.GetDepthOfField().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.GetDepthOfField().GetCount() > 0.0f)
                {
                    lDepthOfField.Add(&lrCurrent.GetDepthOfField(), lfCrossFade * lrCurrent.GetDepthOfField().GetWeight());
                    lDepthOfField.Add(&lrOutgoing.GetDepthOfField(), (1.0f - lfCrossFade) * lrOutgoing.GetDepthOfField().GetWeight());
                }
                else
                {
                    lDepthOfField.Add(&lrCurrent.GetDepthOfField(), lrCurrent.GetDepthOfField().GetWeight());
                }
            }
            if (lrCurrent.Get2DTint().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.Get2DTint().GetCount() > 0.0f)
                {
                    lTint2d.Add(&lrCurrent.Get2DTint().GetData(), lfCrossFade * lrCurrent.Get2DTint().GetWeight());
                    lTint2d.Add(&lrOutgoing.Get2DTint().GetData(), (1.0f - lfCrossFade) * lrOutgoing.Get2DTint().GetWeight());
                }
                else
                {
                    lTint2d.Add(&lrCurrent.Get2DTint().GetData(), lrCurrent.Get2DTint().GetWeight());
                }
            }
            if (lrCurrent.Get3DTint().GetCount() > 0.0f)
            {
                if (lbCrossFading && lrOutgoing.Get3DTint().GetCount() > 0.0f)
                {
                    lbFrame1HasTint    = true;
                    lfFrame1TintWeight = lrCurrent.Get3DTint().GetWeight() * lfCrossFade;
                    lpFrame1->SetTintData(lrCurrent.Get3DTint().GetData(), lfFrame1TintWeight);
                    lbFrame2HasTint = true;
                    lpFrame2->SetTintData(lrOutgoing.Get3DTint().GetData(),
                                          (1.0f - lfCrossFade) * lrOutgoing.Get3DTint().GetWeight());
                }
                else
                {
                    lbFrame1HasTint    = true;
                    lfFrame1TintWeight = lrCurrent.Get3DTint().GetWeight();
                    lpFrame1->SetTintData(lrCurrent.Get3DTint().GetData(), lfFrame1TintWeight);
                }
            }
        }

        // ---- LABEL_48: publish frame 1 -------------------------------------------------------
        const f32 lfFrame1Bloom        = lBloom.GetWeight() * lfFrame1Weight;
        const f32 lfFrame1Vignette     = lVignette.GetWeight() * lfFrame1Weight;
        const f32 lfFrame1Blur         = lBlur.GetWeight() * lfFrame1Weight;
        const f32 lfFrame1DepthOfField = lDepthOfField.GetWeight() * lfFrame1Weight;
        const f32 lfFrame1Tint2d       = lTint2d.GetWeight() * lfFrame1Weight;
        lpFrame1->SetBloomData(lBloom.GetData(), lfFrame1Bloom);
        lpFrame1->SetVignetteData(lVignette.GetData(), lfFrame1Vignette);
        lpFrame1->SetBlurData(lBlur.GetData(), lfFrame1Blur);
        {
            BrnEffects::DepthOfFieldData lDof;
            lDof.mfNearPlane   = lDepthOfField.mfNearPlane;
            lDof.mfFocalPlane  = lDepthOfField.mfFocalPlane;
            lDof.mfFocalPlane2 = lDepthOfField.mfFocalPlane2;
            lDof.mfFarPlane    = lDepthOfField.mfFarPlane;
            lDof.mfDofAmount   = lDepthOfField.mfDofAmount;
            lpFrame1->SetDepthOfFieldData(lDof, lfFrame1DepthOfField);
        }
        lpFrame1->SetTintData2d(lTint2d.GetData(), lfFrame1Tint2d);
        if (PfxDiag())
        {
            static u32 suFrameDiagCalls = 0;
            if (mpCurrent != 0 && mpCurrent->IsActive() && (suFrameDiagCalls++ % 30u) == 0)
            {
                *CgsDev::Log::gpDebugPrint << "[pfx] frame state " << static_cast<s32>(leState)
                                           << " f1w " << lfFrame1Weight << " f2w " << lfFrame2Weight
                                           << " bloom " << lfFrame1Bloom << " vig " << lfFrame1Vignette
                                           << " blur " << lfFrame1Blur << " dof " << lfFrame1DepthOfField
                                           << " tint2d " << lfFrame1Tint2d << " tint3d " << lfFrame1TintWeight
                                           << " t " << mpCurrent->GetTime() << "\n";
                const BrnEffects::VignetteData& lrV = lVignette.GetData();
                *CgsDev::Log::gpDebugPrint << "[pfx]   vignette amount " << lrV.mv2Amount.x << "," << lrV.mv2Amount.y
                                           << " centre " << lrV.mv2Centre.x << "," << lrV.mv2Centre.y
                                           << " inner " << lrV.mv4InnerColour.x << "," << lrV.mv4InnerColour.y << "," << lrV.mv4InnerColour.z << "," << lrV.mv4InnerColour.w
                                           << " outer " << lrV.mv4OuterColour.x << "," << lrV.mv4OuterColour.y << "," << lrV.mv4OuterColour.z << "," << lrV.mv4OuterColour.w
                                           << " angle " << lrV.mfAngle << " sharp " << lrV.mfSharpness
                                           << " cube " << (mpCurrent->Get3DTint().GetData().mpColourCube != 0 ? 1 : 0) << "\n";
            }
        }

        // ---- frame 2: the menu hook, else the background pair ---------------------------------
        const bool lbMenuPhase = (leState == E_FADE_TO_MENU || leState == E_FULL_MENU || leState == E_FADE_FROM_MENU);
        const PFXHookNodeBlender& lrMenu   = *mpMenu;
        const PFXHookNodeBlender& lrBgCur  = *mpBackgroundCurrent;
        const PFXHookNodeBlender& lrBgOut  = *mpBackgroundOutgoing;
        const EBackgroundHookState leBackground = meBackgroundHookState;
        const bool lbBackgroundCrossFading = (leBackground == E_BACKGROUND_CROSSFADING);
        const f32  lfBackgroundFade = lbBackgroundCrossFading
                                    ? mfBackgroundFadeTimeExpired / mfBackgroundFadeTimeToEnd : 0.0f;

        // bloom
        if (lbMenuPhase && lrMenu.GetBloom().GetCount() > 0.0f)
        {
            lpFrame2->SetBloomData(lrMenu.GetBloom().GetData(), lrMenu.GetBloom().GetWeight() * lfFrame2Weight);
        }
        else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.GetBloom().GetCount() > 0.0f)
        {
            if (lbBackgroundCrossFading && lrBgOut.GetBloom().GetCount() > 0.0f)
            {
                lBloom.Construct();
                lBloom.Add(&lrBgCur.GetBloom().GetData(), lfBackgroundFade * lrBgCur.GetBloom().GetWeight());
                lBloom.Add(&lrBgOut.GetBloom().GetData(), (1.0f - lfBackgroundFade) * lrBgOut.GetBloom().GetWeight());
                lpFrame2->SetBloomData(lBloom.GetData(), (1.0f - lfFrame1Bloom) * lBloom.GetWeight());
            }
            else
            {
                lpFrame2->SetBloomData(lrBgCur.GetBloom().GetData(), (1.0f - lfFrame1Bloom) * lrBgCur.GetBloom().GetWeight());
            }
        }
        else
        {
            BrnEffects::BloomData lDefault; lDefault.Construct();
            lpFrame2->SetBloomData(lDefault, 0.0f);
        }
        // vignette
        if (lbMenuPhase && lrMenu.GetVignette().GetCount() > 0.0f)
        {
            lpFrame2->SetVignetteData(lrMenu.GetVignette().GetData(), lrMenu.GetVignette().GetWeight() * lfFrame2Weight);
        }
        else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.GetVignette().GetCount() > 0.0f)
        {
            if (lbBackgroundCrossFading && lrBgOut.GetVignette().GetCount() > 0.0f)
            {
                lVignette.Construct();
                lVignette.Add(&lrBgCur.GetVignette().GetData(), lfBackgroundFade * lrBgCur.GetVignette().GetWeight());
                lVignette.Add(&lrBgOut.GetVignette().GetData(), (1.0f - lfBackgroundFade) * lrBgOut.GetVignette().GetWeight());
                lpFrame2->SetVignetteData(lVignette.GetData(), (1.0f - lfFrame1Vignette) * lVignette.GetWeight());
            }
            else
            {
                lpFrame2->SetVignetteData(lrBgCur.GetVignette().GetData(), (1.0f - lfFrame1Vignette) * lrBgCur.GetVignette().GetWeight());
            }
        }
        else
        {
            BrnEffects::VignetteData lDefault; lDefault.Construct();
            lpFrame2->SetVignetteData(lDefault, 0.0f);
        }
        // blur
        if (lbMenuPhase && lrMenu.GetBlur().GetCount() > 0.0f)
        {
            lpFrame2->SetBlurData(lrMenu.GetBlur().GetData(), lrMenu.GetBlur().GetWeight() * lfFrame2Weight);
        }
        else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.GetBlur().GetCount() > 0.0f)
        {
            if (lbBackgroundCrossFading && lrBgOut.GetBlur().GetCount() > 0.0f)
            {
                lBlur.Construct();
                lBlur.Add(&lrBgCur.GetBlur().GetData(), lfBackgroundFade * lrBgCur.GetBlur().GetWeight());
                lBlur.Add(&lrBgOut.GetBlur().GetData(), (1.0f - lfBackgroundFade) * lrBgOut.GetBlur().GetWeight());
                lpFrame2->SetBlurData(lBlur.GetData(), (1.0f - lfFrame1Blur) * lBlur.GetWeight());
            }
            else
            {
                lpFrame2->SetBlurData(lrBgCur.GetBlur().GetData(), (1.0f - lfFrame1Blur) * lrBgCur.GetBlur().GetWeight());
            }
        }
        else
        {
            BrnEffects::BlurData lDefault; lDefault.Construct();
            lpFrame2->SetBlurData(lDefault, 0.0f);
        }
        // depth of field
        {
            DepthOfFieldBlend lFrame2Dof; lFrame2Dof.Construct();
            f32 lfWeight = 0.0f;
            if (lbMenuPhase && lrMenu.GetDepthOfField().GetCount() > 0.0f)
            {
                lFrame2Dof = lrMenu.GetDepthOfField();
                lfWeight   = lrMenu.GetDepthOfField().GetWeight() * lfFrame2Weight;
            }
            else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.GetDepthOfField().GetCount() > 0.0f)
            {
                if (lbBackgroundCrossFading && lrBgOut.GetDepthOfField().GetCount() > 0.0f)
                {
                    lFrame2Dof.Add(&lrBgCur.GetDepthOfField(), lfBackgroundFade * lrBgCur.GetDepthOfField().GetWeight());
                    lFrame2Dof.Add(&lrBgOut.GetDepthOfField(), (1.0f - lfBackgroundFade) * lrBgOut.GetDepthOfField().GetWeight());
                    lfWeight = (1.0f - lfFrame1DepthOfField) * lFrame2Dof.GetWeight();
                }
                else
                {
                    lFrame2Dof = lrBgCur.GetDepthOfField();
                    lfWeight   = (1.0f - lfFrame1DepthOfField) * lrBgCur.GetDepthOfField().GetWeight();
                }
            }
            BrnEffects::DepthOfFieldData lDof;
            lDof.mfNearPlane   = lFrame2Dof.mfNearPlane;
            lDof.mfFocalPlane  = lFrame2Dof.mfFocalPlane;
            lDof.mfFocalPlane2 = lFrame2Dof.mfFocalPlane2;
            lDof.mfFarPlane    = lFrame2Dof.mfFarPlane;
            lDof.mfDofAmount   = lFrame2Dof.mfDofAmount;
            lpFrame2->SetDepthOfFieldData(lDof, lfWeight);
        }
        // 2D tint
        if (lbMenuPhase && lrMenu.Get2DTint().GetCount() > 0.0f)
        {
            CGS_ASSERT(lrMenu.Get2DTint().GetWeight() <= 1.0f, "mpMenu->Get2DTint().GetWeight() <= 1.0f");   // :1138
            CGS_ASSERT(lfFrame2Weight <= 1.0f, "lfFrame2Weight <= 1.0f");                                    // :1139
            lpFrame2->SetTintData2d(lrMenu.Get2DTint().GetData(), lrMenu.Get2DTint().GetWeight() * lfFrame2Weight);
        }
        else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.Get2DTint().GetCount() > 0.0f)
        {
            if (lbBackgroundCrossFading && lrBgOut.Get2DTint().GetCount() > 0.0f)
            {
                lTint2d.Construct();
                lTint2d.Add(&lrBgCur.Get2DTint().GetData(), lfBackgroundFade * lrBgCur.Get2DTint().GetWeight());
                lTint2d.Add(&lrBgOut.Get2DTint().GetData(), (1.0f - lfBackgroundFade) * lrBgOut.Get2DTint().GetWeight());
                lpFrame2->SetTintData2d(lTint2d.GetData(), (1.0f - lfFrame1Tint2d) * lTint2d.GetWeight());
            }
            else
            {
                lpFrame2->SetTintData2d(lrBgCur.Get2DTint().GetData(), (1.0f - lfFrame1Tint2d) * lrBgCur.Get2DTint().GetWeight());
            }
        }
        else
        {
            BrnEffects::TintData2d lDefault; lDefault.Construct();
            lpFrame2->SetTintData2d(lDefault, 0.0f);
        }
        // 3D tint
        if (lbMenuPhase && lrMenu.Get3DTint().GetCount() > 0.0f)
        {
            lbFrame2HasTint = true;
            lpFrame2->SetTintData(lrMenu.Get3DTint().GetData(), lrMenu.Get3DTint().GetWeight() * lfFrame2Weight);
        }
        else if (leBackground != E_BACKGROUND_INACTIVE && lrBgCur.Get3DTint().GetCount() > 0.0f)
        {
            if (lbBackgroundCrossFading && lrBgOut.Get3DTint().GetCount() > 0.0f && !lbFrame1HasTint)
            {
                // The outgoing background cube rides frame 1's free tint slot.
                lbFrame1HasTint = true;
                lpFrame1->SetTintData(lrBgOut.Get3DTint().GetData(), (1.0f - lfBackgroundFade) * lrBgOut.Get3DTint().GetWeight());
                lbFrame2HasTint = true;
                lpFrame2->SetTintData(lrBgCur.Get3DTint().GetData(), lfBackgroundFade * lrBgCur.Get3DTint().GetWeight());
            }
            else
            {
                lbFrame2HasTint = true;
                lpFrame2->SetTintData(lrBgCur.Get3DTint().GetData(), (1.0f - lfFrame1TintWeight) * lrBgCur.Get3DTint().GetWeight());
            }
        }
        if (!lbFrame1HasTint)
        {
            BrnEffects::TintData lNone; lNone.Construct();
            lpFrame1->SetTintData(lNone, 0.0f);
        }
        if (!lbFrame2HasTint)
        {
            BrnEffects::TintData lNone; lNone.Construct();
            lpFrame2->SetTintData(lNone, 0.0f);
        }
    }

    // ------------------------------------------------------------------------------------
    // The debug-menu callbacks: the selected list entry's name into a synthetic event, every
    // maximum at the "Background Weight" slider, then the real entry points.
    // ------------------------------------------------------------------------------------
    void EffectsArbitrator::Debug_StartHook(void* lpThis)
    {
        EffectsArbitrator* lpArbitrator = static_cast<EffectsArbitrator*>(lpThis);
        GuiPFXHookEvent lEvent;
        lEvent.muGuid = 0;
        CgsCore::SPrintf(lEvent.macName, sizeof(lEvent.macName), "%s",
                         lpArbitrator->maPfxHookList[lpArbitrator->miPfxHookIndex].mpcName);
        const f32 lfWeight = lpArbitrator->mfBackgroundWeight;
        lEvent.mfMaximumBloomWeight        = lfWeight;
        lEvent.mfMaximumVignetteWeight     = lfWeight;
        lEvent.mfMaximumBlurWeight         = lfWeight;
        lEvent.mfMaximumDepthOfFieldWeight = lfWeight;
        lEvent.mfMaximum2DTintWeight       = lfWeight;
        lEvent.mfMaximum3DTintWeight       = lfWeight;
        lpArbitrator->StartHook(&lEvent);
    }

    void EffectsArbitrator::Debug_StartBackgroundHook(void* lpThis)
    {
        EffectsArbitrator* lpArbitrator = static_cast<EffectsArbitrator*>(lpThis);
        GuiPFXStartBackgroundHookEvent lEvent;
        lEvent.muGuid = 0;
        CgsCore::SPrintf(lEvent.macName, sizeof(lEvent.macName), "%s",
                         lpArbitrator->maPfxHookList[lpArbitrator->miPfxHookIndex].mpcName);
        lEvent.mfMaximumWeight = lpArbitrator->mfBackgroundWeight;
        lpArbitrator->StartBackgroundHook(&lEvent);
    }

    void EffectsArbitrator::Debug_StopHook(void* lpThis)
    {
        EffectsArbitrator* lpArbitrator = static_cast<EffectsArbitrator*>(lpThis);
        if (lpArbitrator->mpCurrent->IsActive())
        {
            lpArbitrator->mpCurrent->StartFadeOut(0.0f);
        }
    }

    void EffectsArbitrator::Debug_StopBackgroundHook(void* lpThis)
    {
        EffectsArbitrator* lpArbitrator = static_cast<EffectsArbitrator*>(lpThis);
        if (lpArbitrator->mpBackgroundCurrent->IsActive())
        {
            lpArbitrator->mpBackgroundCurrent->StartFadeOut(0.0f);
        }
    }
}
