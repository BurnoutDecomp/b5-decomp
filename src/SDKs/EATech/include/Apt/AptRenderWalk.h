#pragma once

// ===========================================================================
// EATech Apt -- the render-tree walk entry point.
//
// AptRender @0x82AF33E8 -- traverse the current target's render-item tree and issue
// each visible node's draw (through the render-item Render() virtual -> AptCharacter::
// render -> gAptFuncs.pfnDrawRenderingUnit -> CgsGui::AptRenderHandler::Render, which
// appends to the Apt Im2d command buffer). The host (BrnAptRuntimeBringUp) calls this
// each frame between filling and flushing that command buffer.
//
// nLayerMask: the set of display layers to draw (bit per depth layer); pass 0 for "all
// layers" (the console passes the target's saved layer set; the boot title draws all).
// ===========================================================================

void AptRender(int nLayerMask);
