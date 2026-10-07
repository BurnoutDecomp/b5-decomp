// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeRendererToX.cpp
//
// BrnGame::BrnGameModule renderer -> X bridges (X360 TU GameBridgeRendererToX.cpp).
// This slice homes the one the WORLD render pass rides:
//
//   BrnGameModule::BridgeRendererToWorld  @ 0x823CDD20
//
// Run once per frame from BrnGameModule::DoDispatch @0x823DC458, immediately after
// BrnRendererModule::Update has published this frame's GDL state into the renderer
// OUTPUT buffer: it copies every producer handle the world dispatch pass needs out
// of that buffer and into the world's BrnWorldIO::DispatchInputBuffer. Without it
// the world's GenerateDispatchLists has no DispatchFrame to stamp DRAWRENDERABLE
// commands into and every world dispatch list stays empty.
//
// The X360 body is a straight accessor -> setter forward (no locking: DoDispatch
// brackets the whole bridge set with the buffer locks), plus the two time values
// which come from the GAME MODULE's own timers rather than the renderer:
//   SetGameTime((f32)gm[10095320] + gm[10095324])
//   SetSimTime ((f32)gm[10095348] + gm[10095352])
// i.e. each is a whole-seconds counter plus its fractional remainder, summed into
// one float. Both timers are the original game-module members.
//
// The X360 tail returns the SetRenderSwitches result in r3 as a register artifact;
// the logical return type is void.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"   // BrnWorldIO::DispatchInputBuffer
#include "GameSource/Graphics/BrnRendererModuleIO.h"                 // RendererIO::OutputBuffer
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"

namespace BrnGame
{

// @ 0x823CDD20
void BrnGameModule::BridgeRendererToWorld(BrnWorldIO::DispatchInputBuffer* lpWorldDispatchInput,
                                          RendererIO::OutputBuffer* lpRendererOutput)
{
    // The GDL frame + the frame's shader-constant block.
    lpWorldDispatchInput->SetDispatchFrame(lpRendererOutput->GetDispatchFrame());
    lpWorldDispatchInput->SetShaderConstantsFrame(lpRendererOutput->GetShaderConstantsFrame());

    // The four world effects frames (X360 loop 0..3).
    for (u8 luSlot = 0; luSlot < 4; luSlot++)
    {
        lpWorldDispatchInput->SetEffectsFrame(luSlot, lpRendererOutput->GetWorldEffectsFrame(luSlot));
    }

    lpWorldDispatchInput->SetBlobbyShadowBuffer(lpRendererOutput->GetBlobbyShadowBuffer());
    lpWorldDispatchInput->SetCoronaSubmissionInterface(lpRendererOutput->GetCoronaSubmissionInterface());
    lpWorldDispatchInput->SetCameraInput(&lpRendererOutput->GetBrnCamera());

    // ARTIST823CDDD4..DE5C converts each signed whole-seconds counter to
    // float, then adds the timer's fractional accumulator.
    lpWorldDispatchInput->SetGameTime(static_cast<f32>(mGameTimer.GetAccumTicks())
                                    + mGameTimer.GetAccumulator());
    lpWorldDispatchInput->SetSimTime(static_cast<f32>(mSimTimer.GetAccumTicks())
                                   + mSimTimer.GetAccumulator());

    lpWorldDispatchInput->SetRenderSwitches(*lpRendererOutput->GetRenderSwitches());
}

// @0x823C1168. DoDispatch holds the input write/output read locks.
void BrnGameModule::BridgeRendererToEffects(
    BrnEffects::EffectsIO::DispatchInputBuffer* lpEffectsInput,
    RendererIO::OutputBuffer* lpRendererOutput)
{
    lpEffectsInput->SetDispatchFrame(lpRendererOutput->GetDispatchFrame());
    lpEffectsInput->SetBaseEffectsFrame(lpRendererOutput->GetBaseEffectsFrame());
    for (u8 luSlot = 0; luSlot < 2; ++luSlot)
        lpEffectsInput->SetFXEventsEffectsFrame(luSlot,
                                               lpRendererOutput->GetFXEventsEffectsFrame(luSlot));
    lpEffectsInput->SetEnvironmentMap(static_cast<renderengine::Texture*>(
        CgsGraphics::gTextureScopeTable.GetTexture(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP)));
}

// @ 0x823CD6B0. The module setter copies the five typed pointers and preserves
// its own camera, exactly as its original 823C86C0 body does.
void BrnGameModule::BridgeRendererToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                       RendererIO::OutputBuffer* lpRendererOutput)
{
    CgsGui::ImRendererSet lRenderers;
    lRenderers.Construct();
    lRenderers.mpIm2dRenderBuffer = lpRendererOutput->GetIm2dRenderBuffer();
    lRenderers.mpIm3dRenderBuffer = lpRendererOutput->GetIm3dRenderBuffer();
    lRenderers.mpIm3dRenderBufferUntex = lpRendererOutput->GetIm3dRenderBufferUntex();
    lRenderers.mpIm3dRenderBufferRacePosition = lpRendererOutput->GetIm3dRenderBufferRacePosition();
    lRenderers.mpIm3dRenderBufferMenusAndHud = lpRendererOutput->GetIm3dRenderBufferMenusAndHud();
    lpGuiInput->SetImRenderers(lRenderers);
}

}   // namespace BrnGame
