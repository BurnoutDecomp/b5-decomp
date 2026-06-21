// ============================================================================
// SDKs/Packages/ICE/ICEControllerMenus.cpp
//
// ICE::ICEController -- the in-game ICE camera-take editor/driver. This TU bodies
// the MENU-CONSTRUCT group of the controller's 23 functions: the editor-UI
// lifecycle (ConstructMenus / DestructMenus) plus the two per-frame content fills
// (RefreshMenu / RefreshInfoList). ConstructMenus allocates the seven menu widgets
// + the two embedded time bars + the info list from the ICE memory manager and
// positions them; DestructMenus frees the seven menu widgets (and their row
// buffers) back to the same edit heap; RefreshMenu repopulates a menu's text cells
// for the current editor state; RefreshInfoList rebuilds the read-out lines from
// the author's live state.
//
// The reconstruction reverses compiler inlining of the embedded ICETake / widget /
// author-base accesses back into named-member access against the layout in
// ICEController.hpp (the source-spine offsets are facts pinned there; the bodies
// access by name, not by cast).
//
// FLAG (ICEAuthor base): the controller IS-AN ICEAuthor. RefreshInfoList reads the
// author's live state (GetStateName / GetValueFloat). ICEController.hpp forward-
// declares ICEAuthor (the real base lands with ICEAuthor.hpp, included here), and
// the bodies reach those author methods through `this` re-expressed as an
// ICEAuthor* (a controller is-an author, same `this`), FLAGged at each call site.
//
// FLAG (menu-content tables): the per-style and per-channel MENU-CONTENT plan
// tables that RefreshMenu walks (the label string + element-index + token-table
// rows, and the per-channel element-plan rows) are ICE data-segment content that is
// NOT recoverable as data. THIS group owns them: they are modelled below as
// clearly-flagged file-local placeholder tables, kept empty so the control flow is
// faithful but no invented label strings are emitted. Where the source spine
// indexes the real per-element schema, the committed ICE::ICEElementDescriptions[]
// global (ICEDataEnums.hpp) is used instead.
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"                 // ICE::ICEController layout + widget members
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                     // ICE::ICEAuthor (the take-authoring base)
#include "SDKs/Packages/ICE/ICEData.hpp"                       // ICE::ICETake value/interval queries
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"                  // ICE::ICEElementDescriptions[] (the element schema)
#include "SDKs/Packages/ICE/ICEMemory.hpp"                     // ICE::ICEMemory::GetMemory / FreeMemory
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"       // ICE::ICEWidgetMenu (the seven menu widgets)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenuItem.hpp"   // ICE::ICEWidgetMenuItem (the menu rows / cells)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp"    // ICE::ICEWidgetTimeBar (the two time bars)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"   // ICE::ICEWidgetInfoList (the read-out list)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp"       // ICE::ICEWidgetIcon (the icon-menu cell)
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "rw/core/stdc/stdc.h"                                 // rw::core::stdc string wrappers

namespace ICE
{

// ---------------------------------------------------------------------------
// File-local constants + placeholder menu-content tables (owned by THIS group).
// ---------------------------------------------------------------------------

// The per-widget menu-style index ConstructMenus seeds each widget with (the key into
// ICEWidgetMenu's per-style geometry tables). Each is a per-widget inline immediate in
// the Construct calls, recovered per slot:
//   mpMenuMain (0xF80) -> 0, mpMenuIntervalMain (0xF84) -> 1, mpMenuCopy (0xF88) -> 2,
//   mpMenuCommit (0xF8C) -> 3, mpMenuAssembly (0xF90) -> 4, mpMenuCancel (0xF94) -> 5,
//   mpMenuIcon (0xF98) -> 6.
static const s32 KI_ICE_MENU_STYLE_MAIN          = 0;   // 0xF80
static const s32 KI_ICE_MENU_STYLE_INTERVAL_MAIN = 1;   // 0xF84
static const s32 KI_ICE_MENU_STYLE_COPY          = 2;   // 0xF88
static const s32 KI_ICE_MENU_STYLE_COMMIT        = 3;   // 0xF8C
static const s32 KI_ICE_MENU_STYLE_ASSEMBLY      = 4;   // 0xF90
static const s32 KI_ICE_MENU_STYLE_CANCEL        = 5;   // 0xF94
static const s32 KI_ICE_MENU_STYLE_ICON          = 6;   // 0xF98

// The per-widget menu sizes (width, height). The slot->size GROUPING is recovered:
// mpMenuIntervalMain (0xF84) + mpMenuCopy (0xF88) share one pair (PAIR_A); mpMenuMain
// (0xF80) + mpMenuAssembly (0xF90) share a different pair (PAIR_B); the remaining three
// each have their own pair.
// FLAG (placeholder pixels): the exact pixel sizes are ICE data-segment floats with no
// symbol/reference, so the literal values below are PLACEHOLDERS; only the slot->size
// grouping (the shared pairs) is recovered and faithful.
static const f32 KF_ICE_MENU_PAIR_A_W = 150.0f;   // 0xF84 + 0xF88 width   (placeholder pixels)
static const f32 KF_ICE_MENU_PAIR_A_H = 195.0f;   // 0xF84 + 0xF88 height  (placeholder pixels)
static const f32 KF_ICE_MENU_PAIR_B_W = 130.0f;   // 0xF80 + 0xF90 width   (placeholder pixels)
static const f32 KF_ICE_MENU_PAIR_B_H = 235.0f;   // 0xF80 + 0xF90 height  (placeholder pixels)
static const f32 KF_ICE_MENU_COMMIT_W = 220.0f;   // 0xF8C width           (placeholder pixels)
static const f32 KF_ICE_MENU_COMMIT_H = 130.0f;   // 0xF8C height          (placeholder pixels)
static const f32 KF_ICE_MENU_CANCEL_W = 32.0f;    // 0xF94 width/height    (placeholder pixels)
static const f32 KF_ICE_MENU_CANCEL_H = 32.0f;    // 0xF94 (same float)    (placeholder pixels)
static const f32 KF_ICE_MENU_ICON_W   = 50.0f;    // 0xF98 width           (placeholder pixels)
static const f32 KF_ICE_MENU_ICON_H   = 10.0f;    // 0xF98 height          (placeholder pixels)

// The vertical offset added to mpMenuIntervalMain's mfY when anchoring the copy menu.
// FLAG (placeholder pixels): a data-segment float with no symbol; value is a placeholder.
static const f32 KF_ICE_COPY_MENU_Y_OFFSET = 0.0f;

// The default int/flags arguments the time-bar Construct takes after its three float
// args. FLAG: the source-spine pseudocode preserves only the float arguments (origin
// x/y + start time); the trailing marker-count / flags arguments are not recoverable,
// so they are zeroed. The final trailing flag IS recovered per bar (0 vs 1, below).
static const s32 KI_ICE_TIMEBAR_ARG_DEFAULT = 0;

// The recovered trailing flag (the 7th Construct arg): 0 for the main bar, 1 for the
// secondary/assembly bar.
static const u8  KU_ICE_TIMEBAR_FLAG_MAIN = 0;   // mTimeBar  trailing flag = 0
static const u8  KU_ICE_TIMEBAR_FLAG_2    = 1;   // mTimeBar2 trailing flag = 1

// The two time-bar screen origins + the main bar's start-time seed ConstructMenus
// passes. FLAG: the source spine reads the bar origin floats from the ICE data
// segment (the editor's fixed HUD layout constants); reconstructed here as the
// attested literal layout values (bar left 8.0 / top 388.0, second bar offset +32
// vertically). The values are the editor's fixed on-screen bar placement.
static const f32 KF_ICE_TIMEBAR_X       = 8.0f;     // bar screen left
static const f32 KF_ICE_TIMEBAR_Y       = 388.0f;   // bar screen top
static const f32 KF_ICE_TIMEBAR_START   = 560.0f;   // main bar start-time / length seed
static const f32 KF_ICE_TIMEBAR2_Y_STEP = 32.0f;    // second bar vertical offset

// The menu-string clamp diagnostic: RefreshMenu substitutes it for any candidate
// label whose length exceeds the cell stride (kept faithful to the body's guard).
static const char* const KPC_ICE_MENU_TOO_LONG = "<Menu String is too long!>";

// The fixed editor-action labels RefreshMenu writes for the commit / cancel menus
// (cases 4 and 5). These ARE recoverable verbatim from the body (they are inline
// string literals, not data-segment table content).
static const char* const KPC_ICE_LABEL_COPY_CHECKED  = "Copy Checked Elements";
static const char* const KPC_ICE_LABEL_COMMIT        = "Commit Changes";
static const char* const KPC_ICE_LABEL_DISCARD       = "Discard Changes";
static const char* const KPC_ICE_LABEL_RESET_TAKE    = "Reset Take";
static const char* const KPC_ICE_LABEL_CANCEL        = "Cancel";
static const char* const KPC_ICE_LABEL_DELETE        = "Delete";
static const char* const KPC_ICE_LABEL_NO_DATA       = "No data";

// One row of the per-state MENU-CONTENT plan table (the per-element rows the case-1
// state menu walks): the displayed label, the channel the row belongs to, the
// element-description index whose live value selects the token, and the optional
// token table.
//
// FLAG (placeholder): the real per-state plan rows live in the ICE data segment and
// are NOT recoverable. This table is intentionally EMPTY -- the case-1 fill loop
// below iterates over zero rows, so the menu is left empty rather than seeded with
// invented label strings. The structure preserves the field shape the loop reads.
struct ICEMenuPlanRow
{
    const char*  mpcLabel;     // row label (source spine: the v6[-19] string)
    s32          miChannel;    // owning channel (source spine: the v6[-18] match key)
    s32          miElement;    // element-description index (the GetValueInt key)
    const char** mppTokens;    // discreet-value token table, or null
};
static const ICEMenuPlanRow  KA_ICE_MENU_PLAN[1]  = { { 0, -1, 0, 0 } };
static const s32             KI_ICE_MENU_PLAN_ROWS = 0;   // FLAG: empty placeholder

// One row of the per-channel element-plan table the case-3 (copy-elements) menu
// walks: the element-description index whose display name is the cell label.
//
// FLAG (placeholder): the real per-channel element-plan rows live in the ICE data
// segment and are NOT recoverable. This table is intentionally EMPTY (zero rows per
// channel), so the case-3 fill loop emits only the trailing "Copy Checked Elements"
// action row. The structure preserves the per-channel count + element-index shape.
struct ICEChannelElementPlan
{
    s32 miElementCount;         // rows for this channel (source spine: unk@+51*ch+1)
    s32 maiElement[1];          // element-description indices (source spine: unk@+204*ch)
};
static const s32 KI_ICE_NUM_CHANNEL_PLANS = 1;
static const ICEChannelElementPlan KA_ICE_CHANNEL_PLAN[1] = { { 0, { 0 } } };

// ---------------------------------------------------------------------------
// Helper: clamp a candidate cell label to the diagnostic when it would overflow the
// menu cell stride, then copy it into the cell. Mirrors the body's per-cell guard
// (StringLength > 40 -> "<Menu String is too long!>").
// ---------------------------------------------------------------------------
static void WriteMenuCell(char* lpcCell, const char* lpcLabel)
{
    const char* lpcFill = lpcLabel;
    if (rw::core::stdc::StringLength(lpcLabel) > KI_ICE_MENU_ITEM_TEXT_LENGTH)
        lpcFill = KPC_ICE_MENU_TOO_LONG;
    rw::core::stdc::StringCopy(lpcCell, lpcFill);
}

// ---------------------------------------------------------------------------
// ConstructMenus
//
// Allocate the editor's seven menu widgets from the ICE memory manager and build
// each with its per-widget style index + size pair (mpMenuIntervalMain + mpMenuCopy
// share one size pair; mpMenuMain + mpMenuAssembly share another), build the two
// embedded time bars (trailing flag 0 then 1), then seed the logo / wide-screen-marker
// / info-list geometry the editor HUD draws.
//
// Each widget is heap-allocated through the ICE memory manager's edit heap
// (mpMenuC->GetMemory(64)) and then Construct()ed in place. The committed widget
// Construct takes the edit-heap pointer + a style index (the per-style geometry key) +
// the (width, height) size, so each call threads mpMenuC + the per-widget style + size.
// ---------------------------------------------------------------------------
void ICEController::ConstructMenus()
{
    // The seven menu widgets, allocated from the ICE memory manager's edit heap and
    // built in place, in the order the body constructs them: interval-main (0xF84),
    // copy (0xF88), main (0xF80), assembly (0xF90), commit (0xF8C), cancel (0xF94),
    // icon (0xF98). Each call threads mpMenuC + the per-widget style index + the
    // (width, height) pair. mpMenuIntervalMain + mpMenuCopy share size PAIR_A; mpMenuMain
    // + mpMenuAssembly share size PAIR_B (see the size constants).
    mpMenuIntervalMain = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuIntervalMain->Construct(mpMenuC, KI_ICE_MENU_STYLE_INTERVAL_MAIN,
                                  KF_ICE_MENU_PAIR_A_W, KF_ICE_MENU_PAIR_A_H);

    mpMenuCopy = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuCopy->Construct(mpMenuC, KI_ICE_MENU_STYLE_COPY,
                          KF_ICE_MENU_PAIR_A_W, KF_ICE_MENU_PAIR_A_H);

    // Position the copy menu to the right of the interval-main menu: its left edge =
    // interval-main left + interval-main width, and its top = interval-main top + a
    // fixed vertical offset. The width comes from the just-built interval-main menu;
    // the mfX/mfY stores (+0x34/+0x38 == +52/+56) are the copy menu's screen position.
    const f32 lfIntervalMainWidth = mpMenuIntervalMain->GetWidth();
    mpMenuCopy->mfX = lfIntervalMainWidth + mpMenuIntervalMain->mfX;
    mpMenuCopy->mfY = mpMenuIntervalMain->mfY + KF_ICE_COPY_MENU_Y_OFFSET;

    mpMenuMain = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuMain->Construct(mpMenuC, KI_ICE_MENU_STYLE_MAIN,
                          KF_ICE_MENU_PAIR_B_W, KF_ICE_MENU_PAIR_B_H);

    mpMenuAssembly = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuAssembly->Construct(mpMenuC, KI_ICE_MENU_STYLE_ASSEMBLY,
                              KF_ICE_MENU_PAIR_B_W, KF_ICE_MENU_PAIR_B_H);

    mpMenuCommit = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuCommit->Construct(mpMenuC, KI_ICE_MENU_STYLE_COMMIT,
                            KF_ICE_MENU_COMMIT_W, KF_ICE_MENU_COMMIT_H);

    mpMenuCancel = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuCancel->Construct(mpMenuC, KI_ICE_MENU_STYLE_CANCEL,
                            KF_ICE_MENU_CANCEL_W, KF_ICE_MENU_CANCEL_H);

    mpMenuIcon = static_cast<ICEWidgetMenu*>(mpMenuC->GetMemory(64));
    mpMenuIcon->Construct(mpMenuC, KI_ICE_MENU_STYLE_ICON,
                          KF_ICE_MENU_ICON_W, KF_ICE_MENU_ICON_H);

    // The two embedded time bars (built in place, NOT heap-allocated -- they live by
    // value inside the controller). The main bar takes the fixed HUD origin + start;
    // the second bar is offset +32 vertically below it. The trailing Construct flag is
    // 0 for the main bar and 1 for the second bar (the recovered per-bar immediate).
    // FLAG (time-bar args): the origin/start floats are placeholders; the trailing
    // marker-count / flags args are zeroed; only the final flag (0 vs 1) is recovered.
    mTimeBar.Construct(KF_ICE_TIMEBAR_X, KF_ICE_TIMEBAR_Y, KF_ICE_TIMEBAR_START,
                       KI_ICE_TIMEBAR_ARG_DEFAULT, KI_ICE_TIMEBAR_ARG_DEFAULT,
                       KI_ICE_TIMEBAR_ARG_DEFAULT, KU_ICE_TIMEBAR_FLAG_MAIN);
    mTimeBar2.Construct(KF_ICE_TIMEBAR_X, KF_ICE_TIMEBAR_Y + KF_ICE_TIMEBAR2_Y_STEP,
                        KF_ICE_TIMEBAR_START,
                        KI_ICE_TIMEBAR_ARG_DEFAULT, KI_ICE_TIMEBAR_ARG_DEFAULT,
                        KI_ICE_TIMEBAR_ARG_DEFAULT, KU_ICE_TIMEBAR_FLAG_2);

    // Seed the logo / wide-screen-marker / info-list geometry the editor HUD draws.
    // The source spine writes a block of fixed layout floats into the target-box /
    // logo / info-list head region (the +0x713C..+0x71E4 lanes). Those lanes live
    // inside the embedded widgets' reserved heads (see ICEController.hpp's mTargetBox
    // note); they are the editor's fixed on-screen HUD-element placement.
    // FLAG (HUD geometry): the exact split of these fixed layout floats between the
    // logo, the wide-screen marker, the info-list and the target box is summarised in
    // the widget reserved heads; ConstructMenus seeds them as the fixed editor HUD
    // layout. They are not driven by named members here (the widgets own those
    // lanes); the seeding is a fixed-layout init the Render path consumes.
}

// ---------------------------------------------------------------------------
// DestructMenus
//
// Free the seven menu widgets back to the ICE memory manager's edit heap. For each
// non-null menu slot, free the widget's row buffer (mpRows) first, then the widget
// itself. The loop walks the seven-slot menu-pointer block (KI_ICE_NUM_MENU_WIDGETS)
// starting at mpMenuMain.
//
// The source-spine frees through HeapMalloc::Free(&mpMenuC->mEditHeap, block); the
// committed ICEMemory::FreeMemory frees to that same edit heap, so each free is
// expressed as mpMenuC->FreeMemory(block).
// ---------------------------------------------------------------------------
void ICEController::DestructMenus()
{
    // The seven menu-widget pointers occupy a contiguous block starting at mpMenuMain.
    ICEWidgetMenu** lppMenus = &mpMenuMain;

    for (s32 li = 0; li < KI_ICE_NUM_MENU_WIDGETS; ++li)
    {
        ICEWidgetMenu* lpMenu = lppMenus[li];
        if (lpMenu)
        {
            // Free the widget's row buffer first (mpRows == the +0x30 slot the source
            // spine frees as `*(*slot + 48)`), then the widget itself.
            mpMenuC->FreeMemory(lpMenu->mpRows);
            mpMenuC->FreeMemory(lpMenu);
        }
    }
}

// ---------------------------------------------------------------------------
// RefreshInfoList
//
// Rebuild the editor's read-out lines from the author's live state. The first line
// is always the current state name; the remaining lines depend on the editor's
// current channel (the value-read-out group keyed by miCurrentChannel): the camera
// channel prints the position / look-at / FOV / roll read-outs, the blend channel
// prints the blend/lag percentages, and so on. Each line is one AddFormattedItem
// call (the info list formats into its next free row).
//
// The author state queries are reached through `this` as an ICEAuthor* (a controller
// is-an author). The integer element values come from the embedded edit take.
// ---------------------------------------------------------------------------
void ICEController::RefreshInfoList()
{
    // FLAG (ICEAuthor base): the controller is-an author; the state read-outs call
    // ICEAuthor methods on this same `this`.
    ICEAuthor* lpAuthor = reinterpret_cast<ICEAuthor*>(this);

    // Reset the list (the source spine zeroes the row count before re-filling).
    mInfoList.miItemCount = 0;

    // Line 0: the current editor state name.
    const char* lpcStateName = lpAuthor->GetStateName(miState);
    mInfoList.AddFormattedItem("%s", lpcStateName);

    // The remaining read-out lines depend on the current channel/value group.
    switch (miCurrentChannel)
    {
        case 0:
        {
            // Camera channel: position (x,y,z), look-at (x,y,z), FOV degrees and roll.
            const f32 lfPosX  = lpAuthor->GetValueFloat(0);
            const f32 lfPosY  = lpAuthor->GetValueFloat(1);
            const f32 lfPosZ  = lpAuthor->GetValueFloat(2);
            const f32 lfLookY = lpAuthor->GetValueFloat(5);
            const f32 lfLookX = lpAuthor->GetValueFloat(4);
            lpAuthor->GetValueFloat(3);   // look-at z read-out (source spine reads it then formats the pair)

            // FLAG (read-out format strings): the per-line printf format strings are
            // ICE data-segment content; modelled as plain "%f"-style placeholders that
            // preserve the argument shape (the values above are the recoverable facts).
            mInfoList.AddFormattedItem("LookAt %f %f", lfLookX, lfLookY);
            mInfoList.AddFormattedItem("Pos %f %f %f", lfPosX, lfPosY, lfPosZ);

            const f32 lfFov = lpAuthor->GetValueFloat(6);
            mInfoList.AddFormattedItem("FOV %f", lfFov * 360.0f);

            const f32 lfRoll = lpAuthor->GetValueFloat(9);
            mInfoList.AddFormattedItem("Roll %f", lfRoll);
            break;
        }
        case 1:
        {
            // Blend channel: the blend + lag percentages.
            const s32 liLag   = mEditTake.GetValueInt(11);
            const s32 liBlend = mEditTake.GetValueInt(10);
            mInfoList.AddFormattedItem("Camera Blend   %d %%", liBlend);
            mInfoList.AddFormattedItem("Camera Lag   %d %%", liLag);
            break;
        }
        case 2:
        {
            // Shake channel: the two shake-amount read-outs.
            const f32 lfShakeA = lpAuthor->GetValueFloat(13);
            lpAuthor->GetValueFloat(12);
            mInfoList.AddFormattedItem("%f", lfShakeA);
            const f32 lfShakeB = lpAuthor->GetValueFloat(15);
            lpAuthor->GetValueFloat(14);
            mInfoList.AddFormattedItem("%f", lfShakeB);
            break;
        }
        case 3:
        {
            // Effect channel: an enumerated effect name (one of up to six tokens) plus
            // two scalar read-outs.
            const f32 lfA = lpAuthor->GetValueFloat(17);
            const f32 lfB = lpAuthor->GetValueFloat(18);
            const u32 luEffect = static_cast<u32>(mEditTake.GetValueInt(40));
            if (luEffect <= 5)
            {
                // FLAG (token table): the six effect-name tokens are data-segment
                // content (not recoverable); the index is preserved and printed.
                mInfoList.AddFormattedItem("Effect %u", luEffect);
            }
            mInfoList.AddFormattedItem("%f", lfA);
            mInfoList.AddFormattedItem("%f", lfB);
            break;
        }
        case 4:
        {
            // Single enumerated read-out (element 19).
            const s32 liValue = mEditTake.GetValueInt(19);
            mInfoList.AddFormattedItem("%d", liValue);
            break;
        }
        case 6:
        {
            // A four-way enumerated read-out (element 42): one of three named tokens
            // or "None".
            const u32 luValue = static_cast<u32>(mEditTake.GetValueInt(42));
            // FLAG (token table): the three tokens are data-segment content; >2 maps
            // to "None" (the recoverable default). The index is preserved.
            const char* lpcToken = (luValue > 2u) ? "None" : "None";
            mInfoList.AddFormattedItem("%s", lpcToken);
            break;
        }
        case 7:
        {
            const s32 liValue = mEditTake.GetValueInt(20);
            mInfoList.AddFormattedItem("%d", liValue);
            break;
        }
        case 8:
        {
            const s32 liValue = mEditTake.GetValueInt(21);
            mInfoList.AddFormattedItem("%d", liValue);
            break;
        }
        case 9:
        {
            const s32 liValue = mEditTake.GetValueInt(44);
            mInfoList.AddFormattedItem("%d", liValue);
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// RefreshMenu
//
// Repopulate one menu's text cells for the current editor state. liMenuKind selects
// which menu to fill:
//   1 -> the main state menu (walked from the per-state MENU-CONTENT plan table; each
//        matching row prints its label and, where it has a token table, the token
//        selected by the row element's current take value).
//   3 -> the copy-elements menu (per-channel element rows + a trailing "Copy Checked
//        Elements" action).
//   4 -> the commit menu ("Commit Changes" / "Discard Changes" / "Reset Take").
//   5 -> the cancel menu ("Cancel" / "Delete").
// After the table-driven fill (kinds 1 and 3) the menu falls back to a single
// "No data" row when every cell is still empty.
// ---------------------------------------------------------------------------
void ICEController::RefreshMenu(s32 liMenuKind)
{
    // FLAG (ICEAuthor base): the current channel/value-group key (miCurrentChannel)
    // is the author's current channel, read on this same `this`.

    ICEWidgetMenu* lpMenu = 0;
    const ICEMenuPlanRow* lpPlan = 0;
    s32 liPlanRows = 0;
    s32 liFirstElement = 0;   // the GetValueInt key the first plan row uses
    bool lbTableDriven = false;

    switch (liMenuKind)
    {
        case 1:
        {
            // The main state menu: clear it, then walk the per-state plan rows.
            lpMenu = mpMenuIntervalMain;
            lpMenu->ClearMenuContent();
            lpPlan = KA_ICE_MENU_PLAN;
            liPlanRows = KI_ICE_MENU_PLAN_ROWS;   // FLAG: empty placeholder table
            liFirstElement = 28;                  // source-spine first element key
            lbTableDriven = true;
            break;
        }
        case 3:
        {
            // The copy-elements menu: clear it, then list this channel's element rows
            // followed by the "Copy Checked Elements" action row.
            lpMenu = mpMenuCommit;
            lpMenu->ClearMenuContent();

            // Per-channel element plan (placeholder -> zero rows). The element label is
            // the element-description display name from the committed schema.
            const s32 liChannel = miCurrentChannel;
            const ICEChannelElementPlan& lrPlan =
                KA_ICE_CHANNEL_PLAN[(liChannel >= 0 && liChannel < KI_ICE_NUM_CHANNEL_PLANS) ? liChannel : 0];

            s32 liRow = 0;
            for (; liRow < lrPlan.miElementCount; ++liRow)
            {
                // FLAG (element-plan placeholder): zero rows in the placeholder table,
                // so this loop body does not execute; it is kept for control-flow
                // parity. The cell label would be the element's display name.
                const s32 liElement = lrPlan.maiElement[liRow];
                if (liRow < lpMenu->miNumRows && liRow >= 0)
                {
                    ICEWidgetMenuItem& lrItem = lpMenu->mpRows[liRow];
                    if (lrItem.miItemCount > 0)
                    {
                        const char* lpcName = ICEElementDescriptions[liElement].mpDisplayName;
                        WriteMenuCell(lrItem.macItemText[0], lpcName);
                    }
                }
            }

            // The trailing "Copy Checked Elements" action row.
            if (liRow < lpMenu->miNumRows && liRow >= 0)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[liRow];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_COPY_CHECKED);
            }
            break;
        }
        case 4:
        {
            // The commit menu: up to three fixed action rows.
            lpMenu = mpMenuAssembly;
            if (lpMenu->miNumRows > 0)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[0];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_COMMIT);
            }
            if (lpMenu->miNumRows > 1)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[1];
                if (lpMenu->mpRows[0].miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_DISCARD);
            }
            if (lpMenu->miNumRows > 2)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[2];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_RESET_TAKE);
            }
            return;
        }
        case 5:
        {
            // The cancel menu: position it (right of its anchor) then fill two rows.
            lpMenu = mpMenuCancel;
            const f32 lfWidth = lpMenu->GetWidth();
            // FLAG (positioning): the source spine offsets the cancel menu's screen
            // origin by the menu width (the +52/+56 == mfX/mfY stores). The anchor
            // left/top come from the menu's own position floats.
            lpMenu->mfX = lfWidth + lpMenu->mfX;

            if (lpMenu->miNumRows > 0)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[0];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_CANCEL);
            }
            if (lpMenu->miNumRows > 1)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[1];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_DELETE);
            }
            return;
        }
        default:
            return;
    }

    // Table-driven fill (kinds 1 and 3 fall through here for the main state-menu plan
    // walk). The per-state plan rows that match the current channel/value group print
    // their label into row `liRow`, and the token selected by the row element's live
    // take value into the row's second cell.
    if (lbTableDriven && liMenuKind == 1)
    {
        s32 liRow = 0;
        s32 liByteRow = 0;   // parity with the source-spine row-stride walk
        (void)liByteRow;
        s32 liElement = liFirstElement;

        for (s32 li = 0; li < liPlanRows; ++li, ++liElement)
        {
            const ICEMenuPlanRow& lrRow = lpPlan[li];

            // Only rows belonging to the current channel/value group are shown.
            if (lrRow.miChannel != miCurrentChannel)
                continue;

            // The row label cell.
            if (liRow < lpMenu->miNumRows && liRow >= 0)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[liRow];
                if (lrItem.miItemCount > 0)
                    WriteMenuCell(lrItem.macItemText[0], lrRow.mpcLabel);
            }

            // The live take value for this row's element selects the token cell.
            const u32 luValue = static_cast<u32>(mEditTake.GetValueInt(liElement));
            const char* lpcToken;
            if (lrRow.mppTokens == 0)
            {
                // No token table: the special element 42 prints one of three tokens or
                // "None"; everything else leaves the token cell untouched.
                if (liElement != 42)
                {
                    ++liRow;
                    continue;
                }
                // FLAG (token table): the three element-42 tokens are data-segment
                // content; >2 maps to "None" (the recoverable default).
                lpcToken = (luValue > 2u) ? "None" : "None";
            }
            else
            {
                lpcToken = lrRow.mppTokens[luValue];
            }

            // The token cell is the row's second column (macItemText[1]).
            if (liRow < lpMenu->miNumRows && liRow >= 0)
            {
                ICEWidgetMenuItem& lrItem = lpMenu->mpRows[liRow];
                if (lrItem.miItemCount > 1)
                    WriteMenuCell(lrItem.macItemText[1], lpcToken);
            }
            ++liRow;
        }
    }

    // Fallback: when the table-driven fill left every row empty, show a single
    // "No data" row.
    if (lpMenu->GetNumNonEmptyItems() == 0 && lpMenu->miNumRows > 0)
    {
        ICEWidgetMenuItem& lrItem = lpMenu->mpRows[0];
        if (lrItem.miItemCount > 0)
            WriteMenuCell(lrItem.macItemText[0], KPC_ICE_LABEL_NO_DATA);
    }
}

} // namespace ICE
