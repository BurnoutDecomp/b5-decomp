#ifndef CGS_RESOURCE_TYPE_IDS_H
#define CGS_RESOURCE_TYPE_IDS_H

#include "types.hpp"

// Burnout Paradise resource-type ids: the value each CgsResource::Type::GetTypeID() returns
// and the id a BundleV2 entry carries for its resources. Sourced from the Burnout wiki
// "Resource Types" table (references/Wiki/types.json) and CROSS-VALIDATED against the X360
// ARTIST build's surviving named GetTypeID bodies -- every one matches the wiki:
//   RwMaterialCRC32 0xB=11, Renderable 0xC=12, MaterialTechnique 0xD=13, TextureState 0xE=14,
//   MaterialState 0xF=15, RwShaderParameter 0x14=20, KdTree 0x17=23, ClusteredMesh 0x24=36,
//   Model 0x2A=42.
//
// Texture (RwRaster) = 0: its GetTypeID is a trivial "return 0", so the linker ICF-folded it
// with the other return-0 stubs in the X360 image and it lost its symbol (which is why it is
// not separately named in any dump). The wiki value + the matching named siblings pin it.
namespace CgsResource
{
    enum EResourceType
    {
        E_RESOURCETYPE_TEXTURE                  = 0x0,   // RwRaster, PlaneTexture, TexturePage
        E_RESOURCETYPE_MATERIAL                 = 0x1,   // RwMaterial, MaterialAssembly
        E_RESOURCETYPE_RENDERABLE_MESH          = 0x2,   // Renderable (mesh)
        E_RESOURCETYPE_TEXTFILE                 = 0x3,
        E_RESOURCETYPE_DRAW_INDEX_PARAMS        = 0x4,
        E_RESOURCETYPE_INDEX_BUFFER             = 0x5,
        E_RESOURCETYPE_MESH_STATE               = 0x6,
        E_RESOURCETYPE_VERTEX_BUFFER            = 0x9,
        E_RESOURCETYPE_VERTEX_DESCRIPTOR        = 0xA,   // RwVertexDesc
        E_RESOURCETYPE_RW_MATERIAL_CRC32        = 0xB,
        E_RESOURCETYPE_RENDERABLE               = 0xC,   // RwRenderable, GtRenderable
        E_RESOURCETYPE_MATERIAL_TECHNIQUE       = 0xD,
        E_RESOURCETYPE_TEXTURE_STATE            = 0xE,   // RwTextureState
        E_RESOURCETYPE_MATERIAL_STATE           = 0xF,   // BlendState
        E_RESOURCETYPE_DEPTH_STENCIL_STATE      = 0x10,
        E_RESOURCETYPE_RASTERIZER_STATE         = 0x11,
        E_RESOURCETYPE_RW_SHADER_PROGRAM_BUFFER = 0x12,  // PixelShader, VertexShader
        E_RESOURCETYPE_RW_SHADER_PARAMETER      = 0x14,
        E_RESOURCETYPE_RENDERABLE_ASSEMBLY      = 0x15,
        E_RESOURCETYPE_RW_DEBUG                 = 0x16,
        E_RESOURCETYPE_KDTREE                   = 0x17,
        E_RESOURCETYPE_VOICE_HIERARCHY          = 0x18
        // ... higher ids (audio/gui/streaming/etc.) exist in the wiki table; added as their
        // handlers are brought up. See references/Wiki/types.json for the full list.
    };
}

#endif
