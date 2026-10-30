#pragma once
#include "types.hpp"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
namespace renderengine { class Texture; }
namespace CgsResource
{
    // DecFIGS CgsGuiGeometryObjects.h:46; ARTIST 82859300 reads +0,+4,+C.
    struct GuiGeometryMeshHeader
    {
        s32 miMeshType;
        s32 miTextureMode;
        s32 miTextureId;
        renderengine::Texture* mpTexture;
    };
    struct GuiGeometryMesh
    {
        GuiGeometryMeshHeader mMeshHeader;
        u32 muNumberOfVerticies;
        CgsGraphics::Basic2dColouredTexturedVertex** mppVerticies;
    };
    struct GuiGeometryFile
    {
        u32 muID;
        u32 muNumberOfMeshes;
        GuiGeometryMesh** mppGeometryMeshes;
        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta);
    };
    struct GuiGeometryObject
    {
        u32 muNumberOfFiles;
        u32 muNumberOfTexturePages;
        GuiGeometryFile** mppGeometryFiles;
        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta, bool lbEndianSwap);
    };
    CGS_STATIC_ASSERT((sizeof(GuiGeometryMeshHeader) == 16), "ARTIST GUI mesh header");
    CGS_STATIC_ASSERT((sizeof(GuiGeometryMesh) == 24), "ARTIST GUI mesh");
    CGS_STATIC_ASSERT((sizeof(GuiGeometryFile) == 12), "ARTIST GUI geometry file");
    CGS_STATIC_ASSERT((sizeof(GuiGeometryObject) == 12), "ARTIST GUI geometry object");
}
