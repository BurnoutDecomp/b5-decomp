#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"
#include <cstddef>

// Embed check: exercises the bodied file/object relocation methods and pins the
// member offsets the X360 FixUp/FixDown walk dereferences (file count@4 / mesh table@8;
// mesh vertex count@16 / vertex table@20; object files@8).
namespace
{
    using CgsResource::GuiGeometryMesh;
    using CgsResource::GuiGeometryFile;
    using CgsResource::GuiGeometryObject;

    static_assert(offsetof(GuiGeometryFile, muNumberOfMeshes)  == 4,  "file mesh count @ +4");
    static_assert(offsetof(GuiGeometryFile, mppGeometryMeshes) == 8,  "file mesh table @ +8");

    static_assert(offsetof(GuiGeometryMesh, muNumberOfVerticies) == 16, "mesh vertex count @ +16");
    static_assert(offsetof(GuiGeometryMesh, mppVerticies)        == 20, "mesh vertex table @ +20");

    static_assert(offsetof(GuiGeometryObject, muNumberOfFiles)  == 0, "object file count @ +0");
    static_assert(offsetof(GuiGeometryObject, mppGeometryFiles) == 8, "object file table @ +8");

    void EmbedCheck(GuiGeometryFile* lpFile, GuiGeometryObject* lpObject, u32 luDelta)
    {
        lpFile->FixUp(luDelta);
        lpFile->FixDown(luDelta);
        lpObject->FixUp(luDelta);
        lpObject->FixDown(luDelta, false);
    }
}
