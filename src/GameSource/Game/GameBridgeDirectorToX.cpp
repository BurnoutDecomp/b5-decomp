// ============================================================================
// GameBridgeDirectorToX.cpp -- BrnGame::BrnGameModule::BridgeDirectorToGui
//
// X360 BrnGameModule::BridgeDirectorToGui @0x823DD5C0 (its own asserts name this file,
// GameBridgeDirectorToX.cpp:100 / :101). Runs inside DoUpdate_GUI under the GUI-input
// write bracket with the director output read-locked, and turns the director's per-frame
// requests into GUI events:
//
//   out+0x750 (request hook enumeration)              -> 500 GuiEvent<500>
//   CameraOutput.mEffects.mbHasStopHookNameString     -> 496 GuiPFXHookStopEvent
//   CameraOutput.mEffects.mbHasStartHookNameString    -> 495 GuiPFXHookEvent (six maxima = the blend)
//   CameraOutput.mEffects.mBackgroundEffectRequest    -> 498 start (pending, no stop) / 499 stop
//   CameraOutput.mEffects.muRequestedPostFxId != 0    -> 495 by GUID (maxima 1.0, empty name)
//
// The console function also posts GuiEventTogglePictureParadise (camera state flag 14
// edges + TrainingManager::OnTogglePictureParadise), GuiEventPreraceTrigger and events
// 296 / 297 (director output interface bytes), GuiEventSetBlackBars (camera+272 while
// state bit 1 is set, else 1.0), GuiEventDirectorSettings (out+0x751) and hands the
// CgsCamera to the GUI input (InputBuffer::SetCamera). [FLAG] those legs are not
// transcribed here -- none of their event types has a home on this build yet.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameShared/GameClasses/Gui/CgsGuiModule.h"                              // CgsGui::GuiModule::AddGuiEvent (the static by-reference publisher)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                               // CgsGui::GuiEvent<500>
#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CGS_ASSERT
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"  // BrnDirector::DirectorIO::OutputBuffer
#include "GameSource/Director/Camera/Camera.h"                                    // BrnDirector::Camera::Camera / CameraEffects
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                   // BrnDirector::BackgroundEffectRequest
#include "GameSource/Gui/Events/BrnGuiPFXEvents.h"                                // the 495..499 records

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // [diag] CgsDev::Log::gpDebugPrint
#include <cstring>
#include <cstdlib>

// [diag] BRN_PFX_DIAG -- the director-side witness of the post-FX hook hand-over.
static bool PfxDirDiag()
{
    static const bool sbOn = (getenv("BRN_PFX_DIAG") != 0);
    return sbOn && CgsDev::Log::gpDebugPrint != 0;
}

namespace BrnGame
{
    namespace
    {
        // The console's `CgsCore::SPrintf(dst, 33, "%s", src)` into a HookNameString.
        void CopyHookName(BrnGui::HookNameString& lrDst, const char* lpcSrc)
        {
            std::strncpy(lrDst, lpcSrc, BrnGui::KI_MAX_PFX_ID_LENGTH);
            lrDst[BrnGui::KI_MAX_PFX_ID_LENGTH] = 0;
        }
    }

    void BrnGameModule::BridgeDirectorToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                            const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer)
    {
        CGS_ASSERT(lpGuiInputBuffer != 0,
                   "Invalid Gui Input buffer passed to BrnGameModule::BridgeDirectorToGui");        // :100
        CGS_ASSERT(lpDirectorOutputBuffer != 0,
                   "Invalid director output buffer passed to BrnGameModule::BridgeDirectorToGui");  // :101
        if (lpGuiInputBuffer == 0 || lpDirectorOutputBuffer == 0)
            return;

        const BrnDirector::Camera::Camera* lpCamera = lpDirectorOutputBuffer->GetCameraOutput();
        if (lpCamera == 0)
            return;   // [PC] before the director is prepared this build has no camera output.
        const BrnDirector::Camera::CameraEffects& lrEffects = lpCamera->GetEffects();

        // [FLAG] pseudocode 107..150: CopyToCgsCamera, the picture-paradise flag-14 edges
        // (GuiEventTogglePictureParadise + TrainingManager::OnTogglePictureParadise), the intro
        // rival-look trigger (GuiEventPreraceTrigger), output-interface bytes 12 / 13 (events
        // 296 / 297) and GuiEventSetBlackBars -- not transcribed (no PC home for the types).

        // pseudocode 151..152: the enumeration request -> 500.
        if (lpDirectorOutputBuffer->GetRequestHookEnumeration())
        {
            CgsGui::GuiEvent<500> lRequest;
            CgsGui::GuiModule::AddGuiEvent(lRequest, lpGuiInputBuffer);
            static bool sbSaid500 = false;
            if (PfxDirDiag() && !sbSaid500) { sbSaid500 = true; *CgsDev::Log::gpDebugPrint << "[pfx-dir] bridge posts 500 (enumeration request)\n"; }
        }

        // pseudocode 153..170: a stop request -> 496 (the console leaves the GUID unwritten;
        // EffectsArbitrator::EventUpdate resolves the name).
        if (lrEffects.mbHasStopHookNameString)
        {
            BrnGui::GuiPFXHookStopEvent lStop;
            std::memset(&lStop, 0, sizeof(lStop));
            lStop.muGuid = 0;
            CopyHookName(lStop.macName, lrEffects.GetStopHookNameString().mHookNameString);
            CgsGui::GuiModule::AddGuiEvent(lStop, lpGuiInputBuffer);
            if (PfxDirDiag()) *CgsDev::Log::gpDebugPrint << "[pfx-dir] bridge posts 496 stop '" << lStop.macName << "'\n";
        }

        // pseudocode 171..209: a start request -> 495, every per-type maximum = the blend.
        if (lrEffects.mbHasStartHookNameString)
        {
            const f32 lfBlend = lrEffects.GetStartHookNameBlendAmount();
            BrnGui::GuiPFXHookEvent lStart;
            std::memset(&lStart, 0, sizeof(lStart));
            lStart.muGuid                      = 0;
            lStart.mfMaximumBloomWeight        = lfBlend;
            lStart.mfMaximumVignetteWeight     = lfBlend;
            lStart.mfMaximumBlurWeight         = lfBlend;
            lStart.mfMaximumDepthOfFieldWeight = lfBlend;
            lStart.mfMaximum2DTintWeight       = lfBlend;
            lStart.mfMaximum3DTintWeight       = lfBlend;
            CopyHookName(lStart.macName, lrEffects.GetStartHookNameString().mHookNameString);
            CgsGui::GuiModule::AddGuiEvent(lStart, lpGuiInputBuffer);
            if (PfxDirDiag()) *CgsDev::Log::gpDebugPrint << "[pfx-dir] bridge posts 495 start '" << lStart.macName << "' blend " << lfBlend << "\n";
        }

        // pseudocode 210..230: the background request -> 498 (start pending, no stop) /
        // 499 (start pending AND stop).
        const BrnDirector::BackgroundEffectRequest& lrBackground = lrEffects.GetBackgroundEffectRequest();
        if (lrBackground.mbStartRequested && !lrBackground.mbStopRequest)
        {
            BrnGui::GuiPFXStartBackgroundHookEvent lStartBackground;
            std::memset(&lStartBackground, 0, sizeof(lStartBackground));
            lStartBackground.muGuid          = 0;
            lStartBackground.mfMaximumWeight = lrBackground.GetBackgroundStartRequestBlendAmount();
            CopyHookName(lStartBackground.macName, lrBackground.mHookName.mHookNameString);
            CgsGui::GuiModule::AddGuiEvent(lStartBackground, lpGuiInputBuffer);
        }
        if (lrBackground.mbStartRequested && lrBackground.mbStopRequest)
        {
            BrnGui::GuiPFXStopBackgroundHookEvent lStopBackground;
            std::memset(&lStopBackground, 0, sizeof(lStopBackground));
            lStopBackground.muGuid = 0;
            CopyHookName(lStopBackground.macName, lrBackground.mHookName.mHookNameString);
            CgsGui::GuiModule::AddGuiEvent(lStopBackground, lpGuiInputBuffer);
        }

        // pseudocode 231..243: a post-FX id request -> 495 by GUID, maxima 1.0, empty name.
        if (lrEffects.muRequestedPostFxId != 0)
        {
            BrnGui::GuiPFXHookEvent lById;
            std::memset(&lById, 0, sizeof(lById));
            lById.muGuid                      = lrEffects.muRequestedPostFxId;
            lById.mfMaximumBloomWeight        = 1.0f;
            lById.mfMaximumVignetteWeight     = 1.0f;
            lById.mfMaximumBlurWeight         = 1.0f;
            lById.mfMaximumDepthOfFieldWeight = 1.0f;
            lById.mfMaximum2DTintWeight       = 1.0f;
            lById.mfMaximum3DTintWeight       = 1.0f;
            lById.macName[0]                  = 0;
            CgsGui::GuiModule::AddGuiEvent(lById, lpGuiInputBuffer);
        }

        // [FLAG] pseudocode 244..249: out+0x751 -> GuiEventDirectorSettings (GetDir()), then
        // CgsGuiModuleIO::InputBuffer::SetCamera(guiIn, the CgsCamera copy) -- not transcribed.
    }
}
