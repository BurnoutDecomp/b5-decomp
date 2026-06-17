// RenderWare 4 graphics (`rw::graphics::`) type vocabulary for the Burnout PC decomp.
// Layout-faithful to the Spore PC PDB (IDA Files/SporeApp.pdb) -- the same EA RenderWare 4
// graphics module Burnout uses -- extracted with llvm-pdbutil. The Spore PDB is x86 (4-byte
// pointers, sizeof noted per type); the Burnout PC build is x64, so pointer fields widen
// (field order/types stay faithful, exact offsets/sizeof are the x64 target's).
#pragma once
#include <cstdint>
#include "rw/rwcore_structs.h"   // rw::Resource / rw::ResourceDescriptor

namespace rw {
namespace graphics {

// rw::graphics::Raster (Spore sizeof = 32, x86) - a texture's pixel surface. On PC it wraps a
// D3D9 texture (m_d3dTexture) and m_format is the D3DFORMAT. NOTE: the X360 build stores the
// pixels as packed GPU-memory offsets in a larger, console-specific raster layout, so the
// X360 CgsResource::RwRasterResourceType::FixUp (which rebases packed offsets at +0x20/+0x30)
// does NOT apply to this PC raster -- on PC a texture is realised by creating the D3D texture
// from the resource bytes (rw::graphics::Raster::Initialize), not by offset rebasing.
struct Raster {
    // D3DFORMAT values (the PC raster's pixel format).
    enum Format {
        FORMAT_NA            = -1,
        FORMAT_R8G8B8        = 20,
        FORMAT_A8R8G8B8      = 21,
        FORMAT_X8R8G8B8      = 22,
        FORMAT_R5G6B5        = 23,
        FORMAT_X1R5G5B5      = 24,
        FORMAT_A1R5G5B5      = 25,
        FORMAT_A4R4G4B4      = 26,
        FORMAT_R3G3B2        = 27,
        FORMAT_A8            = 28,
        FORMAT_A8R3G3B2      = 29,
        FORMAT_X4R4G4B4      = 30,
        FORMAT_A2B10G10R10   = 31,
        FORMAT_A8B8G8R8      = 32,
        FORMAT_X8B8G8R8      = 33,
        FORMAT_G16R16        = 34,
        FORMAT_A2R10G10B10   = 35,
        FORMAT_A16B16G16R16  = 36,
        FORMAT_A8P8          = 40,
        FORMAT_P8            = 41,
        FORMAT_L8            = 50,
        FORMAT_A8L8          = 51,
        FORMAT_A4L4          = 52,
        FORMAT_V8U8          = 60,
        FORMAT_L6V5U5        = 61,
        FORMAT_X8L8V8U8      = 62,
        FORMAT_Q8W8V8U8      = 63,
        FORMAT_V16U16        = 64,
        FORMAT_A2W10V10U10   = 67,
        FORMAT_UYVY          = 1498831189,
        FORMAT_R8G8_B8G8     = 1195525970,
        FORMAT_YUY2          = 844715353,
        FORMAT_G8R8_G8B8     = 1111970375,
        FORMAT_DXT1          = 827611204,
        FORMAT_DXT2          = 844388420,
        FORMAT_DXT3          = 861165636,
        FORMAT_DXT4          = 877942852,
        FORMAT_DXT5          = 894720068,
        FORMAT_D16_LOCKABLE  = 70,
    };

    // The view returned by Lock() onto one mip level's pixels (Spore sizeof = 28).
    struct Locked {
        void*    pixelData;    // +0
        Raster*  raster;       // +4
        uint32_t level;        // +8
        uint32_t stride;       // +12
        uint32_t sliceStride;  // +16
        uint32_t lockFlags;    // +20
        uint32_t volumeDepth;  // +24
    };

    Format    m_format;        // +0   D3DFORMAT
    int       m_type;          // +4   raster type flags
    void*     m_d3dTexture;    // +8   IDirect3DBaseTexture9*
    uint16_t  m_width;         // +12
    uint16_t  m_height;        // +14
    uint8_t   m_depth;         // +16  bits per pixel
    uint8_t   m_numMipLevels;  // +17
    uint8_t   m_face;          // +18  active cube-map face
    uint8_t   m_pad;           // +19
    Raster*   m_nextParent;    // +20  D3D9 unmanaged-list link
    void*     m_swapChain;     // +24
    void*     m_window;        // +28
};

} // namespace graphics
} // namespace rw
