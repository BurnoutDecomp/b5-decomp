#include "BrnProgressBarRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::ProgressBarRenderer::Construct        @ 0x82446C00  (EXECUTED in the boot trace)
//   BrnGui::ProgressBarRenderer::GetID            @ 0x82446E50
//   BrnGui::ProgressBarRenderer::Prepare          @ 0x82446C48
//   BrnGui::ProgressBarRenderer::Release          @ 0x82446CF8
//   BrnGui::ProgressBarRenderer::RecvEvent        @ 0x82446DA0
//   BrnGui::ProgressBarRenderer::RenderQuadUntex  @ 0x8245C828
//
// The boot/loading "progress bar" custom HUD renderer. Construct chains the base
// constructor and primes the cached progress to -1 (no value yet) with the two one-shot
// stage guards and the heap-allocator pointer cleared. Prepare/Release are one-shot
// state-machine steps guarded by their stage enum (assert on any unexpected stage value).
// RecvEvent caches the progress percent from the matching event. RenderQuadUntex builds a
// 4-vertex coloured quad (triangle strip) and submits it through the untextured 3D
// immediate-render buffer.

namespace BrnGui
{
namespace
{
    // The "no progress value yet" sentinel Construct stores into the cached event
    // (flt_820037C8 == -1.0f).
    const f32 KF_NO_PROGRESS = -1.0f;

    // RecvEvent dispatch id: the ARTIST build compares the runtime event-type argument against
    // 225 (asm: `cmpwi r26, 0xE1`). FLAG: this is the ARTIST runtime id for the render-progress-bar
    // event; the DWARF/Feb-2007 GuiEventRenderProgressBar template tag is 223 (the enum value
    // E_GUI_RENDER_PROGRESS_BAR drifted between builds). The asm value is authoritative here.
    const s32 KI_EVENT_RENDER_PROGRESS_BAR = 225;

    // RenderQuadUntex colour scale: clamp each float channel to [0,1] then map to 0..255
    // (vmulfp by the 255.0f vector constant unk_8305A950 before the float->int truncation).
    const f32 KF_COLOUR_SCALE = 255.0f;

    // Triangle-strip topology id passed to ImRenderer::Render (renderengine PRIMITIVETYPE_TRIANGLESTRIPS).
    const s32 KI_PRIMITIVE_TRIANGLESTRIP = 6;

    // Pack one clamped+scaled float channel (already in 0..255) to a u8 via truncation,
    // matching the X360 fctidz (round-toward-zero float->int) + low-byte extract.
    inline u8 PackChannel(f32 lfScaled0To255)
    {
        return static_cast<u8>(static_cast<s32>(lfScaled0To255));
    }
}

// 0x82446C00 -- chain the base construct, then prime the renderer's own state: the cached
// progress is set to -1 (no value), and the two stage guards + the heap-allocator pointer
// are cleared to zero.
void ProgressBarRenderer::Construct()
{
    CgsGui::CustomRenderComponentInterface::Construct();

    mRenderProgressBarEvent.mfProgressPercent = KF_NO_PROGRESS;  // guest *(this+0x14) = -1.0f
    mePrepareStage  = E_PREPARESTAGE_START;                      // guest *(this+0x08) = 0
    meReleaseStage  = E_RELEASESTAGE_START;                      // guest *(this+0x0C) = 0
    mpHeapAllocator = nullptr;                                   // guest *(this+0x10) = 0
}

// 0x82446E50 -- the renderer's content id (a fixed 64-bit CgsID baked into the function).
CgsID ProgressBarRenderer::GetID() const
{
    // asm assembles 0xA77271B9CC3ADFA6 across r3 (high word A77271B9, low word CC3ADFA6).
    return 0xA77271B9CC3ADFA6ull;
}

// 0x82446C48 -- one-shot prepare step. On the START stage cache the supplied heap allocator
// (the 2nd parameter, asm stores r5) and advance to DONE; on DONE do nothing; any other stage
// value is an error (assert + return false). The queue parameter (1st) and the 2nd allocator
// (3rd) are unused by the ARTIST body.
bool ProgressBarRenderer::Prepare(void* /*lpEventQueueSmall*/, void* lpResourceAllocatorA,
                                  void* lpResourceAllocatorB)
{
    (void)lpResourceAllocatorB;  // 3rd param unused by the ARTIST body (asm never reads r6)

    if (mePrepareStage == E_PREPARESTAGE_START)
    {
        // asm: *(this+0x10) = r5 (the FIRST rw::IResourceAllocator*, the 2nd declared param).
        mpHeapAllocator = static_cast<rw::IResourceAllocator*>(lpResourceAllocatorA);
        mePrepareStage  = E_PREPARESTAGE_START;  // redundant guest store *(this+8)=0 before the +1 below
    }
    else if (mePrepareStage != E_PREPARESTAGE_DONE)
    {
        CGS_ASSERT(false, " unknown prepare stage in ProgressBarRenderer ");
        return false;
    }

    mePrepareStage = E_PREPARESTAGE_DONE;  // guest *(this+8) = 1
    return true;
}

// 0x82446CF8 -- one-shot release step. START or DONE advance to DONE; any other stage value is
// an error (assert + return false).
bool ProgressBarRenderer::Release()
{
    if (meReleaseStage == E_RELEASESTAGE_START)
    {
        meReleaseStage = E_RELEASESTAGE_START;  // redundant guest store *(this+0xC)=0 before the +1 below
    }
    else if (meReleaseStage != E_RELEASESTAGE_DONE)
    {
        CGS_ASSERT(false, " unknown release stage in ProgressBarRenderer ");
        return false;
    }

    meReleaseStage = E_RELEASESTAGE_DONE;  // guest *(this+0xC) = 1
    return true;
}

// 0x82446DA0 -- receive a GUI event. Asserts the event is non-null, then -- only for the
// render-progress-bar event id -- caches the event's progress percent.
void ProgressBarRenderer::RecvEvent(const void* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent != nullptr, " null event passed ");

    if (liEventType == KI_EVENT_RENDER_PROGRESS_BAR)
    {
        // asm: this->mRenderProgressBarEvent.mfProgressPercent = *(const float*)event
        // (the incoming GuiEventRenderProgressBar's leading mProgressPercent, at event offset 0).
        const GuiEventRenderProgressBar* lpProgressEvent =
            static_cast<const GuiEventRenderProgressBar*>(lpEvent);
        mRenderProgressBarEvent.mfProgressPercent = lpProgressEvent->mfProgressPercent;
    }
}

// 0x8245C828 -- draw one untextured coloured quad. The bounds Vector4 gives the screen
// rectangle (x=left, y=top, z=right, w=bottom); the colour Vector4 holds float RGBA in [0,1].
// Builds the four corners as a triangle strip (TL, BL, TR, BR), packs the shared colour to
// RGBA8, sets the identity transform, and submits the run.
void ProgressBarRenderer::RenderQuadUntex(CgsGraphics::Im3dRenderBufferUntex* lpBuffer,
                                          const Vector4& lrBounds,
                                          const Vector4& lrColour)
{
    using CgsGraphics::BasicColouredVertex;

    // Clamp the float colour to [0,1], scale to 0..255 and pack to one shared RGBA8 word.
    const f32 lfRed   = (lrColour.x < 0.0f ? 0.0f : (lrColour.x > 1.0f ? 1.0f : lrColour.x)) * KF_COLOUR_SCALE;
    const f32 lfGreen = (lrColour.y < 0.0f ? 0.0f : (lrColour.y > 1.0f ? 1.0f : lrColour.y)) * KF_COLOUR_SCALE;
    const f32 lfBlue  = (lrColour.z < 0.0f ? 0.0f : (lrColour.z > 1.0f ? 1.0f : lrColour.z)) * KF_COLOUR_SCALE;
    const f32 lfAlpha = (lrColour.w < 0.0f ? 0.0f : (lrColour.w > 1.0f ? 1.0f : lrColour.w)) * KF_COLOUR_SCALE;

    CgsGraphics::RGBA8 lColour;
    lColour.r = PackChannel(lfRed);
    lColour.g = PackChannel(lfGreen);
    lColour.b = PackChannel(lfBlue);
    lColour.a = PackChannel(lfAlpha);

    // Rectangle corners. The asm builds the four positions from the bounds lanes:
    //   left = x, top = y, right = z, bottom = w (the unused 3rd position lane stays 0).
    const f32 lfLeft   = lrBounds.x;
    const f32 lfTop    = lrBounds.y;
    const f32 lfRight  = lrBounds.z;
    const f32 lfBottom = lrBounds.w;

    // Triangle-strip order matching the asm vertex build: TL, BL, TR, BR.
    BasicColouredVertex laVertices[4];
    laVertices[0].mv3Pos = Vector3{ lfLeft,  lfTop,    0.0f, 0.0f };  // top-left
    laVertices[1].mv3Pos = Vector3{ lfLeft,  lfBottom, 0.0f, 0.0f };  // bottom-left
    laVertices[2].mv3Pos = Vector3{ lfRight, lfTop,    0.0f, 0.0f };  // top-right
    laVertices[3].mv3Pos = Vector3{ lfRight, lfBottom, 0.0f, 0.0f };  // bottom-right
    for (int i = 0; i < 4; ++i)
    {
        laVertices[i].mv4Colour = lColour;
    }

    // Set the identity world transform (the asm loads gIVector-based identity rows), then
    // submit the quad as a 4-vertex triangle strip.
    Matrix44 lIdentity;
    lIdentity.SetIdentity();
    lpBuffer->SetTransform(lIdentity);
    lpBuffer->Render(static_cast<renderengine::PrimitiveType>(KI_PRIMITIVE_TRIANGLESTRIP),
                     laVertices, 4);
}
}
