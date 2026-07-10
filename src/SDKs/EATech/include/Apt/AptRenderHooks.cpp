// ===========================================================================
// EATech Apt -- the Apt -> RenderWare render-callback boundary (homed).
//
// AptHook_DrawShape is the boundary AptCharacter::render @0x810E74 calls to draw a
// shape character's geometry. On the console this is the indirect call
// `dword_1059C6B8(v6[8], op, tick)` -- i.e. the gAptFuncs.pfnDrawRenderingUnit slot
// applied to the shape's geometry sub-field (the AptCharacter word at +0x20). That
// slot is installed by CgsGui::AptAux::ConstructApt @0x5BA0F8 to
// CgsGui::AptCallbackRender::DrawRenderingUnit, which (for the Normal op) draws the
// rendering unit through CgsGui::AptRenderHandler::Render. So this hook is NOT a
// dangling FLAG: it reads the geometry handle off the shape and dispatches through
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

#include <cstdint>

// The byte offset of the shape character's geometry sub-field within AptCharacter (console
// v6[8] == this + 0x20). The shape subclass lays it after the 16-byte base; the host reads it
// positionally (the loaded-shape layout is owned by the .apt parse, not this slice).
static const std::uintptr_t KU_SHAPE_GEOMETRY_OFFSET = 0x20;

// AptHook_DrawShape -- console dword_1059C6B8(v6[8], op, tick). Reads the shape's geometry
// handle (the AptAssetRenderingUnit at +0x20) and dispatches it through the installed
// gAptFuncs.pfnDrawRenderingUnit slot (CgsGui::AptAux::ConstructApt installs it to
// CgsGui::AptCallbackRender::DrawRenderingUnit -> AptRenderHandler::Render).
// FLAG PC-platform leaf: this IS the console's host render-callback boundary (the indirect
// dword_1059C6B8 slot), not an engine Class::method.
void AptHook_DrawShape(AptCharacter* pShape, AptMaskRenderOperation eOp, int nTick)
{
    if (!pShape)
        return;

    // The shape's geometry sub-field (console v6[8] == *(this + 0x20)).
    AptAssetRenderingUnit lpRenderingUnit =
        *reinterpret_cast<AptAssetRenderingUnit*>(
            reinterpret_cast<char*>(pShape) + KU_SHAPE_GEOMETRY_OFFSET);

    // Dispatch through the installed host slot. CgsGui::AptAux::ConstructApt points this at
    // CgsGui::AptCallbackRender::DrawRenderingUnit; if the table has not been installed yet
    // (boot-order safety) the slot is null and there is nothing to draw.
    if (gAptFuncs.pfnDrawRenderingUnit)
        gAptFuncs.pfnDrawRenderingUnit(lpRenderingUnit, eOp, nTick);
}

// AptHookDrawImportGlyph / AptHookResolveImport -- the imported-sub-character draw/resolve
// path. FLAG: deferred with the .apt parse (the import-resolution branch in AptCharacter::render
// couples to the full loaded-.apt data layout). Homed as no-ops so the link is satisfied; the
// common (non-importing) shape path -- the boot-UI case -- renders correctly through DrawShape.
void AptHookDrawImportGlyph(AptCharacter* /*pImport*/, int /*nIndex*/, void* /*pGlyphData*/)
{
    // FLAG: imported-glyph draw deferred (the .apt import table is reconstructed with the parse).
}

int AptHookResolveImport(void* /*pImportFileData*/, int /*nImportId*/)
{
    // FLAG: imported-id resolution deferred (the .apt import table is reconstructed with the parse).
    return 0;
}
