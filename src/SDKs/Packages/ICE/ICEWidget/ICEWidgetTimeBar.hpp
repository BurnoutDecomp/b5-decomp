#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTIMEBAR_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTIMEBAR_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp" // ICE::ICEWidgetIcon (the embedded play-head icon)

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp
//
// ICE::ICEWidgetTimeBar -- the dev-only ICE camera-take editor's timeline-bar
// widget. It draws the editor's horizontal "time bar": the track background, a
// row of segment/marker boxes (one per take element), the current-time play head,
// an optional selection span, loop/in-out markers, and the numeric time read-outs.
// ICEController::ConstructMenus builds it; ICEController::RenderBars draws it and
// (re)Resets it; SetTimeInfo formats the "min:sec.ms / min:sec" caption.
//
// Built like its widget siblings (ICEWidgetIcon / ICEWidgetMenu / ICEWidgetLogo)
// as a flat, NON-polymorphic struct: the empty ICEWidget base contributes no
// layout (EBO), so the first field sits at offset 0. The struct is large (the
// bodies reach past +0x2F98) and made of a leading per-element marker array, an
// embedded play-head icon, a block of time/geometry floats, and the numeric
// read-out fields.
//
// LAYOUT (offsets DERIVED from the four function bodies' field accesses; the ones
// the bodies actually touch are pinned with static_assert):
//
//   maMarkers[150]  @0x00   -- the per-take marker array: 150 elements of an
//                              80-byte stride (ICEWidgetTimeBarMarker). Construct
//                              initialises all 150 (the loop runs 15 times, each
//                              pass filling a 800-byte = 10-element group); Reset
//                              clears miNumMarkers of them; Render walks element `e`
//                              at this+e*80, reading the four embedded poly/colour
//                              lanes (+0x00/+0x10/+0x20/+0x30), an "is-keyframe"
//                              flag (+0x40) and the element's normalised time
//                              (+0x44). 150*80 == 12000 == miNumMarkers's offset.
//   miNumMarkers    @0x2EE0 -- number of live marker elements (12000).
//   mIcon          @0x2EE4 -- the embedded play-head ICEWidgetIcon (drawn when the
//                              "compact" flag is clear); Render calls
//                              mIcon.Render(this+12004) i.e. &mIcon.
//   mbShowIcon      @0x2EF4 -- non-zero enables the embedded play-head icon draw (12020).
//   mfScreenLeft    @0x2EF8 -- screen-space left of the bar / cursor time (12024).
//   mfTrackFlagA    @0x2EFC -- a track-state flag/time (12028) gating the read-outs.
//   mbHasSelection  @0x2F00 -- selection-present flag (12032).
//   mfSelFlagB      @0x2F04 -- a second selection/loop flag-time (12036).
//   mbLoopActive    @0x2F08 -- loop-marker active flag (12040).
//   mbShowAux       @0x2F0C -- auxiliary read-out flag (12044).
//   (read-out scratch block @0x2F10..0x2F67 -- 12048..12116, the per-digit
//    counts and the numeric values the read-out path formats; modelled reserved.)
//   miMarkerSelA    @0x2F68 -- selection/edit marker index (12136; -1 == unset).
//   miMarkerSelB    @0x2F6C -- a second marker step/index (12140).
//   mfLoopStart     @0x2F70 -- loop in-marker normalised time (12144; -1 == unset).
//   mfLoopEnd       @0x2F74 -- loop out-marker normalised time (12148; -1 == unset).
//   (12152/12156 @0x2F78/0x2F7C -- two more read-out values; modelled reserved.)
//   mfBarLength     @0x2F80 -- bar width lane (12160).
//   mfTimeMin       @0x2F84 -- window time min (12164).
//   mfDomainLo      @0x2F88 -- time->screen domain low (12168).
//   mfDomainHi      @0x2F8C -- time->screen domain high (12172).
//   mfBarX          @0x2F90 -- screen X origin of the bar (12176; +35.5 in Render).
//   mfBarY          @0x2F94 -- screen Y origin / centre line (12180).
//   mbCompact       @0x2F98 -- "compact"/minimised layout flag (12184); when set
//                              the bar collapses (no icon, single-line caption).
//
// FLAG: there is NO source and NO layout reference for this widget (the only ICE
//       widget layout reference, ICEWidgetColorPicker, does not cover the time
//       bar). EVERY member name/type/offset is DERIVED from the four bodies' field
//       accesses. The offsets the bodies actually read/write are
//       pinned below; the wide gaps between named fields (the read-out scratch
//       block around +0x2F10..+0x2F67 and unused padding) are modelled as reserved
//       byte arrays rather than invented fields. The marker element's internal
//       shape (poly region / flag / time) is itself asm-derived; see the .cpp.
//
// FLAG: the read-out block accessed by Render through the local digit-count helper
//       (offsets 12100/12101/12104/12108/12112/12116/12120/12124/12128/12132/
//       12152/12156) is modelled as a packed reserved region with the handful of
//       individually-touched fields named where their role is clear; the remainder
//       are reserved bytes (no independent attestation of each role). Those
//       reserved read-out fields are therefore NOT driven by Render/SetTimeInfo in
//       this reconstruction (the read-out digit/threshold maths is summarised).
// ============================================================================

namespace ICE
{
    // The per-take marker element of the time bar. 80-byte stride. Four embedded
    // screen-space poly/colour lanes the segment box is drawn from (RenderPoly reads
    // the element directly as its Poly/colour argument; the draw path touches the
    // lanes at +0x00, +0x10, +0x20 and +0x30). The flag at +0x40 marks a "keyframe"
    // segment and the float at +0x44 is the element's normalised time along the bar.
    // The remaining bytes are reserved (cleared by Reset/Construct but not
    // independently attested).
    // FLAG: element field names/offsets are derived from the body accesses.
    struct ICEWidgetTimeBarMarker
    {
        f32 maPolyA[4];         // +0x00  first embedded poly/colour lane
        f32 maPolyB[4];         // +0x10  second embedded poly/colour lane
        f32 maPolyC[4];         // +0x20  third embedded poly/colour lane
        f32 maPolyD[4];         // +0x30  fourth embedded poly/colour lane
        s32 miIsKeyframe;       // +0x40  non-zero -> draw the keyframe sub-box
        f32 mfTime;             // +0x44  normalised time of this element along the bar
        u8  maReserved48[0x08]; // +0x48  reserved (cleared, not independently attested)
    };

    // Marker array capacity: 150 elements (Construct's loop runs 15 passes, each
    // filling a 800-byte == 10-element group; 150*80 == 12000).
    static const s32 KI_ICE_TIMEBAR_MAX_MARKERS = 150;

    struct ICEWidgetTimeBar : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Build the time bar: store the screen origin (lfX/lfY) and the window
        // start times, clear the read-out state, then Reset(). The float args arrive
        // in order (lfX, lfY, lfStart): `lfX`/`lfY` are the screen origin; `lfStart`
        // seeds both the bar-length and time-min lanes; the three int args + the char
        // are the initial marker count / flags / compact flag. OWNED here (body in
        // the .cpp).
        // ------------------------------------------------------------------
        void Construct(f32 lfX, f32 lfY, f32 lfStart,
                       s32 liArg5, s32 liArg6, s32 liArg7, u8 lbCompact);

        // ------------------------------------------------------------------
        // Draw the whole bar: track background, the per-marker segment boxes, the
        // play head, optional selection span + loop markers, and the numeric time
        // read-outs. `liRenderContext` is the editor-render context the static
        // draw path threads to RenderPoly/ScrPrintf. OWNED here.
        // ------------------------------------------------------------------
        void Render(s32 liRenderContext);

        // ------------------------------------------------------------------
        // Clear the bar back to its empty state: zero each live marker element,
        // reset the time window/domain to the unit defaults, clear every read-out
        // and loop/selection flag, and re-seat the embedded icon geometry. OWNED
        // here.
        // ------------------------------------------------------------------
        void Reset();

        // ------------------------------------------------------------------
        // Format the "min:sec.ms / min:sec" caption from a (time, totalTime) pair
        // into the read-out text buffer. `lfTime` is the current time and
        // `lfTotal` the take length (both in seconds); the ICE frame rate (30) maps
        // them to frames/ms. OWNED here.
        // ------------------------------------------------------------------
        void SetTimeInfo(f32 lfTime, f32 lfTotal);

        // --- layout (offsets the bodies touch are pinned below) -----------
        ICEWidgetTimeBarMarker  maMarkers[KI_ICE_TIMEBAR_MAX_MARKERS];    // +0x0000  per-take marker array (80B stride)
        s32                     miNumMarkers;                             // +0x2EE0  live marker count (12000)
        ICEWidgetIcon           mIcon;                                   // +0x2EE4  embedded play-head icon (12004)
        s32                     mbShowIcon;                               // +0x2EF4  enable the play-head icon (12020)
        f32                     mfScreenLeft;                             // +0x2EF8  bar screen left / cursor time (12024)
        f32                     mfTrackFlagA;                             // +0x2EFC  read-out gate flag/time (12028)
        s32                     mbHasSelection;                           // +0x2F00  selection-present flag (12032)
        f32                     mfSelFlagB;                               // +0x2F04  second selection/loop flag (12036)
        s32                     mbLoopActive;                             // +0x2F08  loop-marker active flag (12040)
        s32                     mbShowAux;                                // +0x2F0C  auxiliary read-out flag (12044)
        u8                      maReservedReadout[0x58];                  // +0x2F10  numeric read-out scratch block (12048..12131)
        s32                     miMarkerSelA;                             // +0x2F68  selection/edit marker index (12136; -1 == unset)
        s32                     miMarkerSelB;                             // +0x2F6C  second marker step/index (12140)
        f32                     mfLoopStart;                              // +0x2F70  loop in-marker time (12144; -1 == unset)
        f32                     mfLoopEnd;                                // +0x2F74  loop out-marker time (12148; -1 == unset)
        u8                      maReserved2F78[0x08];                     // +0x2F78  two more read-out values (12152/12156)
        f32                     mfBarLength;                              // +0x2F80  bar length lane (12160)
        f32                     mfTimeMin;                                // +0x2F84  window time min (12164)
        f32                     mfDomainLo;                               // +0x2F88  time->screen domain low (12168)
        f32                     mfDomainHi;                               // +0x2F8C  time->screen domain high (12172)
        f32                     mfBarX;                                   // +0x2F90  screen X origin (12176)
        f32                     mfBarY;                                   // +0x2F94  screen Y origin / centre line (12180)
        s32                     mbCompact;                                // +0x2F98  compact/minimised layout flag (12184)
    };

    // Pin the offsets the four bodies read/write. The base is empty (EBO), so the
    // leading reserved bytes start at offset 0. sizeof is NOT asserted -- only the
    // fields below are proven; trailing/interleaved members are reserved bytes.
    static_assert(offsetof(ICEWidgetTimeBar, maMarkers)     == 0x0000, "ICEWidgetTimeBar::maMarkers offset");
    static_assert(offsetof(ICEWidgetTimeBar, miNumMarkers)  == 0x2EE0, "ICEWidgetTimeBar::miNumMarkers offset (12000)");
    static_assert(offsetof(ICEWidgetTimeBar, mIcon)        == 0x2EE4, "ICEWidgetTimeBar::mIcon offset (12004)");
    static_assert(offsetof(ICEWidgetTimeBar, mbShowIcon)    == 0x2EF4, "ICEWidgetTimeBar::mbShowIcon offset (12020)");
    static_assert(offsetof(ICEWidgetTimeBar, mfScreenLeft)  == 0x2EF8, "ICEWidgetTimeBar::mfScreenLeft offset (12024)");
    static_assert(offsetof(ICEWidgetTimeBar, mfTrackFlagA)  == 0x2EFC, "ICEWidgetTimeBar::mfTrackFlagA offset (12028)");
    static_assert(offsetof(ICEWidgetTimeBar, mbHasSelection)== 0x2F00, "ICEWidgetTimeBar::mbHasSelection offset (12032)");
    static_assert(offsetof(ICEWidgetTimeBar, mfSelFlagB)    == 0x2F04, "ICEWidgetTimeBar::mfSelFlagB offset (12036)");
    static_assert(offsetof(ICEWidgetTimeBar, mbLoopActive)  == 0x2F08, "ICEWidgetTimeBar::mbLoopActive offset (12040)");
    static_assert(offsetof(ICEWidgetTimeBar, mbShowAux)     == 0x2F0C, "ICEWidgetTimeBar::mbShowAux offset (12044)");
    static_assert(offsetof(ICEWidgetTimeBar, miMarkerSelA)  == 0x2F68, "ICEWidgetTimeBar::miMarkerSelA offset (12136)");
    static_assert(offsetof(ICEWidgetTimeBar, miMarkerSelB)  == 0x2F6C, "ICEWidgetTimeBar::miMarkerSelB offset (12140)");
    static_assert(offsetof(ICEWidgetTimeBar, mfLoopStart)   == 0x2F70, "ICEWidgetTimeBar::mfLoopStart offset (12144)");
    static_assert(offsetof(ICEWidgetTimeBar, mfLoopEnd)     == 0x2F74, "ICEWidgetTimeBar::mfLoopEnd offset (12148)");
    static_assert(offsetof(ICEWidgetTimeBar, mfBarLength)   == 0x2F80, "ICEWidgetTimeBar::mfBarLength offset (12160)");
    static_assert(offsetof(ICEWidgetTimeBar, mfTimeMin)     == 0x2F84, "ICEWidgetTimeBar::mfTimeMin offset (12164)");
    static_assert(offsetof(ICEWidgetTimeBar, mfDomainLo)    == 0x2F88, "ICEWidgetTimeBar::mfDomainLo offset (12168)");
    static_assert(offsetof(ICEWidgetTimeBar, mfDomainHi)    == 0x2F8C, "ICEWidgetTimeBar::mfDomainHi offset (12172)");
    static_assert(offsetof(ICEWidgetTimeBar, mfBarX)        == 0x2F90, "ICEWidgetTimeBar::mfBarX offset (12176)");
    static_assert(offsetof(ICEWidgetTimeBar, mfBarY)        == 0x2F94, "ICEWidgetTimeBar::mfBarY offset (12180)");
    static_assert(offsetof(ICEWidgetTimeBar, mbCompact)     == 0x2F98, "ICEWidgetTimeBar::mbCompact offset (12184)");

    // The marker element is 80 bytes (the +80 stride the bodies walk); the two fields
    // the bodies read by offset are pinned.
    static_assert(sizeof(ICEWidgetTimeBarMarker) == 80, "ICEWidgetTimeBarMarker stride must be 80");
    static_assert(offsetof(ICEWidgetTimeBarMarker, miIsKeyframe) == 0x40, "ICEWidgetTimeBarMarker::miIsKeyframe offset");
    static_assert(offsetof(ICEWidgetTimeBarMarker, mfTime)       == 0x44, "ICEWidgetTimeBarMarker::mfTime offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTIMEBAR_HPP
