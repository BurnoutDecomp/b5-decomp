#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"
#include <cstddef>

// Embed check: pins the native-8 (GUIAPT64 "1:7:8") member offsets the .apt shape-geometry render
// walk + the native-8 rebase (AptFixupGeometryFileNative8) dereference. LAYOUT verified byte-for-byte
// from the loaded TITLE_SCREEN02 + B5HelperComponents shape meshes (crash-dump memory).
// (The console FixUp/FixDown are retired -- see CgsGuiGeometryObjects.cpp's note.)
namespace
{
    using CgsResource::GuiGeometryMesh;
    using CgsResource::GuiGeometryFile;
    using CgsResource::GuiGeometryObject;

    // File: {id@0, numMeshes@4, mppGeometryMeshes@8 (8-byte)}.
    static_assert(offsetof(GuiGeometryFile, muNumberOfMeshes)  == 4,  "file mesh count @ +4");
    static_assert(offsetof(GuiGeometryFile, mppGeometryMeshes) == 8,  "file mesh table @ +8 (native-8)");
    static_assert(sizeof(GuiGeometryFile) == 16, "GuiGeometryFile native-8 sizeof");

    // Mesh: native-8 widened header (see the struct comment). vertex count @0x18, table @0x20.
    static_assert(offsetof(GuiGeometryMesh, miMeshType)          == 0x00, "mesh type @ +0");
    static_assert(offsetof(GuiGeometryMesh, miTextureMode)       == 0x04, "mesh texmode @ +4");
    static_assert(offsetof(GuiGeometryMesh, miTextureId)         == 0x08, "mesh texid @ +8");
    static_assert(offsetof(GuiGeometryMesh, mpTexture)           == 0x10, "mesh texture ptr @ +0x10 (native-8)");
    static_assert(offsetof(GuiGeometryMesh, muNumberOfVerticies) == 0x18, "mesh vertex count @ +0x18 (native-8)");
    static_assert(offsetof(GuiGeometryMesh, mppVerticies)        == 0x20, "mesh vertex table @ +0x20 (native-8)");
    static_assert(sizeof(GuiGeometryMesh) == 0x28, "GuiGeometryMesh native-8 sizeof");

    // Object: {numFiles@0, numTexPages@4, mppGeometryFiles@8 (8-byte)}.
    static_assert(offsetof(GuiGeometryObject, muNumberOfFiles)  == 0, "object file count @ +0");
    static_assert(offsetof(GuiGeometryObject, mppGeometryFiles) == 8, "object file table @ +8 (native-8)");

    // Exercise the (gated-off on x64) console relocate methods so they compile against the widened
    // struct + are not dead-stripped. Never called at runtime on x64 (see CgsGuiGeometryObjects.cpp).
    void EmbedCheck(GuiGeometryFile* lpFile, GuiGeometryObject* lpObject, u32 luDelta)
    {
        lpFile->FixUp(luDelta);
        lpFile->FixDown(luDelta);
        lpObject->FixUp(luDelta);
        lpObject->FixDown(luDelta, false);
    }
}
