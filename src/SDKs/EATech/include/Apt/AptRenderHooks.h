#pragma once

#include <cstdint>   // intptr_t (the pointer-width ZId handles; XB1-verified)

// ===========================================================================
// EATech Apt -- the Apt -> RenderWare render-callback boundary.
//
// The Apt player does not rasterise anything itself: when it walks the render
// tree it sets the current transform/colour on the context (forwarded to the host
// via gAptFuncs.pfnSetVertexMatrix/pfnSetColourTransform) and, at each shape,
// calls a host "draw" callback that triangulates + fills the vector-shape
// geometry on the GPU. Those draw callbacks are the boundary between this engine
// and the RenderWare 2D vector renderer (a SEPARATE subsystem -- the host's RW
// graphics integration installs them; on PC that is the RW/D3D 2D path).
//
// Console: the callbacks are global function pointers in the Apt user-function
// table (gAptFuncs, Apt.h), reached by AptCharacter::render (@0x810E74):
//   dword_1059C6B8  draw a shape's geometry  == gAptFuncs.pfnDrawRenderingUnit
//   dword_1059C6A4  draw an imported glyph    == gAptFuncs.pfnBindTexture-adjacent slot
//   dword_1059C69C  resolve an imported id    == gAptFuncs.pfnLoadTexture-adjacent slot
//
// AptHook_DrawShape IS wired: the console's `dword_1059C6B8(char[8], op, tick)` is
// the gAptFuncs.pfnDrawRenderingUnit slot, which CgsGui::AptAux::ConstructApt installs
// to CgsGui::AptCallbackRender::DrawRenderingUnit -> AptRenderHandler::Render. So the
// hook reads the shape's geometry sub-field (AptCharacter +0x20, the console's v6[8])
// and calls through the installed slot. The imported-sub-character path is served
// by the homed member AptCharacterAnimation::GetIDFromImportFile @0x82ADE998 when
// the AptCharacter::render import branch lands -- no free-function shim remains here.
//
// (The PC hook takes the whole AptCharacter and reads the geometry from it -- the host
// owns the loaded-shape layout -- which keeps the AptCharacter base slice free of the
// .apt loaded layout the parse defines.)
// ===========================================================================

struct AptCharacter;
class AptRenderingContext;
enum AptMaskRenderOperation : int;

// Wired to gAptFuncs.pfnDrawRenderingUnit (see AptRenderHooks.cpp).
void AptHook_DrawShape(AptCharacter* pShape, AptMaskRenderOperation eOp, int nTick);

// ---------------------------------------------------------------------------
// Custom-control host hooks (AptRenderItemCustomControl). A custom control is a
// game-supplied widget the Apt player does not draw itself; the host installs
// these to draw/destroy it and to receive the instance-name render-data
// notifications. The slots are DEFINED in AptGlobals.cpp (null until the host
// installs them); declared here so the render items name the boundary.
// ---------------------------------------------------------------------------
extern bool gbAptCustomControlRenderEnabled;                                   // byte_82F733F6
extern void (*gpfnAptDestroyCustomControl)(intptr_t nZId);                     // dword_8324E898
extern void (*gpfnAptDrawCustomControl)(const char* pType, const char* pTarget,
                                        void* pPayload, const char* pProperties,
                                        AptMaskRenderOperation eOp, int nTick); // dword_8324E88C
extern void (*gpfnAptDrawCustomControlById)(intptr_t nZId, void* pPayload,
                                            AptMaskRenderOperation eOp, int nTick);   // dword_8324E89C
extern void (*gpfnAptCustomControlPushRenderData)(const char* pInstanceName);  // dword_8324E8CC
extern void (*gpfnAptCustomControlPopRenderData)(const char* pInstanceName);   // dword_8324E8D0
