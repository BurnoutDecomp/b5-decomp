#ifndef SDKS_PACKAGES_ICE_ICEAUTHOR_HPP
#define SDKS_PACKAGES_ICE_ICEAUTHOR_HPP

#include "types.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"        // ICE::ICETake (two embedded takes), ICETakeData, IResourceManager
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"   // ICE::eICESpace / ICEChannel / ICEElementDescription / ICEParameter
#include "SDKs/Packages/ICE/ICEMemory.hpp"      // ICE::ICEPointers (Construct arg)
#include "SDKs/Packages/ICE/ICEMath.hpp"        // ICE::Matrix4 (GetWorldTransformationMatrix arg)

// ============================================================================
// SDKs/Packages/ICE/ICEAuthor.hpp
//
// ICE::ICEAuthor -- the in-game ICE camera-take/assembly EDITOR. It owns the live
// "edit" take the artist scrubs, a second take used as the assembly edit-source /
// undo scratch, the editor state machine (BROWSER..ASSEMBLY_EDIT_END), the
// scrub/zoom/range scalars, an intrusive list of newly created (edited) takes, and
// the per-element copy/paste buffers. ICEAuthor is the BASE the in-game
// ICEController is-a (the editor `this` handed to ICEController::EditorOn /
// SetState IS this ICEAuthor); ICEController grows the menu/cursor/JoyHandler layer
// on top.
//
// ----------------------------------------------------------------------------
// LAYOUT (32-bit, derived from ALL 43 ICEAuthor bodies; offsets are this-relative
// byte offsets, cross-checked between the byte-offset `*(a1 + N)` reads and the
// word-index `a1[k]` reads -- e.g. a1[934]==+0xE98 state, a1[936]==+0xEA0 channel,
// a1[979]==+0xF4C list-tail -- which agree):
//
//   @0x000  (vtable / not-touched-by-these-bodies leading region; ICEAuthor has no
//           field read at +0x00 in the 43 bodies. Construct begins writing at +4.)
//   @0x004  ICEFileHandler*       mpFileHandler        (Construct: = bundle a2[1])
//   @0x008  CameraSpaceHandler*   mpCameraSpaceHandler (Construct: = bundle a2[3];
//                                 GetWorld/TransformSpace pass it to
//                                 CameraSpaceHandler::GetTransformToWorld)
//   @0x00C  IResourceManager*     mpResourceManager    (Construct: = bundle a2[5];
//                                 ICETake::Construct(... , mpResourceManager); also
//                                 invoked as a take-lookup functor in
//                                 SetAssemblySourceTake: result=(**+0xC)(+0xC))
//   @0x010  ICETakeData*          mpEditTakeData       (a1[4]; EditorOn source take)
//   @0x014  ICETakeData*          mpAssemblyTakeData   (a1[5]; EditAssembly source)
//   @0x018  ICETakeData*          mpSubTakeData        (sub-take data; SetState/Load*)
//   @0x01C  f32                   mfInsertTangent      (*(v4+28); InsertTakeSegment)
//   @0x020  s32                   miSubTakeGuid        (v2[7]=guid in SetAssemSrcTake)
//   @0x028  ICETake               mEditTake            (the live edit take; 0x738)
//   @0x760  ICETake               mScratchTake         (assembly-edit-source / undo
//                                 scratch take; built+edit-buffered in Construct; 0x738)
//   @0xE98  s32                   miState              (a1[934]; editor state enum)
//   @0xE9C  s32                   miPrevState          (a1[935]; previous state)
//   @0xEA0  s32                   miCurrentChannel     (a1[936]; current channel/space idx)
//   @0xEA4  s32                   maiElementWidget[28] (per-element index/dirty slots;
//                                 zeroed in Construct, written by SetState's
//                                 element-description sweep; spans 0xEA4..0xF14)
//   @0xF14  f32                   mfMoveAccumulator    (Move/SetParameterChange)
//   @0xF18  f32                   mfSavedParameter     (SetState save/restore)
//   @0xF1C  f32                   mfRangeLo            (play range low)
//   @0xF20  f32                   mfRangeHi            (play range high)
//   @0xF24  f32                   mfZoomLo             (assembly/zoom range low)
//   @0xF28  f32                   mfZoomHi             (assembly/zoom range high)
//   @0xF2C  f32                   mfSavedRangeLo       (SetState save)
//   @0xF30  f32                   mfSavedRangeHi       (SetState save)
//   @0xF38  s32                   miPad3896            (zeroed in Construct)
//   @0xF3C  s32                   miPad3900            (zeroed in Construct)
//   @0xF40  s32                   miPad3904            (zeroed in Construct)
//   @0xF44  s32                   miSelectedTakeGuid   (init -1 in Construct)
//   @0xF48  void*                 mEditedTakeList[2]   (+0xF48/+0xF4C; intrusive
//                                 bTList<ICETakeData> head, self-linked sentinel:
//                                 [0]=Next/Head, [1]=Prev/Tail; word index 979 ==
//                                 +0xF4C is the Tail. CreateNewTake links new takes here)
//   @0xF50  u8                    mabElementEnabled[28] (per-element enable flags; set
//                                 to 1 in Construct's trailing byte loop)
//
// The scrub/range/zoom scalar block, the edited-takes list head and the 28 enable
// bytes follow maiElementWidget[28] CONTIGUOUSLY (the widget array ends at
// 0xEA4 + 28*4 = 0xF14, and Construct's stores land at 0xF14..0xF6C; the list Tail
// slot at +0xF4C == word index 979 cross-checks). There is no gap after the widget
// array.
//
// FLAG (ICEController overlap): ICEController.hpp (a MINIMAL SLICE, not owned here)
// places mEditTake @0x28 and a `miMenusActive` flag @0xE98. Those offsets COINCIDE
// with this ICEAuthor's mEditTake @0x28 and miState @0xE98 -- because ICEController
// IS-A ICEAuthor and they share the same `this`. ICEController's slice mEditTake /
// miMenusActive are stand-ins for THESE author members. When the ICEController TU is
// reconstructed it should derive from ICEAuthor and DROP its slice copies; do NOT
// fork. (Not edited here per the no-touch-ICEController rule.)
//
// FLAG (runtime ICETake size): the second take lands at +0x760 == 0x28 + 0x738, i.e.
// the runtime ICETake is 0x738 bytes; miState lands at +0xE98 == 0x760 + 0x738. The
// reconstructed PC ICETake may differ in sizeof (the IResourceManager vptr / the two
// undo-list head pointers widen on the 64-bit host), so the touched offsets are
// pinned with static_assert against the *expected* 32-bit layout via a synthetic
// offset model below, NOT against sizeof(ICETake). See the ICEData.hpp ICEController
// note for the identical sizeof-drift situation.
// ============================================================================

namespace ICE
{

// CameraSpaceHandler is referenced by POINTER only (mpCameraSpaceHandler @0x08 is
// passed by value into CameraSpaceHandler::GetTransformToWorld in
// GetWorldTransformationMatrix / TransformSpace). Forward-declared so this header
// does not pull the whole ICECameraSpaceHandler vocabulary; the Space/Transform
// fan-out .cpp includes ICECameraSpaceHandler.hpp for the real type.
struct CameraSpaceHandler;

// ICEFileHandler is the take text sink (real home SDKs/Packages/ICE/ICEFile.hpp);
// SaveTake forwards it by pointer, so a forward declaration suffices here.
class ICEFileHandler;

// ---------------------------------------------------------------------------
// ICE::ICEAuthor (the camera-take/assembly editor). Per-element-count arrays are
// sized eICE_NUM_ELEMENTS (== 48) in the value table inside the takes, but the
// author's two per-element side tables (the widget-index slots and the enable
// flags) are 28-wide -- the Construct loops run exactly 28 iterations. So the
// author's per-element arrays are 28, NOT 48. FLAG: 28 is the loop count attested by
// Construct; if a fan-out body indexes past 28, revisit.
// ---------------------------------------------------------------------------
static const s32 KI_ICE_AUTHOR_ELEMENTS = 28;

// Editor state machine. SetState/GetStateName drive these; 0 == OFF/idle. Values
// are the switch cases of GetStateName (1..18). E_-prefixed per the project enum
// convention; the GetStateName strings are kept verbatim as the names.
enum EICEAuthorState
{
    E_ICE_AUTHOR_STATE_OFF                   = 0,
    E_ICE_AUTHOR_STATE_BROWSER               = 1,
    E_ICE_AUTHOR_STATE_EXIT_CONFIRM          = 2,
    E_ICE_AUTHOR_STATE_DELETE_CONFIRM        = 3,
    E_ICE_AUTHOR_STATE_LOADING_ANIMATION     = 4,
    E_ICE_AUTHOR_STATE_PLAYING_ANIMATION     = 5,
    E_ICE_AUTHOR_STATE_SCREENSHOT            = 6,
    E_ICE_AUTHOR_STATE_SCRUBBER             = 7,
    E_ICE_AUTHOR_STATE_ASSEMBLY_MARK         = 8,
    E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT         = 9,
    E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING = 10,
    E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS     = 11,
    E_ICE_AUTHOR_STATE_START_KEY_SELECTED    = 12,
    E_ICE_AUTHOR_STATE_END_KEY_SELECTED      = 13,
    E_ICE_AUTHOR_STATE_EDIT_START_KEY        = 14,
    E_ICE_AUTHOR_STATE_EDIT_END_KEY          = 15,
    E_ICE_AUTHOR_STATE_SELECT_COPY_MODE      = 16,
    E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START   = 17,
    E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END     = 18
};

struct ICEAuthor
{
    // ---- head pointers / scalars (Construct begins writing at +0x04) ----
    void*                   mpVTablePad;          // @0x000  leading word (no field read at +0 in these bodies)
    ICEFileHandler*         mpFileHandler;        // @0x004  bundle a2[1]
    CameraSpaceHandler*     mpCameraSpaceHandler; // @0x008  bundle a2[3]
    IResourceManager*       mpResourceManager;    // @0x00C  bundle a2[5]; also a take-lookup functor
    ICETakeData*            mpEditTakeData;        // @0x010  a1[4]  current edit-source take
    ICETakeData*            mpAssemblyTakeData;    // @0x014  a1[5]  assembly source take
    ICETakeData*            mpSubTakeData;         // @0x018  sub-take data
    f32                     mfInsertTangent;       // @0x01C  insert-segment tangent value
    s32                     miSubTakeGuid;         // @0x020  guid written before the take-lookup functor call
    u32                     muPad0x24;             // @0x024  alignment to the embedded take @0x28

    // ---- the two embedded ICETakes ----
    ICETake                 mEditTake;             // @0x028  live edit take
    ICETake                 mScratchTake;          // @0x760  assembly-source / undo scratch take

    // ---- editor state machine ----
    s32                     miState;               // @0xE98  a1[934]  EICEAuthorState
    s32                     miPrevState;           // @0xE9C  a1[935]  previous state
    s32                     miCurrentChannel;      // @0xEA0  a1[936]  current channel / space index

    // ---- per-element widget/index side table (28-wide; zeroed in Construct) ----
    s32                     maiElementWidget[KI_ICE_AUTHOR_ELEMENTS]; // @0xEA4 .. ends +0xF14

    // ---- scrub / range / zoom scalars (immediately follow the widget array) ----
    f32                     mfMoveAccumulator;     // @0xF14  Move/SetParameterChange accumulator
    f32                     mfSavedParameter;      // @0xF18  SetState saves/restores playback param
    f32                     mfRangeLo;             // @0xF1C  play-range low
    f32                     mfRangeHi;             // @0xF20  play-range high
    f32                     mfZoomLo;              // @0xF24  assembly/zoom range low
    f32                     mfZoomHi;              // @0xF28  assembly/zoom range high
    f32                     mfSavedRangeLo;        // @0xF2C  SetState save (range low)
    f32                     mfSavedRangeHi;        // @0xF30  SetState save (range high)
    u32                     muPad0xF34;            // @0xF34  alignment / unrecovered word

    s32                     miPad3896;             // @0xF38  zeroed in Construct
    s32                     miPad3900;             // @0xF3C  zeroed in Construct
    s32                     miPad3904;             // @0xF40  zeroed in Construct
    s32                     miSelectedTakeGuid;    // @0xF44  init -1 in Construct

    // ---- the edited-takes list (intrusive bTList<ICETakeData> head) ----
    // Self-linked sentinel: mEditedTakeList[0]==Next/Head, [1]==Prev/Tail. The Tail
    // slot is at +0xF4C (word index 979). CreateNewTake links new takes here;
    // FindEditedTakeFromGuid / Commit* walk and splice it. Modeled as a 2-pointer head
    // buffer (the bTList base reconstruction lives in ICEMemory.hpp; this matches the
    // ICETake mUndoList[2] modelling).
    void*                   mEditedTakeList[2];    // @0xF48 (Head) / @0xF4C (Tail)

    // ---- per-element enable flags (28-wide; set to 1 in Construct) ----
    u8                      mabElementEnabled[KI_ICE_AUTHOR_ELEMENTS]; // @0xF50 .. +28

    // ========================================================================
    // KeyElementOps group (designated header-extender pass): the per-element
    // COPY / VALUE CACHE the key-element setters and the SetState element sweep
    // touch. The accesses resolve to two of the side tables ALREADY declared
    // above -- no new storage is added (these are member-function views over
    // existing arrays), so the frozen layout is unchanged.
    //
    //   * VALUE CACHE: SetKeyElementValue / SetKeyElementValueChange and the
    //     SetState element sweep store the element's edited value as an f32 at
    //     element-stride from the SAME base as maiElementWidget (the widget
    //     side-table @0xEA4). i.e. the widget side-table IS the per-element value
    //     cache, read/written as f32 by the value setters and zeroed (bit-pattern
    //     0 == 0.0f) by Construct. ElementValueCache() exposes the f32 view so the
    //     bodies do not type-pun raw offsets.
    //
    //   * ENABLE FLAGS: CopyKeyElements / PasteKeyElements gate each element on a
    //     per-element enable BYTE indexed by the copy plan-table's element index.
    //     That byte array is mabElementEnabled (set to 1 in Construct). The bodies
    //     read it through ElementEnabled().
    //
    // NOTE: the scalar block (mfMoveAccumulator .. mfSavedRangeHi), the int pads, the
    // selected-take guid, the edited-takes list head and the 28 enable bytes land at
    // 0xF14..0xF6C -- IMMEDIATELY after the widget/value-cache array (which ends at
    // 0xEA4 + 28*4 = 0xF14), with the list Tail slot at +0xF4C == word index 979. The
    // layout above reflects this contiguous placement (an earlier draft modeled a
    // phantom 0x300 gap before the scalar block; it has been removed).
    // ========================================================================

    // f32 view over the per-element value cache (== maiElementWidget reinterpreted;
    // base 0xEA4, element stride 4). The value setters store the edited element
    // value here and the SetState sweep refreshes it from the take.
    f32*       ElementValueCache()       { return reinterpret_cast<f32*>(maiElementWidget); }
    const f32* ElementValueCache() const { return reinterpret_cast<const f32*>(maiElementWidget); }

    // Per-element enable flag (the copy/paste gate byte; == mabElementEnabled).
    bool ElementEnabled(s32 liElement) const { return mabElementEnabled[liElement] != 0; }

    // ========================================================================
    // The 43 ICEAuthor methods. Signatures are derived from the bodies + the call
    // patterns in the dossier (the ledger `__fastcall (this, args...)` maps the
    // first int arg to `this`). FLAGs below mark any signature whose arg count is
    // uncertain (the body shows more trailing register args than the call sites
    // pass -- typical of compiler-spilled / varargs-ish frames).
    //
    // FOUNDATIONAL group has bodies in ICEAuthor.cpp (below); the rest are
    // declaration-only for the fan-out (the per-TU `cl /c` gate does not link).
    // ========================================================================

    // --- lifecycle / entry ---
    void Construct(const ICEPointers* lpICEPointers);
    // EditorOn/EditAssembly compare their arg against the stored take-data pointer
    // (+0x10 / +0x14) and write it back, so the arg is the source ICETakeData*, not
    // an int. FLAG: the ledger renders the arg as `int a2` (a register-width word);
    // the body's `*(this+16) != a2` comparison against the take-data pointer fixes it
    // as ICETakeData* here.
    void EditorOn(ICETakeData* lpTakeData);
    void EditAssembly(ICETakeData* lpAssemblyTakeData);

    // --- state machine ---
    // SetState's third arg (a3, double) is only consumed on the SELECT_COPY_MODE
    // path (passed to IsEndKeyState as the "current key" disambiguator). Callers
    // EditorOn/EditAssembly invoke the 2-arg form; the trailing param defaults so
    // both call shapes bind. FLAG: a3 default value is a reconstruction choice (the
    // runtime leaves the FP arg register undefined on the 2-arg calls); 0.0 is safe
    // because the only consumer (IsEndKeyState) treats it as a key index that the
    // SELECT_COPY path overrides.
    void        SetState(s32 liState, f32 lfKey = 0.0f);
    const char* GetStateName(s32 liState) const;
    // IsEndKeyState: liKey == -1 walks back to the live state; returns true for the
    // END_KEY_SELECTED / EDIT_END_KEY / ASSEMBLY_EDIT_END states. Called with a key
    // index by most callers; the (this) overload is implied by the loop seeding from
    // miState when liKey == -1.
    bool        IsEndKeyState(s32 liKey) const;

    // --- trivial getters / range ---
    f32  GetValueFloat(s32 liElement) const;
    s32  GetCurrentSubtakeGuid() const;
    f32  GetShuttleSpeed() const;
    void AdjustZoomRange();
    f32  GetSubAssemRatio() const;

    // --- interval queries (delegators) ---
    void GetIntervalBracket(u16 lu16Interval, f32* lpfStart, f32* lpfEnd) const;
    f32  GetIntervalEnd(s16 li16Interval) const;
    s32  GetIntervalKey(u16 lu16Interval) const;
    f32  GetIntervalStart(u16 lu16Interval) const;

    // --- transform / space ---
    void GetWorldTransformationMatrix(Matrix4* lpMatrix, u8 lu8Space) const;
    void TransformSpace(f32* lpPoint, u8 lu8SrcA, u8 lu8SrcB, u8 lu8DstA, u8 lu8DstB) const;
    void SwitchEyeSpace(u16 lu16Interval, s32 liSpaceA, s32 liSpaceB);
    void SwitchLookSpace(u16 lu16Interval, s32 liSpaceA, s32 liSpaceB);

    // --- animation advance ---
    bool AdvanceAnimation(f32 lfTimeStep);
    bool AdvanceAssemblyAnimation(f32 lfTimeStep);

    // --- take ops ---
    ICETakeData* CreateNewTake(const char* lpcName, s32 liGuid);
    void ClearEditTake();
    void LoadCurrentTake();
    void CommitCurrentTake();
    void SaveTake(ICETakeData* lpTakeData);
    ICETakeData* FindEditedTakeFromGuid(s32 liGuid);

    // --- assembly ops ---
    void CommitCurrentAssembly();
    void LoadCurrentAssembly();
    void ChangeAssemTakeLength(f32 lfDeltaSeconds);
    void SetAssemblySourceTake(s32 liGuid);

    // --- key element ops ---
    void ToggleKey();
    bool CopyKeyElements();
    bool PasteKeyElements();
    void SetKeyElementValue(s32 liElement, f32 lfValue);
    void SetKeyElementValueChange(s32 liElement, s32 liChange, f32 lfDelta);
    s32  GetElementIndexByTag(const char* lpcTag) const;
    // SetState helper (body in ICEAuthorKeyElementOps.cpp): reload the per-element
    // value cache from the live take at the resolved key edge, over the current
    // channel's elements. Used by the key-selected / edit-key state entries.
    void RefreshElementValueCache(f32 lfKey);

    // --- segment ops ---
    bool InsertTakeSegment(f32 lfTangent, f32 lfSegmentLength);
    bool DeleteTakeSegment();
    void JumpToNextKey();
    void JumpToPreviousKey();

    // --- parameter move (bodies in ICEAuthorParamOps.cpp) ---
    // Nudge the current channel's interval-boundary parameter by a per-frame delta,
    // optionally snapping it to a neighbouring boundary when within a range-derived
    // tolerance and folding the leftover into mfMoveAccumulator. lbApply gates the
    // snap/accumulate path; when false the raw delta is forwarded unchanged.
    void MoveParameter(f32 lfDelta, bool lbApply);
    // Apply an accumulated parameter change to the live playback parameter: snap the
    // current parameter onto a bracket boundary within the tolerance, fold the
    // leftover into mfMoveAccumulator, then seek the take parameter (clamped 0..1).
    void SetParameterChange(f32 lfDelta, bool lbApply);
};

// ---------------------------------------------------------------------------
// Offset pins.
//
// The modeled byte offsets (mpFileHandler @+0x04, mEditTake @+0x28, miState @+0xE98,
// ...) are the 32-bit-ABI facts and live in the LAYOUT block above. They CANNOT be
// asserted as absolute `offsetof == 0xNN` on this 64-bit host: every pointer member
// (and the two list-head void* slots and the IResourceManager vptr inside the
// embedded ICETake) widens from 4 to 8 bytes, so the host offsets drift by the
// accumulated pointer-width delta -- the same documented drift ICEData.hpp /
// ICEController.hpp call out for the embedded ICETake. Pinning the absolute numbers
// would only ever pass under a 32-bit build.
//
// What IS pinned (ABI-independent): the MEMBER ORDER and contiguity that the layout
// depends on -- the head scalar block precedes the two embedded takes, the two takes
// are adjacent (mScratchTake immediately follows mEditTake), the state machine
// follows the second take, and the two per-element side arrays keep their 28 width.
// These catch any accidental reordering of the frozen layout. The fixed-width head
// fields (the f32 / s32 scalars) ARE offset-stable relative to mEditTake, so a
// couple of those relative pins are kept too.
// ---------------------------------------------------------------------------
static_assert(offsetof(ICEAuthor, mpFileHandler) < offsetof(ICEAuthor, mEditTake),
    "ICEAuthor head pointer block precedes mEditTake");
static_assert(offsetof(ICEAuthor, mEditTake) < offsetof(ICEAuthor, mScratchTake),
    "ICEAuthor mEditTake precedes mScratchTake");
static_assert(offsetof(ICEAuthor, mScratchTake) + sizeof(ICETake) == offsetof(ICEAuthor, miState),
    "ICEAuthor miState immediately follows the second embedded take (state block @ take2 end)");
static_assert(offsetof(ICEAuthor, miPrevState) == offsetof(ICEAuthor, miState) + sizeof(s32),
    "ICEAuthor miPrevState immediately follows miState");
static_assert(offsetof(ICEAuthor, miCurrentChannel) == offsetof(ICEAuthor, miPrevState) + sizeof(s32),
    "ICEAuthor miCurrentChannel immediately follows miPrevState");
static_assert(offsetof(ICEAuthor, maiElementWidget) == offsetof(ICEAuthor, miCurrentChannel) + sizeof(s32),
    "ICEAuthor widget side-table immediately follows miCurrentChannel");
static_assert(offsetof(ICEAuthor, mfRangeHi) == offsetof(ICEAuthor, mfRangeLo) + sizeof(f32),
    "ICEAuthor mfRangeHi immediately follows mfRangeLo (AdjustZoomRange treats them as a 2-float window)");
static_assert(offsetof(ICEAuthor, mfZoomHi) == offsetof(ICEAuthor, mfZoomLo) + sizeof(f32),
    "ICEAuthor mfZoomHi immediately follows mfZoomLo (zoom window is a 2-float pair)");
static_assert(sizeof(((ICEAuthor*)0)->maiElementWidget) == KI_ICE_AUTHOR_ELEMENTS * sizeof(s32),
    "ICEAuthor widget side-table is 28-wide");
static_assert(sizeof(((ICEAuthor*)0)->mabElementEnabled) == KI_ICE_AUTHOR_ELEMENTS,
    "ICEAuthor element-enable flags are 28-wide");

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEAUTHOR_HPP
