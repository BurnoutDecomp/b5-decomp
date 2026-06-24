#ifndef BRN_ROAD_SIGN_H
#define BRN_ROAD_SIGN_H

#include "types.hpp"

// BrnGui::RoadSign -- a single road-sign nav record drawn by the crash-nav icon renderer
// (BrnGui::CrashNavIconRenderer::RenderRoadSign / RenderRoadSigns). Reconstructed from the
// X360 ARTIST build:
//   RoadSign::RoadSign(const RoadSign&)  @ 0x8244BAD8  (copy ctor)
//   RoadSign::operator=(const RoadSign&) @ 0x8244BBB8  (copy assignment)
// Both are plain 104-byte (0x68) field-by-field copies. The DWARF field names are not in
// the export; the layout below is recovered store-for-store from the two copy functions:
//   +0x00..+0x4F : 20 f32 (lfs/stfs)
//   +0x50        : u32   (lwz/stw)
//   +0x54        : f32   (lfs/stfs in operator=)
//   +0x58        : f32   (lfs/stfs in operator=)
//   +0x5C        : u32   (lwz/stw)
//   +0x60        : u32   (lwz/stw)
//   +0x64        : u32   (lwz/stw)
namespace BrnGui
{
    struct RoadSign
    {
        f32 mafField00[20];   // +0x00 (5x Vector4 / transform+colour block, exact split not in export)
        u32 muField50;        // +0x50
        f32 mfField54;        // +0x54
        f32 mfField58;        // +0x58
        u32 muField5C;        // +0x5C
        u32 muField60;        // +0x60
        u32 muField64;        // +0x64

        RoadSign() {}
        RoadSign(const RoadSign& lrOther);              // @ 0x8244BAD8
        RoadSign& operator=(const RoadSign& lrOther);   // @ 0x8244BBB8
    };
}

#endif
