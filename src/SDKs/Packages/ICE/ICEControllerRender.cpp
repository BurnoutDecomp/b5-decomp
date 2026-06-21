// ============================================================================
// SDKs/Packages/ICE/ICEControllerRender.cpp
//
// ICE::ICEController -- the in-game ICE camera-take editor/driver. This TU bodies
// the RENDER group of the controller's 23 functions:
//
//   * Render           -- the top-level editor overlay entry. Gated on the editor
//                         being active (miState > 0): draws a full-screen backdrop
//                         poly, then (if the "show UI" toggle is set) the status
//                         line, the animated logo + widescreen marker, the two
//                         time bars (RenderBars), the active menu (RenderMenus)
//                         and -- in the edit-start/edit-end-key states -- the 3D
//                         look-at target box (RenderTargetBox).
//   * RenderBars       -- fill + draw the two timeline bars (the main bar and the
//                         secondary/assembly bar) plus the read-out info list:
//                         per-marker segment boxes, the play head, the numeric
//                         time read-outs and the per-state read-out lines.
//   * RenderMenus      -- draw the menu widget(s) selected by the editor state
//                         (each preceded by a RefreshMenu fill).
//   * RenderTargetBox  -- place the editor's 3D look-at target box at the take's
//                         decoded look-at point (a reference-space transform from
//                         the camera-space handler, times the look-at offset) and
//                         draw it.
//
// The reconstruction reverses compiler inlining of the embedded ICETake / widget /
// author-base field accesses back into named-member access against the layout in
// ICEController.hpp; the widget draw work is delegated to the committed widget
// Render/Reset/SetTimeInfo APIs and ICERender.
//
// FLAG (ICEAuthor base): the controller IS-AN ICEAuthor and the bodies here call a
// few ICEAuthor methods (GetCurrentSubtakeGuid / IsEndKeyState) on `this`. Until the
// ICEAuthor base lands on the struct (see ICEController.hpp), those calls are made
// through a reinterpret_cast<ICEAuthor*>(this) -- the controller shares the author's
// `this`, and the author fields sit at the same offsets. FLAGged at each call site.
//
// FLAG (unrecoverable data tables): the bar-marker layout draws from several ICE
// data-segment tables that carry no symbol and no source/layout reference -- the
// per-channel marker-description table (the "which elements get a marker" list), the
// element-index map, and the segment/marker box COLOUR tables (selected by an editor
// take query). None are recoverable, so they are modelled here as clearly-FLAGGED
// file-local `static const` placeholders; the per-marker geometry fill is summarised
// against them. The control flow, the attested member/API accesses and the time/
// parameter maths are reconstructed faithfully.
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                    // ICE::ICEAuthor (the base; GetCurrentSubtakeGuid / IsEndKeyState)
#include "SDKs/Packages/ICE/ICEData.hpp"                      // ICE::ICETake / ICEChannel value+interval queries
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"                 // ICE::ICEElementDescriptions[] / eICESpace
#include "SDKs/Packages/ICE/ICERender.hpp"                    // ICE::ICERender::RenderPoly / ScrShadowPrintf
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"        // ICE::CameraSpaceHandler::GetTransformToWorld (RenderTargetBox)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp"   // ICE::ICEWidgetTimeBar::Render / Reset / SetTimeInfo
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"  // ICE::ICEWidgetInfoList::Render
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"      // ICE::ICEWidgetMenu::Render
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"      // ICE::ICEWidgetLogo::Render
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.hpp" // ICE::ICEWidgetWideScrMarker::Render
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp" // ICE::ICEWidgetTargetBox::Render
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (project assert macro)

namespace ICE
{

// ----------------------------------------------------------------------------
// Shared draw constants.
// ----------------------------------------------------------------------------

// ICE frame rate: time(seconds) -> frames (the marker/read-out maths scale by 30).
static const f32 KF_ICE_FRAME_RATE = 30.0f;

// Round-to-nearest bias added before the frame-count truncation.
static const f32 KF_ICE_ROUND_BIAS = 0.00050000002f;

// Reciprocal of ICE_PARAMETER_MAX (65535): unpacks a u16 interval-key parameter back
// into the unit interval [0,1]. The marker times are stored as quantized u16s.
static const f32 KF_ICE_PARAM_UNPACK = 0.000015259022f; // == 1.0f / 65535.0f

// The editor bar's screen-space origin (the X/Y the main time bar is built at by
// ConstructMenus, and the base ScrShadowPrintf position for the status line). PLACEHOLDER:
// the two base coordinates are ICE data-segment floats with no symbol/reference.
// FLAG: placeholder origin (the exact screen position is not recoverable).
static const f32 KF_ICE_BAR_BASE_X = 40.0f;
static const f32 KF_ICE_BAR_BASE_Y = 40.0f;

// Status-line text size (the editor sub-take name caption uses 16.0).
static const f32 KF_ICE_STATUS_TEXT_SIZE = 16.0f;

// The look-at target box is built from the take's element-description indices 3/4/5
// (the look-at X/Y/Z target-offset elements) sampled at the resolved key.
static const s32 KI_ICE_ELEM_LOOKAT_X = 3;
static const s32 KI_ICE_ELEM_LOOKAT_Y = 4;
static const s32 KI_ICE_ELEM_LOOKAT_Z = 5;

// The take element whose decoded value selects the look-at reference space.
static const s32 KI_ICE_ELEM_LOOKAT_SPACE = 31;

// The backdrop poly + segment/marker box COLOURS. PLACEHOLDER: the editor's box
// colours live in ICE data-segment vectors selected by an editor take query (a small
// 0/1 index into per-style colour pairs) and carry no symbol / no reference, so the
// exact RGBA values are not recoverable. They are modelled as neutral file-local
// placeholders; the colour-selection structure is reproduced faithfully (see
// IceBarMarkerColour below).
// FLAG: placeholder colours -- exact RGBA not recoverable.
static const Vector4 KV_ICE_COLOUR_BACKDROP = { 0.0f, 0.0f, 0.0f, 0.0f };
static const Vector4 KV_ICE_COLOUR_SEGMENT  = { 1.0f, 1.0f, 1.0f, 1.0f };
static const Vector4 KV_ICE_COLOUR_SELECTED = { 1.0f, 1.0f, 0.0f, 1.0f };

// ----------------------------------------------------------------------------
// Editor take query helper (the unnamed `this+0x28` take predicate the bar fill
// uses to pick a marker box colour by style). The full arg list of the underlying
// take query is uncertain (the draw path passes the edit take plus spilled element/
// key registers the decompiler dropped), and it only feeds the placeholder colour
// selection above, so it is modelled as a file-local helper returning the small 0/1
// style index the colour table is keyed by.
// FLAG: placeholder -- returns a fixed style index; the real per-element editor query
//       is not recoverable from the draw path alone.
// ----------------------------------------------------------------------------
static s32 IceBarMarkerStyle(const ICETake& /*lrEditTake*/)
{
    return 0;
}

// Pick the marker box colour for a segment given its style index and whether the
// segment is the selected/highlighted one. PLACEHOLDER selection over the placeholder
// colours above (reproduces the "selected vs normal" split the draw path encodes).
// FLAG: placeholder colour selection.
static const Vector4& IceBarMarkerColour(s32 /*liStyle*/, bool lbSelected)
{
    return lbSelected ? KV_ICE_COLOUR_SELECTED : KV_ICE_COLOUR_SEGMENT;
}

// ----------------------------------------------------------------------------
// Resolve the read-out key edge for the current edit state: the END-key states
// (END_KEY_SELECTED / EDIT_END_KEY / ASSEMBLY_EDIT_END) bias the key by +1; the
// SELECT_COPY_MODE state defers to ICEAuthor::IsEndKeyState with the state argument.
// Returns 1 when the END edge applies, 0 otherwise. Shared by RenderBars (case 12/13)
// and RenderTargetBox.
//
// FLAG (ICEAuthor base): IsEndKeyState is an ICEAuthor base method invoked on `this`
// through the author cast (the controller shares the author's `this`).
// ----------------------------------------------------------------------------
static s32 IceResolveEndKeyEdge(ICEController* lpController, s32 liState, s32 liStateArg)
{
    if (liState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)              // 16
    {
        ICEAuthor* lpAuthor = reinterpret_cast<ICEAuthor*>(lpController); // is-a author
        return lpAuthor->IsEndKeyState(liStateArg) ? 1 : 0;
    }
    if (liState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED              // 13
        || liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY              // 15
        || liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END)         // 18
    {
        return 1;
    }
    return 0;
}

// ============================================================================
// ICEController::Render
//
// The top-level editor overlay. No-op unless the editor owns the camera
// (miState > 0). It always draws the full-screen backdrop poly into the editor
// render context (mRenderContext, passed by address); then, when the "show UI"
// toggle (miShowUi) is set, it draws the status caption (only in status mode 2 with
// a live count), the animated logo + widescreen marker, the time bars, the menus,
// and -- in the EDIT_START_KEY / EDIT_END_KEY states with the aux flag set -- the
// look-at target box.
// ============================================================================
void ICEController::Render()
{
    if (miState <= 0)
        return;

    // The backdrop quad the editor draws behind the overlay. The four poly verts +
    // the two colour args are assembled from the backdrop colour; the committed
    // RenderPoly reads vertex 0 (the corner) and a derived extent. PLACEHOLDER
    // geometry: the exact full-screen corner span is an inline data-segment vector
    // with no symbol -- modelled as a degenerate (zeroed) poly so the call shape and
    // colour are faithful. FLAG: placeholder backdrop poly geometry.
    Poly lBackdrop;
    for (s32 liVert = 0; liVert < 4; ++liVert)
    {
        lBackdrop.maVertices[liVert].x = 0.0f;
        lBackdrop.maVertices[liVert].y = 0.0f;
        lBackdrop.maVertices[liVert].z = 0.0f;
        lBackdrop.maVertices[liVert].w = 0.0f;
    }

    // The editor render context is passed BY ADDRESS to the static draw entry points
    // (the editor threads `this + mRenderContext` as the draw context). The committed
    // all-static RenderPoly ignores its localWorld/context slot.
    // FLAG: the render context is an opaque editor draw-context word; the committed
    //       static draw path does not read it, so it is mapped to the ignored slot.
    (void)mRenderContext;
    ICERender::RenderPoly(&lBackdrop, &KV_ICE_COLOUR_BACKDROP,
                          &KV_ICE_COLOUR_BACKDROP, /*localWorld*/ 0);

    if (!miShowUi)
        return;

    // Status caption: only when a status line is live (count > 0) and the status mode
    // selects the print (mode == 2). The status text base is miStatusText (the buffer
    // at +0x720C); the render context rides as the first ScrShadowPrintf arg (the
    // committed convention used by the widget Render bodies).
    if (miStatusCount > 0 && miStatusMode == 2)
    {
        const char* lpcStatus = reinterpret_cast<const char*>(&miStatusText);
        ICERender::ScrShadowPrintf((s32)mRenderContext, 150,
                                   KF_ICE_STATUS_TEXT_SIZE, "%s", lpcStatus);
    }

    // The animated logo and the widescreen/letterbox marker (both 2D screen-space) are
    // TWO SEPARATE widget members (mLogo @+0x71E4, mWideScrMarker @+0x71E0; see
    // ICEController.hpp). Each is drawn through its own committed widget Render entry
    // point against the editor render context. The logo is drawn first, then the marker.
    mLogo.Render((s32)mRenderContext);
    mWideScrMarker.Render(reinterpret_cast<ICERender*>(&mRenderContext));

    // Snapshot the state across the bar/menu draws (the target-box gate compares the
    // state captured before the menu draw).
    const s32 liStateBeforeMenus = miState;

    RenderBars();
    RenderMenus();

    // The 3D look-at target box: only in the EDIT_START_KEY / EDIT_END_KEY states
    // (14 / 15) and only when the aux edit toggle is set.
    if (liStateBeforeMenus == E_ICE_AUTHOR_STATE_EDIT_END_KEY            // 15
        || liStateBeforeMenus == E_ICE_AUTHOR_STATE_EDIT_START_KEY)      // 14
    {
        if (miFlagAux)
            RenderTargetBox();
    }
}

// ============================================================================
// ICEController::RenderBars
//
// Fill and draw the two editor timeline bars and the read-out info list.
//
// Structure (per the draw path):
//   1. Reset the main time bar.
//   2. When the status flag is set, print the current sub-take name caption (its
//      source depends on the editor state: the assembly source in ASSEMBLY_MARK,
//      the live sub-take name otherwise, "Invalid"/"Not Selected" fallbacks).
//   3. Resolve the channel to draw markers for (channel 0 when status, else the
//      current channel), look up its marker count, and clamp the selected marker.
//   4. Walk the channel's marker-description table; for each live marker compute its
//      normalised time (the quantized u16 interval key * 1/65535), push it into the
//      bar's marker slot with a style-selected colour, and flag hard-cut boundaries.
//   5. Compute the bar's time window / domain from the take parameter + length and
//      hand the (time,total) read-out to SetTimeInfo.
//   6. Repeat the per-marker fill for the secondary/assembly bar.
//   7. Refresh the read-out info list, set the per-state read-out lines, then draw
//      the info list and both bars.
//
// FLAG: the per-channel marker-description table, the element-index map and the
//       segment box colour tables are unrecoverable data-segment tables -> the
//       per-marker geometry/colour fill is summarised against the file-local
//       placeholders above. The control flow, the time/parameter maths and the
//       widget API calls are reconstructed faithfully.
// ============================================================================
void ICEController::RenderBars()
{
    ICEAuthor* lpAuthor = reinterpret_cast<ICEAuthor*>(this); // is-a author

    const s32 liState = miState;

    // (1) Clear the main bar back to its empty state.
    mTimeBar.Reset();

    // (2) Status caption: the current sub-take name (+ a category line). Drawn only
    // when the status flag (the time-bar's auxiliary read-out flag) is set.
    if (mTimeBar.mbShowAux)
    {
        // The category sub-line (held in the sub-take source record). PLACEHOLDER:
        // the record's name field offset is reached through the sub-take source
        // pointer; modelled as the source's own name.
        // FLAG: placeholder category text (source-record name-field offset inferred).
        const char* lpcCategory = mpSubTakeSource
            ? reinterpret_cast<const char*>(mpSubTakeSource)
            : "";

        const char* lpcName;
        if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK)                 // 8
        {
            // Assembly-mark: the assembly edit-source take's name, or "Not Selected".
            // The source is the author-base sub-take data pointer (controller +0x18).
            // FLAG (ICEAuthor base): mpSubTakeData lives on the ICEAuthor base; reached
            //       through the author cast until the base lands on the controller.
            const ICETakeData* lpAssemblySource = lpAuthor->mpSubTakeData;
            lpcName = lpAssemblySource
                ? reinterpret_cast<const char*>(lpAssemblySource)
                : "Not Selected";
        }
        else if (lpAuthor->GetCurrentSubtakeGuid() == -1)
        {
            lpcName = "Invalid";
        }
        else
        {
            // The live sub-take name reached through the sub-take source virtual
            // (mpSubTakeSource). PLACEHOLDER name-field offset.
            // FLAG: placeholder sub-take name text.
            lpcName = mpSubTakeSource
                ? reinterpret_cast<const char*>(mpSubTakeSource)
                : "";
        }

        ICERender::ScrShadowPrintf((s32)(KF_ICE_BAR_BASE_X + 2.0f), 16,
                                   KF_ICE_STATUS_TEXT_SIZE, "%s", lpcName);
        ICERender::ScrShadowPrintf((s32)(KF_ICE_BAR_BASE_X + 2.0f),
                                   (s32)(KF_ICE_BAR_BASE_Y + 55.0f),
                                   KF_ICE_STATUS_TEXT_SIZE, "%s", lpcCategory);

        // Clear the bar's auxiliary read-out gate after the caption.
        mTimeBar.mbShowAux = 0;
    }

    // (3) The channel to draw markers for: channel 0 while the status flag is set,
    // otherwise the current channel.
    const s32 liBarChannel = mTimeBar.mbShowAux ? 0 : miCurrentChannel;

    const u16 lu16NumIntervals  = mEditTake.GetNumIntervals(liBarChannel);
    u16       lu16SelectedKey   = mEditTake.GetCurrentInterval(liBarChannel);
    if (lu16SelectedKey >= lu16NumIntervals)
        lu16SelectedKey = 0;

    // No intervals on this channel: push a single zero-time marker and skip the walk.
    if (lu16NumIntervals == 0)
    {
        if (mTimeBar.miNumMarkers < KI_ICE_TIMEBAR_MAX_MARKERS)
        {
            mTimeBar.maMarkers[mTimeBar.miNumMarkers].mfTime = 0.0f;
            ++mTimeBar.miNumMarkers;
        }
        lu16SelectedKey = 0;
    }
    else
    {
        // (4) Per-marker fill. PLACEHOLDER: the marker-description walk (which take
        // elements get a marker, in what order) drives off the unrecoverable per-
        // channel description table + element-index map. Reconstructed as a straight
        // walk over the channel's intervals: each interval's normalised time is the
        // quantized key parameter * 1/65535; the box colour is style-selected; a
        // hard-cut boundary flags the keyframe sub-box.
        // FLAG: the description table / element-index map are not recoverable; the
        //       walk + colour selection are summarised against the placeholders.
        const s32 liStyle = IceBarMarkerStyle(mEditTake);
        (void)liStyle;

        for (u16 lu16Interval = 0; lu16Interval < lu16NumIntervals; ++lu16Interval)
        {
            f32 lfTime;
            if (lu16Interval == 0)
            {
                lfTime = 0.0f;
            }
            else if (lu16Interval < lu16NumIntervals)
            {
                const u16 lu16Key = mEditTake.GetIntervalKey(liBarChannel,
                                                             (u16)(lu16Interval - 1));
                lfTime = (f32)lu16Key * KF_ICE_PARAM_UNPACK;
            }
            else
            {
                lfTime = 1.0f;
            }

            if (mTimeBar.miNumMarkers < KI_ICE_TIMEBAR_MAX_MARKERS)
            {
                const s32  liSlot     = mTimeBar.miNumMarkers;
                const bool lbSelected = (liState >= E_ICE_AUTHOR_STATE_SCRUBBER) // >= 7
                                        && (lu16Interval == lu16SelectedKey);

                ICEWidgetTimeBarMarker& lrMarker = mTimeBar.maMarkers[liSlot];
                lrMarker.mfTime = lfTime;

                // Seed the four poly/colour lanes from the style-selected colour.
                const Vector4& lrColour = IceBarMarkerColour(liStyle, lbSelected);
                lrMarker.maPolyA[0] = lrColour.x; lrMarker.maPolyA[1] = lrColour.y;
                lrMarker.maPolyA[2] = lrColour.z; lrMarker.maPolyA[3] = lrColour.w;

                // A hard-cut interval boundary draws the keyframe sub-box.
                if (mEditTake.IsHardCut(liBarChannel, (s32)lu16Interval))
                    lrMarker.miIsKeyframe = 1;

                ++mTimeBar.miNumMarkers;
            }
        }
    }

    // (5) The bar's time window + domain. The take parameter is the playback param
    // (or the sub-parameter in the assembly-edit states); the take length scales it
    // to the read-out frame count.
    const f32 lfBarParam = (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT          // 9
                            || liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING) // 10
                           ? mEditTake.GetSubParameter()
                           : mEditTake.GetParameter();

    const ICETakeData* lpTakeData = mEditTake.GetData();
    const f32 lfTakeLength = lpTakeData ? lpTakeData->GetLength() : 0.0f;

    // The play-head normalised time of the selected key.
    f32 lfSelectedTime;
    if (lu16SelectedKey == 0)
        lfSelectedTime = 0.0f;
    else if (lu16SelectedKey < lu16NumIntervals)
        lfSelectedTime = (f32)mEditTake.GetIntervalKey(liBarChannel, lu16SelectedKey)
                         * KF_ICE_PARAM_UNPACK;
    else
        lfSelectedTime = 1.0f;
    (void)lfSelectedTime;

    // Hand the (current-time, total-time) read-out to the main bar.
    mTimeBar.SetTimeInfo(lfBarParam, lfTakeLength * KF_ICE_FRAME_RATE + KF_ICE_ROUND_BIAS);

    // (6) The secondary / assembly bar: Reset + the same per-marker fill (channel 10
    // = the assembly channel in the assembly-edit states, else channel 0). Summarised
    // against the same placeholders as the main bar.
    // FLAG: secondary-bar marker fill summarised (same unrecoverable tables).
    mTimeBar2.Reset();
    const s32 liSecondChannel = (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING)
                                ? 10 : 0;
    const u16 lu16Num2 = mEditTake.GetNumIntervals(liSecondChannel);
    if (lu16Num2 == 0)
    {
        if (mTimeBar2.miNumMarkers < KI_ICE_TIMEBAR_MAX_MARKERS)
        {
            mTimeBar2.maMarkers[mTimeBar2.miNumMarkers].mfTime = 0.0f;
            ++mTimeBar2.miNumMarkers;
        }
    }
    else
    {
        for (u16 lu16Interval = 0; lu16Interval < lu16Num2; ++lu16Interval)
        {
            if (mTimeBar2.miNumMarkers >= KI_ICE_TIMEBAR_MAX_MARKERS)
                break;
            const f32 lfTime = (lu16Interval == 0)
                ? 0.0f
                : (f32)mEditTake.GetIntervalKey(liSecondChannel,
                                                (u16)(lu16Interval - 1)) * KF_ICE_PARAM_UNPACK;
            mTimeBar2.maMarkers[mTimeBar2.miNumMarkers].mfTime = lfTime;
            ++mTimeBar2.miNumMarkers;
        }
    }
    if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT)                     // 9
        mTimeBar2.SetTimeInfo(mEditTake.GetParameter(), lfTakeLength * KF_ICE_FRAME_RATE);

    // (7) Refresh + draw the read-outs and both bars.
    RefreshInfoList();

    // Per-state read-out line flags. Each editor state lights a different combination
    // of the time-bar / info-list read-out flags (the play head, the selection span,
    // the loop markers). The flag combinations are reconstructed from the draw path;
    // the individual read-out scratch fields they set are the time-bar's named flags.
    switch (liState)
    {
    case E_ICE_AUTHOR_STATE_PLAYING_ANIMATION:                          // 5
    case E_ICE_AUTHOR_STATE_SCRUBBER:                                   // 7
        mTimeBar.mbHasSelection = 1;
        mTimeBar.mbShowIcon     = 1;
        break;
    case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:                              // 8
        mTimeBar.mbHasSelection = 1;
        mTimeBar.mbShowIcon     = 1;
        break;
    case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:                              // 9
    case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING:                      // 10
        mTimeBar.mbHasSelection = 1;
        mTimeBar.mbShowIcon     = 1;
        // The assembly read-out additionally checks element 47 (the assembly-loop
        // flag) to decide whether the loop marker is shown.
        if (mEditTake.GetValueInt(47) != 0)
            mTimeBar.mbHasSelection = 1;
        break;
    case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:                          // 11
        mTimeBar.mbShowIcon = 1;
        break;
    case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:                        // 12
    case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:                          // 13
        mTimeBar.mbShowIcon = 1;
        // The key-selected read-out marks the selected key and the end-edge flag.
        {
            const s32 liEndEdge = IceResolveEndKeyEdge(this, miState, miStateArg);
            mTimeBar.miMarkerSelA = (s32)lu16SelectedKey;
            mTimeBar.miMarkerSelB = liEndEdge;
        }
        break;
    case E_ICE_AUTHOR_STATE_EDIT_START_KEY:                            // 14
    case E_ICE_AUTHOR_STATE_EDIT_END_KEY:                              // 15
        mTimeBar.mfSelFlagB    = 1.0f;
        mTimeBar.mbLoopActive  = (liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY) ? 1 : 0;
        mTimeBar.mbShowIcon    = 1;
        break;
    case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START:                       // 17
    case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END:                         // 18
        mTimeBar.mbHasSelection = 1;
        mTimeBar.mbShowIcon     = 1;
        break;
    default:
        break;
    }

    mInfoList.Render((s32)KF_ICE_BAR_BASE_X);
    mTimeBar.Render((s32)mRenderContext);
    mTimeBar2.Render((s32)mRenderContext);
}

// ============================================================================
// ICEController::RenderMenus
//
// Draw the menu widget(s) the editor state selects. Each menu is refreshed (its
// cells filled by RefreshMenu) just before it is drawn. The state->menu mapping:
//   BROWSER (1)          -> main menu (refresh kind 0)
//   DELETE_CONFIRM (3)   -> main menu (kind 0) + cancel/delete menu (kind 5)
//   EXIT_CONFIRM (2)     -> commit/discard menu (kind 4)
//   INTERVAL_SETTINGS(11)-> interval main menu (kind 1)
//   SELECT_COPY_MODE(16) -> copy-elements menu (kind 3)
// ============================================================================
void ICEController::RenderMenus()
{
    switch (miState)
    {
    case E_ICE_AUTHOR_STATE_BROWSER:                                    // 1
    case E_ICE_AUTHOR_STATE_DELETE_CONFIRM:                            // 3
        RefreshMenu(0);
        mpMenuMain->Render((s32)mRenderContext);
        if (miState == E_ICE_AUTHOR_STATE_DELETE_CONFIRM)
        {
            RefreshMenu(5);
            mpMenuCancel->Render((s32)mRenderContext);
        }
        break;
    case E_ICE_AUTHOR_STATE_EXIT_CONFIRM:                              // 2
        RefreshMenu(4);
        mpMenuAssembly->Render((s32)mRenderContext);
        break;
    case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:                         // 11
        RefreshMenu(1);
        mpMenuIntervalMain->Render((s32)mRenderContext);
        break;
    case E_ICE_AUTHOR_STATE_SELECT_COPY_MODE:                         // 16
        RefreshMenu(3);
        mpMenuCommit->Render((s32)mRenderContext);
        break;
    default:
        break;
    }
}

// ============================================================================
// ICEController::RenderTargetBox
//
// Place the editor's 3D look-at target box at the take's decoded look-at point and
// draw it. The look-at point is the take's element 3/4/5 (look-at X/Y/Z target
// offset) sampled at the resolved key, transformed by the reference-space matrix the
// camera-space handler hands back for the take's look-at space (decoded element 31).
//
// The transform is:  worldPoint = wAxis + xAxis*p.x + yAxis*p.y + zAxis*p.z
// (the reference-space affine's rows times the look-at offset), and the box widget's
// transform is filled with the reference-space rows + the transformed point as its
// translation row, then drawn.
// ============================================================================
void ICEController::RenderTargetBox()
{
    ICEAuthor* lpAuthor = reinterpret_cast<ICEAuthor*>(this); // is-a author

    // The look-at reference space is the take's decoded element 31.
    const s32 liSpace = mEditTake.GetValueInt(KI_ICE_ELEM_LOOKAT_SPACE);

    // The reference-space transform for that space (the camera-space handler returns a
    // 4-row affine by value). mpCameraSpaceHandler is the author-base camera-space
    // handler pointer (controller +0x08).
    // FLAG (ICEAuthor base): mpCameraSpaceHandler lives on the ICEAuthor base; reached
    //       through the author cast until the base lands on the controller layout.
    const Matrix44Affine lTransform =
        lpAuthor->mpCameraSpaceHandler->GetTransformToWorld((eICESpace)liSpace);

    // Resolve the look-at key edge for the current state (the END-key states bias the
    // key by +1), then add it to the channel's current interval key.
    const s32 liChannel = miCurrentChannel;

    // The current interval's key for this channel (0 when the channel has no keys).
    u16 lu16Key = 0;
    const u16 lu16Interval = mEditTake.GetCurrentInterval(liChannel);
    if (lu16Interval != 0)
    {
        const u16 lu16NumIntervals = mEditTake.GetNumIntervals(liChannel);
        if ((u16)(lu16Interval + 1) < lu16NumIntervals)
            lu16Key = mEditTake.GetIntervalKey(liChannel, lu16Interval);
        else
            lu16Key = (u16)(mEditTake.GetNumKeys(liChannel) - 2);
    }

    const s32 liEndEdge = IceResolveEndKeyEdge(this, miState, miStateArg);
    const u16 lu16SampleKey = (u16)(lu16Key + liEndEdge);

    // Sample the look-at target offset (elements 3/4/5) at the resolved key.
    Vector4 lLookAt;
    lLookAt.x = mEditTake.GetValueFloat(KI_ICE_ELEM_LOOKAT_X, lu16SampleKey);
    lLookAt.y = mEditTake.GetValueFloat(KI_ICE_ELEM_LOOKAT_Y, lu16SampleKey);
    lLookAt.z = mEditTake.GetValueFloat(KI_ICE_ELEM_LOOKAT_Z, lu16SampleKey);
    lLookAt.w = 0.0f;

    // Fill the box widget's transform: the reference-space rotation rows verbatim and
    // the transformed look-at point as the translation row (wAxis + R * lookAt). The
    // widget draws a fixed +/-0.05 cube at this point.
    mTargetBox.mTransform.xAxis = lTransform.xAxis;
    mTargetBox.mTransform.yAxis = lTransform.yAxis;
    mTargetBox.mTransform.zAxis = lTransform.zAxis;

    Vector3& lrTranslation = mTargetBox.mTransform.wAxis;
    lrTranslation.x = lTransform.wAxis.x
                      + lTransform.xAxis.x * lLookAt.x
                      + lTransform.yAxis.x * lLookAt.y
                      + lTransform.zAxis.x * lLookAt.z;
    lrTranslation.y = lTransform.wAxis.y
                      + lTransform.xAxis.y * lLookAt.x
                      + lTransform.yAxis.y * lLookAt.y
                      + lTransform.zAxis.y * lLookAt.z;
    lrTranslation.z = lTransform.wAxis.z
                      + lTransform.xAxis.z * lLookAt.x
                      + lTransform.yAxis.z * lLookAt.y
                      + lTransform.zAxis.z * lLookAt.z;
    lrTranslation.w = 0.0f;

    // Draw the box. The editor threads its render context to the widget; the committed
    // ICEWidgetTargetBox::Render builds its own debug-render context, so it takes no
    // argument here.
    // FLAG: the editor passes its render context (controller +0xF6C) to the widget;
    //       the committed target-box Render ignores it (it acquires the 3D debug
    //       renderer itself), so no context is forwarded.
    mTargetBox.Render();
}

} // namespace ICE
