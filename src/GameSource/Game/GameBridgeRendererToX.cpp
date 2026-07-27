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
// one float. [FLAG] those two timer members sit in BrnGameModule's omitted layout
// range (this incremental layout declares only reached members), so the two Set
// calls are recorded here and deferred with the game-module timer block; the world
// dispatch pass reads mfGameTime/mfSimTime only for effects/animation phase, not
// for list generation.
//
// The X360 tail returns the SetRenderSwitches result in r3 as a register artifact;
// the logical return type is void.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"   // BrnWorldIO::DispatchInputBuffer
#include "GameSource/Graphics/BrnRendererModuleIO.h"                 // RendererIO::OutputBuffer

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
    lpWorldDispatchInput->SetCameraInput(lpRendererOutput->GetBrnCamera());

    // [FLAG] SetGameTime / SetSimTime -- see the TU note above (the two game-module
    // timer members are in this layout's omitted range).

    lpWorldDispatchInput->SetRenderSwitches(*lpRendererOutput->GetRenderSwitches());
}

}   // namespace BrnGame
