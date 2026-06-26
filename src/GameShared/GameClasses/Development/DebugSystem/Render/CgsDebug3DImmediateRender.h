#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsFont.h"   // SafeResourceHandle<Font> (SetDebugFont)
#include "rw/math/vpu/types.h"                       // rw::math::vpu::Vector3 / Matrix44Affine
#include "rw/rwcore_structs.h"                       // rw::RGBA

// CgsDev::Debug3DImmediateRender - the world-space (3D) debug immediate renderer (the 3D counterpart
// of Debug2DImmediateRender). INCREMENTAL: only the debug-font handoff is modelled here, so
// DebugManager::SetDebugFont can set the font on BOTH the 2D and 3D renderers exactly as the X360
// does; the 3D drawing path (world-space DrawLine/DrawBox/DrawText) is the render follow-on. X360
// SetDebugFont (0x823B1448) stores the 2-word handle at this+0x2C/+0x30.

namespace CgsDev
{
    struct Debug3DImmediateRender
    {
        // The debug-font handoff (X360 0x823B1448): store the loaded bitmap Font's handle. Mirrors
        // Debug2DImmediateRender::SetDebugFont; DebugManager::SetDebugFont drives both.
        void SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont);
        bool HasResourceFont() const { return !mpFont.IsNull(); }

        // World-space primitive draws (declared-only; bodies are the 3D render follow-on). Recovered
        // from callers such as TriggerEntityModuleDebugComponent::RenderWorld: an oriented box given
        // local-space min/max corners + a world transform, and a sphere given a world centre +
        // radius, each tinted by an RGBA.
        void DrawBox(const rw::math::vpu::Vector3& lrMin,
                     const rw::math::vpu::Vector3& lrMax,
                     const rw::math::vpu::Matrix44Affine& lrTransform,
                     const rw::RGBA& lrColour);
        void DrawSphere(const rw::math::vpu::Vector3& lrCentre, f32 lfRadius, const rw::RGBA& lrColour);

        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;
    };
}
