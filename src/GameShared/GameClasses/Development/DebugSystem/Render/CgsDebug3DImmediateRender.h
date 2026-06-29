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

        // Additional world-space primitive draws (declared-only; bodies are the 3D render follow-on).
        // Recovered from BrnDeformationDebugComponent::RenderWorld / DrawDetachedWheels (the deformation
        // rig visualiser): a hollow (wireframe) sphere, a hollow triangle / a line / an arrow between two
        // world points, and a coordinate-axis gizmo given a world transform, each tinted by an RGBA.

        // Wireframe sphere at a world centre + radius.
        void DrawHollowSphere(const rw::math::vpu::Vector3& lrCentre, f32 lfRadius, const rw::RGBA& lrColour);

        // Wireframe triangle from three world-space corners.
        void DrawHollowTriangle(const rw::math::vpu::Vector3& lrA, const rw::math::vpu::Vector3& lrB,
                                const rw::math::vpu::Vector3& lrC, const rw::RGBA& lrColour);

        // A line / an arrow from lrFrom to lrTo.
        void DrawLine(const rw::math::vpu::Vector3& lrFrom, const rw::math::vpu::Vector3& lrTo, const rw::RGBA& lrColour);
        void DrawArrow(const rw::math::vpu::Vector3& lrFrom, const rw::math::vpu::Vector3& lrTo, const rw::RGBA& lrColour);

        // A coordinate-axis gizmo (the three basis vectors of the transform, drawn from its
        // origin). The colour defaults (each axis is conventionally drawn in its own R/G/B);
        // callers that only have a transform - e.g. EffectsDebugComponent::RenderWorld (X360
        // 0x82278DB8) - pass just the transform, matching the single-argument X360 call.
        void DrawAxis(const rw::math::vpu::Matrix44Affine& lrTransform, const rw::RGBA& lrColour = rw::RGBA());

        // A SOLID (filled) oriented box given local-space min/max corners + a world transform.
        // The wireframe counterpart is DrawBox; the prop debug overlay draws a solid box then
        // overlays the wire box in black. (X360 callers: PropEntityDebugComponent::Draw 0x822A9770.)
        void DrawSolidBox(const rw::math::vpu::Vector3& lrMin,
                          const rw::math::vpu::Vector3& lrMax,
                          const rw::math::vpu::Matrix44Affine& lrTransform,
                          const rw::RGBA& lrColour);

        // World-space text at a world position, at a pixel scale (white). Recovered from the prop
        // debug overlay's per-prop stat read-outs (RenderProps / RenderPropStats / RenderTrafficLights).
        void DrawText(const rw::math::vpu::Vector3& lrWorldPosition, const char* lpcText, f32 lfScale);

        // The current debug camera world position (the cull origin the world-space debug passes
        // measure prop distance from). X360: read by RenderProps/RenderPropStats/RenderInertiaBoxes/
        // RenderTrafficLights as the first lane group of the renderer's view state (+0x7DB0).
        const rw::math::vpu::Vector3& GetCameraPosition() const;

        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;
    };
}
