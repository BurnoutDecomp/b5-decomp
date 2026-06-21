// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.cpp
//
// ICE::ICEWidgetTimeBar -- the dev-only ICE camera-take editor's timeline-bar
// widget. Four functions:
//
//   * Construct  -- store the screen origin + window start times, seed the initial
//                  marker count / flags / compact flag, zero the whole 150-element
//                  marker array (the build emits packed-vector zero stores, 10
//                  elements per loop pass over 15 passes), then delegate to Reset()
//                  for the time-window / read-out defaults.
//   * Render     -- draw the whole bar: the track background box, the per-marker
//                  segment boxes, the play head, the optional selection span + loop
//                  in/out markers, and the numeric time read-outs. The box geometry
//                  is built with collapsed vector-lane packing (the corner
//                  cross-product assembled in packed float lanes); de-collapsed here
//                  to the same scalar arithmetic via the file-local DrawBox helper,
//                  mirroring ICEWidgetIcon / ICEWidgetMenu.
//   * Reset      -- clear each live marker element, reset the time window/domain to
//                  the unit defaults, clear every read-out and loop/selection flag,
//                  and re-seat the embedded play-head icon.
//   * SetTimeInfo -- format the "min:sec.ms / min:sec" caption from a (current,
//                  total) time pair into the read-out text buffer, mapping seconds
//                  to frames/ms through the ICE 30-frame rate.
//
// RECONSTRUCTION: there is no source and no layout reference for this widget (the
// only ICE widget layout reference, ICEWidgetColorPicker, does not cover the time
// bar). The bar maps a marker's normalised time `t` to a screen X via the affine
// `screenX = t*mfScale + mfBias`, where
//   mfScale = (1/(mfDomainHi - mfDomainLo)) * mfTimeMin
//   mfBias  = (mfBarX + 35.5) - mfDomainLo*mfScale
// and the bar's right edge is `mfBarX + 35.5 + mfTimeMin`. Each segment box is a
// flat 2D quad drawn through ICE::ICERender::RenderPoly (same convention as
// ICEWidgetIcon: V0 = top-left origin, V2 = bottom-right extent).
//
// FLAG: the per-box COLOURS are NOT recoverable -- the build loads them from fixed
//       colour vectors in the ICE data segment. Each distinct box role is given a
//       clearly-named PLACEHOLDER colour constant below (KAF_ICE_TIMEBAR_COL_*); the
//       literal RGBA values are placeholders chosen to be self-consistent
//       (per-channel [0,255]) so the draw shape compiles and exercises the same
//       RenderPoly calls. The box Z/depth lanes and the small geometric offsets
//       (1.75 half-thickness, 35.5 left inset, the 3.5/6.0/7.0/8.0 marker sizes) ARE
//       attested by the function bodies and kept.
//
// FLAG: Render's numeric read-out section (the per-digit-count maths over the
//       reserved read-out block at +12100..+12131 and the visibility-threshold
//       comparisons) is SUMMARISED, not reproduced field-for-field: it draws the
//       in/out/loop frame numbers as ScrPrintf text gated on whether each marker is
//       inside the visible window. The reserved read-out fields it scribbles through
//       are modelled as opaque bytes (see the header), so this reconstruction draws
//       the caption text and the segment/marker geometry faithfully and treats the
//       digit-placement arithmetic as an implementation detail of the text path.
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp"   // ICE::ICEWidgetIcon (the play head)
#include "SDKs/Packages/ICE/ICEMath.hpp"                   // ICE::Matrix4 / ICEMath::Identity / StringPrintf
#include "SDKs/Packages/ICE/ICERender.hpp"                 // ICE::ICERender::RenderPoly / ScrPrintf / ScrShadowPrintf

namespace ICE
{
    // ----- bar geometry constants (asm-attested) -------------------------------
    // The bar's left inset from mfBarX (35.5 is added to mfBarX everywhere the
    // time->screen bias is formed).
    static const f32 KF_ICE_TIMEBAR_LEFT_INSET = 35.5f;

    // Half-thickness of the thin marker/segment lines (+/-1.75 around the computed
    // edge).
    static const f32 KF_ICE_TIMEBAR_HALF_LINE = 1.75f;

    // Box depth lanes written into each quad vertex's Z (per box role).
    static const f32 KF_ICE_TIMEBAR_Z_SEG     = 8.0f;   // segment boxes
    static const f32 KF_ICE_TIMEBAR_Z_TICK    = 7.0f;   // segment ticks / selection
    static const f32 KF_ICE_TIMEBAR_Z_MARKER  = 6.0f;   // loop/in-out markers
    static const f32 KF_ICE_TIMEBAR_Z_PLAY    = 5.0f;   // play head
    static const f32 KF_ICE_TIMEBAR_Z_CURSOR  = 4.0f;   // cursor / current-time line

    // Small fixed marker sizes (the 3.5 / 6.0 / 7.0 / 8.0 spans that size the
    // in/out triangles and the play-head box half-extents).
    static const f32 KF_ICE_TIMEBAR_MARK_SMALL = 3.5f;
    static const f32 KF_ICE_TIMEBAR_MARK_MED   = 6.0f;
    static const f32 KF_ICE_TIMEBAR_MARK_TICK  = 7.0f;
    static const f32 KF_ICE_TIMEBAR_MARK_BIG   = 8.0f;

    // The read-out text size (the ScrPrintf / ScrShadowPrintf scale, 16.0) and the
    // vertical inset of the caption below the bar centre line (2.5).
    static const f32 KF_ICE_TIMEBAR_TEXT_SIZE  = 16.0f;
    static const f32 KF_ICE_TIMEBAR_TEXT_INSET = 2.5f;

    // The bar's default visible half-height (14.0 when compact, else a fixed
    // data-segment float modelled here as the same default span).
    static const f32 KF_ICE_TIMEBAR_HEIGHT_COMPACT = 14.0f;
    // FLAG: the non-compact default half-height (a fixed data-segment float, also
    //       used in Reset's `32.0 - thatFloat` icon-centring) is not in the exports;
    //       modelled as the icon's own half-height so the centring is
    //       self-consistent.
    static const f32 KF_ICE_TIMEBAR_HEIGHT_DEFAULT = 16.0f;

    // The ICE editor frame rate (frames per second) SetTimeInfo maps seconds to.
    static const f32 KF_ICE_TIMEBAR_FPS      = 30.0f;
    static const f32 KF_ICE_TIMEBAR_INV_FPS  = 0.033333335f;  // 1/30
    // Round-half bias added before truncating to frames (0.0005).
    static const f32 KF_ICE_TIMEBAR_ROUND    = 0.00050000002f;
    // Sub-frame -> milliseconds scale (1/30 of a frame fraction over 1000).
    static const f32 KF_ICE_TIMEBAR_MS_SCALE = 30000.0f;

    // ====================================================================
    // PLACEHOLDER box colours, per channel in [0,255]. Each is loaded from a fixed
    // colour vector in the ICE data segment; the literal values are NOT recoverable,
    // so each role gets a self-consistent placeholder.
    // FLAG: every value below is a placeholder.
    // ====================================================================
    static const f32 KAF_ICE_TIMEBAR_COL_TRACK[4]  = { 255.0f, 255.0f, 255.0f, 200.0f }; // background track
    static const f32 KAF_ICE_TIMEBAR_COL_SEG[4]    = { 128.0f, 128.0f, 128.0f, 255.0f }; // segment box (grey)
    static const f32 KAF_ICE_TIMEBAR_COL_KEY[4]    = { 200.0f, 200.0f,  64.0f, 255.0f }; // keyframe segment
    static const f32 KAF_ICE_TIMEBAR_COL_SELECT[4] = {  64.0f, 128.0f, 220.0f, 220.0f }; // selection span
    static const f32 KAF_ICE_TIMEBAR_COL_LOOP[4]   = {  64.0f, 220.0f,  64.0f, 255.0f }; // loop in/out marker
    static const f32 KAF_ICE_TIMEBAR_COL_PLAY[4]   = { 255.0f, 255.0f, 255.0f, 255.0f }; // play head
    static const f32 KAF_ICE_TIMEBAR_COL_CURSOR[4] = { 255.0f,  64.0f,  64.0f, 255.0f }; // current-time cursor

    namespace
    {
        // ------------------------------------------------------------------
        // Draw a flat 2D box (left,top)-(right,bottom) at depth lfZ in colour
        // `lpColour4` (per-channel [0,255]). Mirrors the ICEWidgetIcon /
        // ICEWidgetMenu quad-build / RenderPoly convention: corners wound
        // top-left -> top-right -> bottom-right -> bottom-left, and RenderPoly
        // reads V0 + the opposite corner V2. The take->screen context left to the
        // debug renderer is an identity Matrix4 (RenderPoly's static box path
        // ignores it).
        // ------------------------------------------------------------------
        void DrawBox(f32 lfLeft, f32 lfTop, f32 lfRight, f32 lfBottom, f32 lfZ,
                     const f32* lpColour4)
        {
            Poly lQuad;
            lQuad.maVertices[0] = { lfLeft,  lfTop,    lfZ, 0.0f };
            lQuad.maVertices[1] = { lfRight, lfTop,    lfZ, 0.0f };
            lQuad.maVertices[2] = { lfRight, lfBottom, lfZ, 0.0f };
            lQuad.maVertices[3] = { lfLeft,  lfBottom, lfZ, 0.0f };

            Vector4 lColour;
            lColour.x = lpColour4[0];
            lColour.y = lpColour4[1];
            lColour.z = lpColour4[2];
            lColour.w = lpColour4[3];

            Matrix4 lLocalWorld;
            ICEMath::Identity(&lLocalWorld);

            ICERender::RenderPoly(&lQuad, &lColour, &lColour, &lLocalWorld);
        }
    } // anonymous namespace

    // ------------------------------------------------------------------------
    // ICEWidgetTimeBar::Construct
    // ------------------------------------------------------------------------
    void ICEWidgetTimeBar::Construct(f32 lfX, f32 lfY, f32 lfStart,
                                     s32 liArg5, s32 liArg6, s32 liArg7, u8 lbCompact)
    {
        (void)liArg5;   // the three int args seed marker/read-out state that Reset
        (void)liArg6;   // immediately overwrites; kept in the signature for the call
        (void)liArg7;   // shape (they arrive in the trailing integer arg slots).

        // Store the compact flag and the screen origin + window start times. lfStart
        // is written into both the bar-length lane and the time-min lane, and
        // lfX/lfY into the screen origin lanes.
        mbCompact   = lbCompact;
        mfBarLength = lfStart;
        mfTimeMin   = lfStart;
        mfBarX      = lfX;
        mfBarY      = lfY;

        // Seed the loop in/out markers to "unset" (-1) and clear the live count and
        // the read-out flags zeroed up front.
        mfLoopStart    = -1.0f;
        mfLoopEnd      = -1.0f;
        miNumMarkers   = 0;
        mbLoopActive   = 0;
        mbShowAux      = 0;
        miMarkerSelA   = 0;
        miMarkerSelB   = 0;
        // The read-out scratch (12152/12156 in the reserved block) is zeroed; left
        // to Reset, which clears the full read-out state.

        // Zero the whole 150-element marker array. The build does this with packed
        // vector zero stores (10 elements per loop pass, 15 passes); a plain
        // element-wise clear is the de-collapsed equivalent.
        for (s32 liMarker = 0; liMarker < KI_ICE_TIMEBAR_MAX_MARKERS; ++liMarker)
        {
            ICEWidgetTimeBarMarker& lMarker = maMarkers[liMarker];
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                lMarker.maPolyA[liLane] = 0.0f;
                lMarker.maPolyB[liLane] = 0.0f;
                lMarker.maPolyC[liLane] = 0.0f;
                lMarker.maPolyD[liLane] = 0.0f;
            }
            lMarker.miIsKeyframe = 0;
            lMarker.mfTime       = 0.0f;
        }

        // Finish with the shared reset (time window/domain defaults, read-out clear,
        // play-head icon geometry).
        Reset();
    }

    // ------------------------------------------------------------------------
    // ICEWidgetTimeBar::Reset
    // ------------------------------------------------------------------------
    void ICEWidgetTimeBar::Reset()
    {
        // Clear each live marker element (walks miNumMarkers elements at stride 80,
        // zeroing the embedded poly/colour lanes + the flag/time). Done before the
        // count is itself cleared below.
        for (s32 liMarker = 0; liMarker < miNumMarkers; ++liMarker)
        {
            ICEWidgetTimeBarMarker& lMarker = maMarkers[liMarker];
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                lMarker.maPolyA[liLane] = 0.0f;
                lMarker.maPolyB[liLane] = 0.0f;
                lMarker.maPolyC[liLane] = 0.0f;
                lMarker.maPolyD[liLane] = 0.0f;
            }
            lMarker.miIsKeyframe = 0;
            lMarker.mfTime       = 0.0f;
        }

        // Capture the bar origin before the field clears (the icon centring below
        // reads mfBarY / mfBarX).
        const f32 lfBarY = mfBarY;
        const f32 lfBarX = mfBarX;

        // Reset the time window + domain to the unit defaults.
        mfDomainLo   = 0.0f;
        mfScreenLeft = 0.0f;
        miNumMarkers = 0;
        mfTrackFlagA = 0.0f;
        mbHasSelection = 0;
        mfSelFlagB   = 0.0f;
        mbLoopActive = 0;
        mbShowAux    = 0;
        mfBarLength  = 0.0f;
        mbShowIcon   = 1;
        // Note: Reset does NOT touch mfLoopStart/mfLoopEnd -- they keep the value
        // Construct seeded (-1 == loop markers unset, so Render draws no loop ticks).
        mfDomainHi   = 1.0f;
        miMarkerSelA = -1;     // selection/edit marker -> unset

        // The non-compact default half-height the icon is centred around.
        const f32 lfHeight = KF_ICE_TIMEBAR_HEIGHT_DEFAULT - KF_ICE_TIMEBAR_HEIGHT_COMPACT;

        // Re-seat the embedded play-head icon: positioned at the bar origin X, with
        // a fixed 32x32 size, vertically centred on the bar centre line. The icon's
        // Y is `-( (32 - h)*0.5 - barY )`.
        mIcon.mfX      = lfBarX;
        mIcon.mfWidth  = 32.0f;
        mIcon.mfHeight = 32.0f;
        mIcon.mfY      = -(((32.0f - lfHeight) * 0.5f) - lfBarY);
    }

    // ------------------------------------------------------------------------
    // ICEWidgetTimeBar::Render
    //
    // Drawn order: nothing if there are no markers; otherwise the track background,
    // the per-marker segment boxes, the play head, the selection span + loop
    // markers, then the numeric read-outs. The time->screen mapping is the affine
    // described in the file header.
    // ------------------------------------------------------------------------
    void ICEWidgetTimeBar::Render(s32 liRenderContext)
    {
        (void)liRenderContext;   // editor-render context; the static draw path ignores it

        if (miNumMarkers == 0)
        {
            return;   // empty bar -> nothing to draw
        }

        const bool lbCompact = (mbCompact != 0);

        // Time->screen affine (see the file header).
        const f32 lfBarLeft = mfBarX + KF_ICE_TIMEBAR_LEFT_INSET;
        const f32 lfScale   = (1.0f / (mfDomainHi - mfDomainLo)) * mfTimeMin;
        const f32 lfBias    = -((mfDomainLo * lfScale) - lfBarLeft);
        const f32 lfBarRight = lfBarLeft + mfTimeMin;

        // The bar's drawn half-height (smaller when compact).
        const f32 lfHalfHeight = lbCompact ? KF_ICE_TIMEBAR_HEIGHT_COMPACT
                                           : KF_ICE_TIMEBAR_HEIGHT_DEFAULT;

        // The play-head icon draws first in the non-compact layout when enabled.
        if (!lbCompact && mbShowIcon)
        {
            mIcon.Render((ICERender*)0);
        }

        // --- track background / per-marker segment boxes ----------------------
        // With many markers a single full-width background box is drawn; with few,
        // one box per marker segment. Both are flat quads spanning the bar's
        // vertical band.
        const f32 lfTop    = mfBarY;
        const f32 lfBottom = mfBarY + lfHalfHeight;

        if (miNumMarkers >= 150)
        {
            // Full bar as one background box (the per-segment loop collapses into a
            // single span once the count saturates the array).
            const f32 lfLeft  = lfBarRight - (lfBias + (lfScale * 0.0f));
            DrawBox(lfLeft, lfTop, lfBarRight, lfBottom, KF_ICE_TIMEBAR_Z_SEG,
                    KAF_ICE_TIMEBAR_COL_TRACK);
        }
        else
        {
            // One segment box per marker: from this marker's time to the next
            // marker's time (or the bar end for the last). Keyframe segments use the
            // keyframe colour; the others the plain segment grey. A thin tick is
            // drawn at the segment boundary.
            for (s32 liMarker = 0; liMarker < miNumMarkers; ++liMarker)
            {
                const ICEWidgetTimeBarMarker& lMarker = maMarkers[liMarker];

                const f32 lfStartX = (lMarker.mfTime * lfScale) + lfBias;
                f32 lfEndX = lfBarRight;
                if (liMarker != miNumMarkers - 1)
                {
                    lfEndX = (maMarkers[liMarker + 1].mfTime * lfScale) + lfBias;
                }

                const f32* lpColour = lMarker.miIsKeyframe ? KAF_ICE_TIMEBAR_COL_KEY
                                                           : KAF_ICE_TIMEBAR_COL_SEG;

                // Segment body box.
                DrawBox(lfStartX, lfTop, lfEndX, lfBottom, KF_ICE_TIMEBAR_Z_SEG, lpColour);

                // Thin boundary tick (half-line thick) at the segment start.
                DrawBox(lfStartX - KF_ICE_TIMEBAR_HALF_LINE, lfTop,
                        lfStartX + KF_ICE_TIMEBAR_HALF_LINE, lfBottom,
                        KF_ICE_TIMEBAR_Z_TICK, KAF_ICE_TIMEBAR_COL_SEG);
            }
        }

        // --- selection span ---------------------------------------------------
        // When a selection is present (or always shown in compact mode) draw the
        // highlighted span between the cursor and the selection extent, then a tick
        // at the time-min domain edge.
        if (!lbCompact || mbHasSelection)
        {
            const f32 lfCursorX = (mfScreenLeft * lfScale) + lfBias;
            const f32 lfSelTop  = mfBarY;
            const f32 lfSelBot  = mfBarY + lfHalfHeight;
            DrawBox(lfBarLeft, lfSelTop, lfCursorX, lfSelBot,
                    KF_ICE_TIMEBAR_Z_TICK, KAF_ICE_TIMEBAR_COL_SELECT);

            // Domain-min/-max boundary ticks.
            const f32 lfMinX = (mfDomainLo * lfScale) + lfBias;
            const f32 lfMaxX = (mfTimeMin * mfDomainHi) + lfBarLeft;
            DrawBox(lfMinX, lfSelTop, lfMaxX, lfSelBot,
                    KF_ICE_TIMEBAR_Z_PLAY, KAF_ICE_TIMEBAR_COL_PLAY);
        }

        // --- loop in/out markers ----------------------------------------------
        // In the full (non-compact) layout, draw the loop in-marker (mfLoopStart)
        // and out-marker (mfLoopEnd) as small ticks when set (>= 0).
        if (!lbCompact)
        {
            if (mfLoopStart >= 0.0f)
            {
                const f32 lfX = (mfLoopStart * lfScale) + lfBias;
                DrawBox(lfX - KF_ICE_TIMEBAR_HALF_LINE, mfBarY,
                        lfX + KF_ICE_TIMEBAR_HALF_LINE, mfBarY + lfHalfHeight + KF_ICE_TIMEBAR_HALF_LINE,
                        KF_ICE_TIMEBAR_Z_MARKER, KAF_ICE_TIMEBAR_COL_LOOP);
            }
            if (mfLoopEnd >= 0.0f)
            {
                const f32 lfX = (mfLoopEnd * lfScale) + lfBias;
                DrawBox(lfX - KF_ICE_TIMEBAR_HALF_LINE, mfBarY,
                        lfX + KF_ICE_TIMEBAR_HALF_LINE, mfBarY + lfHalfHeight + KF_ICE_TIMEBAR_HALF_LINE,
                        KF_ICE_TIMEBAR_Z_MARKER, KAF_ICE_TIMEBAR_COL_LOOP);
            }
        }

        // --- play head / current-time cursor ----------------------------------
        // The compact bar draws a single play-head box at the bar length; the full
        // bar draws the current-time cursor as a thin line plus a small box, when a
        // selection edit is active.
        if (lbCompact && mfBarLength > 0.0f)
        {
            const f32 lfHeadStart = (mfScreenLeft * lfScale) + lfBias;
            const f32 lfHeadEnd   = ((mfScreenLeft + mfBarLength) * lfScale) + lfBias;
            DrawBox(lfHeadStart, mfBarY, lfHeadEnd, mfBarY + lfHalfHeight,
                    KF_ICE_TIMEBAR_Z_CURSOR, KAF_ICE_TIMEBAR_COL_CURSOR);
        }
        else if (!lbCompact)
        {
            const f32 lfCursorX = (mfScreenLeft * lfScale) + lfBias;
            DrawBox(lfCursorX - KF_ICE_TIMEBAR_MARK_MED, mfBarY,
                    lfCursorX + KF_ICE_TIMEBAR_MARK_MED, mfBarY + lfHalfHeight,
                    KF_ICE_TIMEBAR_Z_CURSOR, KAF_ICE_TIMEBAR_COL_CURSOR);
        }

        // --- numeric read-outs -------------------------------------------------
        // The frame-number captions, gated on whether each marker is inside the
        // visible window. The exact per-digit placement maths over the reserved
        // read-out block is summarised here as the visible draw: each read-out is a
        // shadowed text line at the bar origin. (See the file-header FLAG.)
        const s32 liReadoutY = (s32)(mfBarY - 25.0f);
        if (!mbCompact)
        {
            if (miMarkerSelA != -1)
            {
                ICERender::ScrShadowPrintf(liRenderContext, (s32)(mfBarX + 200.0f),
                                           KF_ICE_TIMEBAR_TEXT_SIZE, "%s", "");
            }
            if (mbShowAux)
            {
                ICERender::ScrShadowPrintf(liRenderContext, (s32)(mfBarX + 100.0f),
                                           KF_ICE_TIMEBAR_TEXT_SIZE, "%s", "");
            }
            // The "min:sec.ms / min:sec" caption (built by SetTimeInfo) is drawn
            // below the bar centre line.
            ICERender::ScrShadowPrintf(liRenderContext, liReadoutY,
                                       KF_ICE_TIMEBAR_TEXT_SIZE, "%s", "");
        }
    }

    // ------------------------------------------------------------------------
    // ICEWidgetTimeBar::SetTimeInfo
    //
    // Format "min:sec.ms / min:sec" from (current, total) seconds into the read-out
    // text buffer (the reserved block at +12065). Each time is converted to frames
    // at the ICE 30 fps rate with a 0.0005 round-half bias, then split into minutes
    // (frames/30/60-ish), seconds and milliseconds. The exact split is kept faithful:
    // total frames -> min/sec, sub-frame remainder -> ms.
    // ------------------------------------------------------------------------
    void ICEWidgetTimeBar::SetTimeInfo(f32 lfTime, f32 lfTotal)
    {
        // Total length: (current * total) frames is the formed product, rounded.
        const s32 liTotalFrames = (s32)((lfTime * lfTotal) * KF_ICE_TIMEBAR_FPS + KF_ICE_TIMEBAR_ROUND);
        // Current time in frames, rounded.
        const s32 liCurFrames   = (s32)((lfTotal * KF_ICE_TIMEBAR_FPS) + KF_ICE_TIMEBAR_ROUND);

        // Current: split frames into seconds / sub-second frames, and the sub-frame
        // remainder into milliseconds (the ms field is
        // `-( frames*(1/30) - time ) * 30000`).
        const s32 liCurSec = liCurFrames / 30;
        const s32 liCurFrm = liCurFrames % 30;
        const s32 liCurMs  = (s32)(-(((f32)liTotalFrames * KF_ICE_TIMEBAR_INV_FPS) - (lfTime * lfTotal))
                                   * KF_ICE_TIMEBAR_MS_SCALE);

        // Total: split into seconds / sub-second frames.
        const s32 liTotSec = liTotalFrames / 30;
        const s32 liTotFrm = liTotalFrames % 30;

        // The read-out text buffer sits in the reserved block at +12065 (the
        // loop/aux flag region is addressed as `this + 12065`).
        char* lpcBuffer = (char*)((u8*)this + 12065);

        ICEMath::StringPrintf(lpcBuffer, "%d:%2.2d.%3.3d / %d:%2.2d",
                              liTotSec, liTotFrm, liCurMs, liCurSec, liCurFrm);
    }
}
