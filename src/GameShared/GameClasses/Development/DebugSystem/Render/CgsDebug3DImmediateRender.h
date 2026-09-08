#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsFont.h"   // SafeResourceHandle<Font> (SetDebugFont)
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"
#include "rw/math/vpu/types.h"                       // rw::math::vpu::Vector3 / Matrix44Affine
#include "rw/rwcore_structs.h"                       // rw::RGBA

// CgsDev::Debug3DImmediateRender - the world-space (3D) debug immediate renderer (the 3D counterpart
// of Debug2DImmediateRender). INCREMENTAL: only the debug-font handoff is modelled here, so
// DebugManager::SetDebugFont can set the font on BOTH the 2D and 3D renderers exactly as the X360
// does; the 3D drawing path (world-space DrawLine/DrawBox/DrawText) is the render follow-on. X360
// SetDebugFont (0x823B1448) stores the 2-word handle at this+0x2C/+0x30.

namespace rw { struct IResourceAllocator; }   // struct -- must match rwcore_structs.h's class-key (MSVC mangling)
namespace CgsGraphics { class Im3dRenderBuffer; }   // the 3D debug render buffer (opaque on PC)

namespace CgsDev
{
    struct Debug3DImmediateRender
    {
        enum ZTestEnable
        {
            E_ZTEST_ON = 0,
            E_ZTEST_OFF = 1,
            E_ZTEST_COUNT = 2,
        };

        enum ProjectionMode
        {
            E_PROJECTION_2D = 0,
            E_PROJECTION_3D = 1,
            E_PROJECTION_COUNT = 2,
        };

        enum DrawingMode
        {
            E_DRAWING_LINES = 0,
            E_DRAWING_TRIANGLES = 1,
            E_DRAWING_QUADS = 2,
            E_DRAWING_TRISTRIP_SOLID = 3,
            E_DRAWING_TRISTRIP_LINES = 4,
            E_DRAWING_FONT = 5,
            E_DRAWING_COUNT = 6,
        };

        static const s32 KI_VERTEX_BUFFER_SIZE = 1000;

        // X360 Construct @0x8281A488, called by DebugManager::ConstructRenderer @0x8281ADD0 with
        // (this, the manager's allocator, UI-metrics width, UI-metrics height). The X360 body also
        // constructs the renderer's vector font + text renderer, creates the debug render states
        // (CreateDebugRenderStates) and builds the sphere index table (BuildSphereIndices); those
        // members/paths are the Debug3D render follow-on -- the slice modelled here initialises the
        // members this type carries (the font handle + the virtual screen size).
        void Construct(rw::IResourceAllocator* lpAllocator, f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight);

        // The debug-font handoff (X360 0x823B1448): store the loaded bitmap Font's handle. Mirrors
        // Debug2DImmediateRender::SetDebugFont; DebugManager::SetDebugFont drives both.
        void SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont);
        bool HasResourceFont() const { return !mpFont.IsNull(); }

        // X360 SetRenderBuffer inline (DebugManager::Render @0x8282F770 stores the frame's 3D debug
        // render buffer here after asserting it non-null, CgsDebug3DImmediateRender.h:282).
        void SetRenderBuffer(CgsGraphics::Im3dRenderBuffer* lpRenderBuffer) { mpRenderBuffer = lpRenderBuffer; }

        // X360 Begin/End - open/close the frame's 3D batch (DebugManager::RenderWorld @0x8282E030
        // brackets the buffered-prim replay + the component RenderWorld pass with these). BOUNDED:
        // the render-state/matrix-latch bodies land with the Debug3D render follow-on (nothing on
        // this build emits 3D debug geometry yet).
        void Begin(const rw::math::vpu::Matrix44& lrViewProjection,
                   rw::math::vpu::Vector3 lCameraPosition);
        void End();

        rw::math::vpu::Vector2 GetVirtualScreenSize() const;
        rw::math::vpu::Vector3 GetCameraPosition() const;
        const rw::math::vpu::Matrix44 GetViewProjectionMatrix() const;

        // World-space primitive draws (declared-only; bodies are the 3D render follow-on). Recovered
        // from callers such as TriggerEntityModuleDebugComponent::RenderWorld: an oriented box given
        // local-space min/max corners + a world transform, and a sphere given a world centre +
        // radius, each tinted by an RGBA.
        void DrawBox(rw::math::vpu::Vector3 lMin,
                     rw::math::vpu::Vector3 lMax,
                     rw::math::vpu::Matrix44Affine lTransform,
                     rw::RGBA lColour);
        void DrawBox(rw::math::vpu::Vector3 lMin, rw::math::vpu::Vector3 lMax, rw::RGBA lColour);
        void DrawSphere(rw::math::vpu::Vector3 lCentre, f32 lfRadius, rw::RGBA lColour);

        // Additional world-space primitive draws (declared-only; bodies are the 3D render follow-on).
        // Recovered from BrnDeformationDebugComponent::RenderWorld / DrawDetachedWheels (the deformation
        // rig visualiser): a hollow (wireframe) sphere, a hollow triangle / a line / an arrow between two
        // world points, and a coordinate-axis gizmo given a world transform, each tinted by an RGBA.

        // Wireframe sphere at a world centre + radius.
        void DrawHollowSphere(rw::math::vpu::Vector3 lCentre, f32 lfRadius, rw::RGBA lColour);

        // Wireframe triangle from three world-space corners.
        void DrawHollowTriangle(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                                rw::math::vpu::Vector3 lC, rw::RGBA lColour);

        // A line / an arrow from lrFrom to lrTo.
        void DrawLine(rw::math::vpu::Vector3 lFrom, rw::math::vpu::Vector3 lTo, rw::RGBA lColour);

        // A wireframe quad from four world-space corners (winding order as given), tinted by RGBA.
        // DWARF-authoritative shape (CgsDebug3DImmediateRender.h:98): all five args by value. Recovered
        // from BrnAI::BrnAIDebugUtils::DrawBoundryLineWithY (X360 0x827674F8), which draws each AI
        // boundary line as a vertical quad (top-start, top-end, bottom-end, bottom-start).
        void DrawQuad(rw::math::vpu::Vector3 lTopStart, rw::math::vpu::Vector3 lTopEnd,
                      rw::math::vpu::Vector3 lBottomEnd, rw::math::vpu::Vector3 lBottomStart,
                      rw::RGBA lColour);
        void DrawArrow(rw::math::vpu::Vector3 lFrom, rw::math::vpu::Vector3 lTo, rw::RGBA lColour);

        // A coordinate-axis gizmo (the three basis vectors of the transform, drawn from its
        // origin). The colour defaults (each axis is conventionally drawn in its own R/G/B);
        // callers that only have a transform - e.g. EffectsDebugComponent::RenderWorld (X360
        // 0x82278DB8) - pass just the transform, matching the single-argument X360 call.
        void DrawAxis(rw::math::vpu::Matrix44Affine lTransform);
        void DrawAxis(rw::math::vpu::Matrix44Affine lTransform, rw::RGBA lColour);

        // A SOLID (filled) oriented box given local-space min/max corners + a world transform.
        // The wireframe counterpart is DrawBox; the prop debug overlay draws a solid box then
        // overlays the wire box in black. (X360 callers: PropEntityDebugComponent::Draw 0x822A9770.)
        void DrawSolidBox(rw::math::vpu::Vector3 lMin,
                          rw::math::vpu::Vector3 lMax,
                          rw::math::vpu::Matrix44Affine lTransform,
                          rw::RGBA lColour);
        void DrawSolidBox(rw::math::vpu::Vector3 lMin, rw::math::vpu::Vector3 lMax, rw::RGBA lColour);

        void DrawSolidQuad(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                           rw::math::vpu::Vector3 lC, rw::math::vpu::Vector3 lD,
                           rw::RGBA lColour);
        void DrawAngleDeg(rw::math::vpu::Vector3 lPosition, f32 lfAngle, rw::RGBA lColour);
        void DrawAngleRad(rw::math::vpu::Vector3 lPosition, f32 lfAngle, rw::RGBA lColour);
        void DrawSolidTriangle(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                               rw::math::vpu::Vector3 lC, rw::RGBA lColour);
        void DrawSolidSphere(rw::math::vpu::Vector3 lCentre, f32 lfRadius, rw::RGBA lColour);
        void DrawPoint(rw::math::vpu::Vector3 lPosition, rw::RGBA lColour);
        void DrawCircle(rw::math::vpu::Matrix44Affine lTransform, f32 lfRadius, rw::RGBA lColour);
        void DrawCircle(rw::math::vpu::Vector3 lCentre, rw::math::vpu::Vector3 lNormal,
                        f32 lfRadius, rw::RGBA lColour);
        void DrawSolidArrow(rw::math::vpu::Vector3 lFrom, rw::math::vpu::Vector3 lTo,
                            rw::RGBA lColour);
        void DrawCapsule(rw::math::vpu::Vector3 lStart, rw::math::vpu::Vector3 lEnd,
                         f32 lfRadius, rw::RGBA lColour);
        void DrawCylinder(rw::math::vpu::Vector3 lStart, rw::math::vpu::Vector3 lEnd,
                          f32 lfRadius, rw::RGBA lColour);
        void DrawTriangle(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                          rw::math::vpu::Vector3 lC, rw::RGBA lColour);

        // World-space text at a world position, at a pixel scale (white). Recovered from the prop
        // debug overlay's per-prop stat read-outs (RenderProps / RenderPropStats / RenderTrafficLights).
        void DrawText(rw::math::vpu::Vector3 lWorldPosition, const char* lpcText,
                      f32 lfScale, rw::RGBA lColour = rw::RGBA(255, 255, 255, 255));

        // The current debug camera world position (the cull origin the world-space debug passes
        // measure prop distance from). X360: read by RenderProps/RenderPropStats/RenderInertiaBoxes/
        // RenderTrafficLights as the first lane group of the renderer's view state (+0x7DB0).
    private:
        void SetProjectionMode(ProjectionMode leMode);
        void SetZTestEnable(ZTestEnable leEnable);
        void SetDrawingMode(DrawingMode leMode);
        void DispatchVertices();
        void AddVertex(rw::math::vpu::Vector3 lPosition, rw::RGBA lColour);
        void AddLine(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB, rw::RGBA lColour);
        void AddTriangle(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                         rw::math::vpu::Vector3 lC, rw::RGBA lColour);
        void AddQuad(rw::math::vpu::Vector3 lA, rw::math::vpu::Vector3 lB,
                     rw::math::vpu::Vector3 lC, rw::math::vpu::Vector3 lD,
                     rw::RGBA lColour);

        ProjectionMode meProjectionMode;
        DrawingMode meDrawingMode;
        ZTestEnable meZTestEnable;
        rw::math::vpu::Vector3 mPointBoxRadius;
        f32 mfVirtualScreenWidth;
        f32 mfVirtualScreenHeight;
        CgsGraphics::Im3dRenderBuffer* mpRenderBuffer;
        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;
        VectorFont mVectorFont;
        CgsGraphics::BasicColouredTexturedVertex maIm3dVertsArray[KI_VERTEX_BUFFER_SIZE];
        s16 miIm3dVertsHead;
        rw::math::vpu::Matrix44 mViewProjectionMatrix;
        rw::math::vpu::Vector3 mCameraPosition;
        CgsGraphics::TextRenderer mTextRenderer;
        s32 mePrimitiveType;
        u8 maSphereIndices[192];
    };
}
