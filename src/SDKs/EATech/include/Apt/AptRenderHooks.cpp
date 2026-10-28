// ===========================================================================
// EATech Apt -- the Apt -> RenderWare render-callback boundary (homed).
//
// AptHook_DrawShape is the boundary AptCharacter::render @0x810E74 calls to draw a
// shape character's geometry. On the console this is the indirect call
// `dword_1059C6B8(v6[8], op, tick)` -- i.e. the gAptFuncs.pfnDrawRenderingUnit slot
// applied to the shape's geometry sub-field (the AptCharacter word at +0x20). That
// slot is installed by CgsGui::AptAux::ConstructApt @0x5BA0F8 to
// CgsGui::AptCallbackRender::DrawRenderingUnit, which (for the Normal op) draws the
// rendering unit through CgsGui::AptRenderHandler::Render. So this hook is a LIVE
// boundary: it reads the geometry handle off the shape and dispatches through
// the installed gAptFuncs slot, reaching the committed DrawRenderingUnit.
//
// The geometry handle lives at AptCharacter +0x20 (console v6[8]); the AptCharacter
// base slice models only the first 16 bytes (the shape subclass adds the geometry
// field at +0x20), so the hook reads it positionally -- the host owns the loaded-
// shape layout, exactly as the boundary contract documents.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderHooks.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"   // AptCharacter (the shape; geometry @ +0x20)
#include "SDKs/EATech/include/Apt/Apt.h"            // gAptFuncs.pfnDrawRenderingUnit (the installed slot)

#include "types.hpp"

// The byte offset of the shape character's geometry sub-field within AptCharacter (console
// v6[8] == this + 0x20). The shape subclass lays it after the 16-byte base; the host reads it
// positionally (the loaded-shape layout is owned by the .apt parse, not this slice).
static const uintptr_t KU_SHAPE_GEOMETRY_OFFSET = 0x20;

// The former AptHookDrawImportGlyph / AptHookResolveImport no-op shims are DELETED
// (2026-08-07): no caller references them anywhere in the tree, and the real
// import-id resolution is the homed member AptCharacterAnimation::GetIDFromImportFile
// @0x82ADE998 (AptCharacterAnimation.cpp) that the AptCharacter::render import
// branch will call when it lands with the .apt parse.
