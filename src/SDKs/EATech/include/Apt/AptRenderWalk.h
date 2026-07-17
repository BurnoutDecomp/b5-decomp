#pragma once

// ===========================================================================
// EATech Apt -- the render-tree walk entry point.
//
// AptRender @0x82AF33E8 -- traverse the current target's render-item tree and issue
// each visible node's draw (through the render-item Render() virtual -> AptCharacter::
// render -> gAptFuncs.pfnDrawRenderingUnit -> CgsGui::AptRenderHandler::Render, which
// appends to the Apt Im2d command buffer). CgsGui::AptAux::Render @0x82848FB8 drives it
// each frame through AptRenderTarget (the render half of the Update/Render target pair).
//
// nElapsedMs: the render-side elapsed milliseconds -- banked onto the consumed render
// tick (dword_8324E524), clamped strictly below the update side's credit (dword_8324E520).
// nLayerMask/nLevelMask: the set of display layers to draw (bit per depth layer; the
// AptAnimLevelE of the PS3 signature). AptAux passes -1 (every layer).
// ===========================================================================

struct AptTarget;

void AptRender(int nElapsedMs, int nLayerMask);

// AptRenderTarget @0x82AF4ED0 (PS3 _Z15AptRenderTargetPvj13AptAnimLevelE @0x7FBFCC) --
// run AptRender with pTarget swapped in as the render-side current context, restoring
// the previous context after (the render mirror of AptUpdateTarget @0x82B0DE80).
void AptRenderTarget(AptTarget* pTarget, int nElapsedMs, int nLevelMask);
