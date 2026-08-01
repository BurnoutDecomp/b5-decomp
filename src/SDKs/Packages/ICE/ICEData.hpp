#ifndef SDKS_PACKAGES_ICE_ICEDATA_HPP
#define SDKS_PACKAGES_ICE_ICEDATA_HPP

#include "types.hpp"
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"   // ICE::ICEValue, ICEChannel, ICEParameter, enums
#include "SDKs/Packages/ICE/ICEPoint.hpp"        // ICE::Cubic1D (ICETake embeds Cubic1D[28])
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"  // CgsResource::ID (IResourceManager::GetTakeData(ID))

// ============================================================================
// SDKs/Packages/ICE/ICEData.hpp
//
// The ICE camera-take runtime types: ICETakeData (one serialised camera-take
// record in an ICE take dictionary), the live ICETake that plays one back,
// ICEElementCount, and the IResourceManager interface ICETake queries for take
// data. This is the canonical mirrored home (the DecFIGS DWARF declares these in
// SDKs/Packages/ICE/ICEData.hpp, and the ledger maps ICE::ICETake* /
// ICE::ICETakeData::* methods to SDKs/Packages/ICE/ICEData.cpp).
//
// GROWN from the minimal ICETakeData-header slice to the full DWARF layout so the
// ICEData.cpp body phase can be reconstructed against a frozen layout. The
// ICETakeData FIELDS are kept EXACTLY as committed (DWARF-correct; miGuid @+0x8 is
// X360-attested -- BrnResource::ICEList reads it). Member NAMES/ORDER/TYPES are
// authoritative from the DWARF; trivial inline-away accessors carry bodies,
// everything substantial is declaration-only (bodies in ICEData.cpp; the per-TU
// `cl /c` gate does not link).
//
// LAYOUT (32-bit, DWARF ICEData.hpp:51-79; miGuid @+0x8 also X360-attested):
//   ICETakeData @0x00  bTNode<ICETakeData> base (tree node, 8 bytes on 32-bit)
//               @0x08  s32  miGuid
//               @0x0C  char macTakeName[KI_MAX_TAKENAME_LENGTH]
//               @0x2C  f32  mfLength
//               @0x30  u32  muAllocated
//               @0x34  ICEElementCount mElementCounts[12]
//
// (DWARF spells the scalars float32_t / int32_t; those are NOT project types ->
// f32 / s32, per the type vocabulary in types.hpp.)
// ============================================================================

// CgsResource::Resource -- the resource-fixup argument of ICETakeData::FixUp /
// FixDown (DWARF: `const Resource&`, i.e. CgsResource::Resource). Those two methods
// are declaration-only here (the body phase defines them out-of-line) and take the
// type only by const reference, so an incomplete forward declaration suffices --
// this avoids pulling the full resource-system header into the ICE vocabulary.
// (tag `struct` per the DWARF.)
namespace CgsResource
{
    struct Resource;
}

namespace ICE
{

// Debug text-stream sink used by ICETakeData::SaveData. Its real home is
// SDKs/Packages/ICE/ICEFile.hpp (the ICEFile.cpp TU); forward-declared here since
// SaveData only takes it by pointer.
class ICEFileHandler;

// ---------------------------------------------------------------------------
// ICE camera/take constants (ICEData.hpp:34-57, DWARF). The float-typed ones are
// declared extern (defined out-of-line in ICEData.cpp) since their values are not
// in the DWARF; the integer ones are inline constants.
// ---------------------------------------------------------------------------
static const u32 KU_GTID_STRING_LENGTH    = 13;
extern const f32 ICE_LENS_DEFAULT;
extern const f32 ICE_NEAR_CLIP_DEFAULT;
static const u8  ICE_APERTURE_MAX         = 37;
extern const f32 ICE_TANGENT_LENGTH_DEFAULT;
static const u8  ICE_PERCENT_MAX          = 100;
static const u16 ICE_INVALID_KEY          = 65535;
static const u16 ICE_INVALID_INTERVAL     = 65535;
static const u16 ICE_MAX_EDIT_KEYS        = 100;
static const u16 ICE_MAX_EDIT_INTERVALS   = 99;
static const u32 ICE_MAX_UNDO_SIZE        = 102400;
extern const f32 ICE_EPSILON;
extern const f32 ICE_DATA_SMALL_FLOAT;
static const s32 ICE_GROUP_NAME_LENGTH       = 128;
static const s32 ICE_SHORT_GROUP_NAME_LENGTH = 4;

// ICEData.hpp:64 (DWARF). Fixed take-name buffer length.
static const s32 KI_MAX_TAKENAME_LENGTH = 32;

// ICEData.hpp:75 (DWARF). Per-channel interval/key counts -- 4 bytes (two u16); a
// 12-entry array spans the 0x34..0x5B region of the take header.
struct ICEElementCount
{
    u16 mu16Intervals;
    u16 mu16Keys;
};

// ---------------------------------------------------------------------------
// ICE::Precision -- returns the number of significant decimal digits to display
// for a value (used by ICETakeData::SaveData when formatting a <FLOAT> element).
// Scans the integer magnitude to cap the fractional-digit budget, then extracts
// fractional digits until the fraction is exhausted or the budget is reached.
// ---------------------------------------------------------------------------
int Precision(f32 lfValue);

// ---------------------------------------------------------------------------
// ICETakeData (ICEData.hpp:93, DWARF). One serialised camera-take record. In the
// DWARF this derives from bTNode<ICETakeData> (intrusive tree node); that base has
// no reconstructed home yet, so it is modeled here as an explicit 8-byte padding
// buffer (a placeholder for the not-yet-reconstructed base, to be replaced when the
// bNode/bTNode TU lands -- NOT an offset hack).
//
// FIELDS kept EXACTLY as committed (DWARF field-for-field; miGuid @+0x8 is
// additionally X360-attested via BrnResource::ICEList's guid lookup). The fields
// stay PUBLIC: BrnResource::ICEList::GetICETakeDataFromGuid reads `mpTakeData->miGuid`
// directly, so making them private would break that committed consumer.
//
// GROWN with the full method set (DWARF ICEData.hpp:111-156). Trivial inline-away
// accessors carry bodies; everything substantial is declaration-only (bodies in
// ICEData.cpp).
// ---------------------------------------------------------------------------
struct ICETakeData
{
    // ---- fields (kept exactly as committed) ----
    u8              mPadNodeBase[8];                    // @0x00  bNode/bTNode tree base (8 bytes)
    s32             miGuid;                             // @0x08  GameDB id (read by ICEList)
    char            macTakeName[KI_MAX_TAKENAME_LENGTH];// @0x0C  take name
    f32             mfLength;                           // @0x2C  take length (seconds)
    u32             muAllocated;                        // @0x30  allocated size/flag
    ICEElementCount mElementCounts[12];                 // @0x34  per-channel interval/key counts

    // ICEData.hpp:64 (DWARF). Key-index storage type for the take's channels.
    typedef u16 ICE_KEY_INDEX;

    // --- DECLARE-ONLY (bodies in ICEData.cpp / sibling TUs) ---
    u32  GetResourceType();
    void FixUp(const CgsResource::Resource& lrResource);
    void FixDown(const CgsResource::Resource& lrResource);
    bool operator==(const ICETakeData& lrOther);
    ICETakeData& operator=(const ICETakeData& lrOther);
    void Construct();
    u32  ComputeEditSize();
    u32  ComputeActualSize() const;
    u8*  GetVariableData() const;
    u32  GetVariableDataSize() const;
    bool HasChannelData(s32 liChannel) const;
    ICE_KEY_INDEX GetNumKeys(s32 liChannel) const;
    ICE_KEY_INDEX GetNumIntervals(s32 liChannel) const;
    void SetNumKeys(s32 liChannel, s32 liNumKeys);
    void SetNumIntervals(s32 liChannel, s32 liNumIntervals);
    void SetName(const char* lpcName);
    // Debug text dumper (class:ICE::ICETakeData TU, @0x82532CF8): writes an XML-ish
    // dump of the decoded take through an ICEFileHandler. ICEFileHandler is the
    // SDKs/Packages/ICE/ICEFile.cpp type (forward-declared above).
    void SaveData(ICEFileHandler* lpHandler, s32 liTakeNumber);

    // --- Trivial inline-away accessors ---
    bool IsAllocated() const                       { return muAllocated != 0; }
    const ICEElementCount* GetElementCounts() const { return &mElementCounts[0]; }
    s32  GetGuid() const                           { return miGuid; }
    void SetGuid(s32 liGuid)                       { miGuid = liGuid; }
    f32  GetLength() const                         { return mfLength; }
    void SetLength(f32 lfLength)                   { mfLength = lfLength; }
    // No out-of-line X360 symbol -- inlined at every call site as the bare
    // `take + 0x0C`. Attested at ICEAuthor::SaveTake @0x82533B90, which formats
    // `"<TAKE name=\"%s\" guid=\"%s\">"` with `(a2 + 12)` as the name argument (and
    // `*(a2 + 8)` == miGuid, with -1 as its invalid sentinel, as the guid source).
    const char* GetName() const                    { return macTakeName; }
};

// ---------------------------------------------------------------------------
// IResourceManager (ICEData.hpp:226, DWARF). The abstract interface ICETake holds
// a const pointer to in order to resolve take data by id or index. Minimal slice --
// only the two GetTakeData virtuals the DWARF declares; declaration-only (no
// bodies), modeled as a pure-virtual interface (the I-prefix interface convention).
// ICETake only ever uses it by const pointer, so the layout impact is one vptr.
// ---------------------------------------------------------------------------
struct IResourceManager
{
    virtual ~IResourceManager() {}

    virtual const ICETakeData* GetTakeData(CgsResource::ID lId) const = 0;
    virtual const ICETakeData* GetTakeData(s32 liIndex) const = 0;
};

// ---------------------------------------------------------------------------
// ICETake (ICEData.hpp:236, DWARF). A live, evaluatable camera take: it points at
// the serialised ICETakeData (and an optional sub-take for blending), holds the
// decoded per-element ICEValue table, the runtime ICEChannel array, the Cubic1D
// followers, and the edit-buffer element-data pointers, and exposes the value /
// interval queries the camera system drives each frame.
//
// FIELDS IN ORDER (DWARF ICEData.hpp:356-366):
//   @0x000 const IResourceManager* mpResourceManager
//   @0x004 ICETakeData* mpTakeData
//   @0x008 f32 mfParameter
//   @0x00C f32 mfSubParameter
//   @0x010 ICETakeData* mpSubTakeData
//   @0x014 ICEValue mValues[48]
//   ...    ICEChannel mChannels[12]
//   ...    Cubic1D mCubics[28]
//   ...    u8* mpElementData[48]
//   ...    s32 mxSubTakeChannels         (bitmask: which channels come from the sub-take)
//   ...    bool mbNewSubTakeThisFrame
//
// Trivial inline-away accessors carry bodies; everything the body phase will define
// (Construct, edit-buffer management, parameter/sub-take setup, value get/set,
// interval arithmetic) is declaration-only.
// ---------------------------------------------------------------------------
struct ICETake
{
private:
    const IResourceManager* mpResourceManager;   // @0x000  take-data provider
    ICETakeData*            mpTakeData;           // @0x004  primary take
    f32                     mfParameter;          // @0x008  current playback parameter
    f32                     mfSubParameter;       // @0x00C  sub-take parameter
    ICETakeData*            mpSubTakeData;        // @0x010  optional blend sub-take
    ICEValue                mValues[48];          // @0x014  decoded per-element values
    ICEChannel              mChannels[12];        //         runtime channels
    Cubic1D                 mCubics[28];          //         cubic followers
    u8*                     mpElementData[48];    //         edit-buffer element data
    s32                     mxSubTakeChannels;    //         sub-take channel bitmask
    bool                    mbNewSubTakeThisFrame;//         sub-take changed this frame

    // ---- EDITOR-ONLY state (present in the camera-take editor; the runtime/eval
    // layout ends at mbNewSubTakeThisFrame). The edit path maintains a bounded undo
    // history of whole serialised-take snapshots. Offsets are struct-relative to the
    // 32-bit layout; sizeof differs on the 64-bit host because the two list-head
    // pointers widen (same as the IResourceManager vptr situation). ----

    // @0x72C  Undo-history list head: an intrusive doubly-linked list of saved
    // ICETakeData snapshots. When empty, Next/Prev point back at the head (self-
    // linked sentinel). Each node is a heap copy of an ICETakeData whose 8-byte node
    // base provides the Next/Prev linkage; the list is a bTList<ICETakeData> whose
    // reconstructed home does not exist yet, so it is modeled here as an 8-byte head
    // buffer (a placeholder for the not-yet-reconstructed list base, to be replaced
    // when the bTList TU lands -- NOT an offset hack). Edit bodies treat
    // mUndoList[0] as Next and mUndoList[1] as Prev.
    void*                   mUndoList[2];         // @0x72C  intrusive undo-snapshot list head

    // @0x734  Running total of bytes held by the undo list (sum of each node's
    // ComputeActualSize). PushUndo evicts oldest snapshots until a new one fits
    // within the undo budget; FlushUndo zeroes it.
    u32                     muUndoUsedBytes;      // @0x734  undo-list byte accumulator

public:
    // Default constructor: zeros the decoded value table and default-inits the
    // per-element Cubic1D followers (the data pointers / channels are bound later by
    // SetData / SetDataPointers). Body in ICEDataICETake.cpp.
    ICETake();

    // --- DECLARE-ONLY (lifecycle / edit buffer; bodies in ICEData.cpp) ---
    void Construct(const IResourceManager* lpResourceManager);
    void Destruct();
    void NewEditBuffer();
    void FreeEditBuffer();

    // --- DECLARE-ONLY (undo history; bodies in ICEDataICETake.cpp) ---
    // Snapshot the current editable take onto the undo list, evicting the oldest
    // snapshots first so the list stays within ICE_MAX_UNDO_SIZE.
    void PushUndo();
    // Restore the newest snapshot onto the live take and drop it. Returns true if a
    // snapshot was popped, false if the undo list was empty.
    bool PopUndo();
    // Drop the newest snapshot without restoring it.
    void DiscardUndo();
    // True if the live editable take differs from its newest undo snapshot (or if
    // there is no snapshot yet).
    bool DataChanged() const;

    // --- Trivial inline-away accessors ---
    bool IsAllocated() const               { return mpElementData[0] != 0; }
    ICETakeData* GetData() const           { return mpTakeData; }
    f32  GetParameter() const              { return mfParameter; }
    f32  GetSubParameter() const           { return mfSubParameter; }
    void SetSubParameter(f32 lfSubParameter) { mfSubParameter = lfSubParameter; }
    bool IsNewSubTakeThisFrame() const     { return mbNewSubTakeThisFrame; }
    f32  GetLength() const                 { return mpTakeData->GetLength(); }

    // --- DECLARE-ONLY (data binding / parameter & sub-take setup; bodies elsewhere) ---
    void SetData(ICETakeData* lpTakeData, f32 lfParameter);
    void SetDataPointers(ICETakeData* lpTakeData, bool lbEdit);
    bool IsChannelFromSubTake(s32 liChannel);

    bool SetParameter(f32 lfParameter, bool lbForce, bool lbWrap);

    void SetSubTake(s32 liGuid, bool lbForce);
    void SetSubTake(const ICETakeData* lpSubTakeData, bool lbForce);

    void SetLength(f32 lfLength);
    f32  GetSubTakeLength() const;

    // --- DECLARE-ONLY (value queries; bodies elsewhere) ---
    ICEValue GetValue(s32 liChannel, u16 lu16Key) const;
    ICEValue GetValue(s32 liChannel) const;
    void     SetValue(s32 liChannel, u16 lu16Key, ICEValue lValue);

    f32 GetValueFloat(s32 liChannel, u16 lu16Key) const;
    f32 GetValueFloat(s32 liChannel) const;

    s32 GetValueInt(s32 liChannel, u16 lu16Key) const;
    s32 GetValueInt(s32 liChannel) const;

    bool IsHardCut(s32 liChannel, s32 liElement) const;
    bool IsHardCut(s32 liChannel, u16 lu16Key, s32 liElement) const;

    u8*           GetElementData(s32 liChannel) const;

    u16 GetCurrentInterval(s32 liChannel) const;

    // --- Channel-forwarding accessors (no out-of-line X360 symbol: the console
    // INLINES each of these into its callers). Every one is a single read through
    // the embedded ICEChannel, and each is attested by the inlined form in the
    // callers' asm -- ICETakeData::SaveData @0x82532CF8 walks a bound scratch take
    // and reads channel+0 (keys), channel+2 (intervals), channel+8 (mpKeyIndices)
    // and channel+0xC (mpParameters) directly, with mChannels based at take+0xD4
    // and a 16-byte channel stride. Bodied here beside the other trivial
    // inline-away accessors, per this header's own convention. ---
    ICEParameter* GetParameterData(s32 liChannel) const { return mChannels[liChannel].GetParameterData(); }
    u16*          GetKeyData(s32 liChannel) const       { return mChannels[liChannel].GetKeyData(); }

    u16 GetNumKeys(s32 liChannel) const      { return (u16)mChannels[liChannel].GetNumKeys(); }
    u16 GetNumIntervals(s32 liChannel) const { return (u16)mChannels[liChannel].GetNumIntervals(); }

    u16 GetIntervalKey(s32 liChannel, u16 lu16Interval) const;
    f32 GetIntervalEnd(s32 liChannel, u16 lu16Interval) const;
    f32 GetIntervalSize(s32 liChannel, u16 lu16Interval) const;
    f32 GetIntervalStart(s32 liChannel, u16 lu16Interval) const;
    void GetIntervalBracket(s32 liChannel, u16 lu16Interval, f32* lpfStart, f32* lpfEnd) const;
    f32 GetIntervalParameter(s32 liChannel, u16 lu16Interval, s32 liElement) const;

    // --- DECLARE-ONLY (editor element/parameter ops; bodies in ICEDataICETake.cpp) ---
    // Copy one element's value out of a source take/key into this take at a
    // destination key. liChannel bounds the destination key; liElement is the
    // element-description index (passed straight through to GetValue/SetValue).
    s32  CopyKeyElement(s32 liChannel, s32 liDestKey, s32 liElement,
                        const ICETake* lpSrc, u16 lu16SrcKey);
    // Move an interval-boundary parameter by lfParameter, clamped to keep at least
    // one frame of spacing from each neighbouring boundary, then re-enforce spacing.
    // liDelta offsets lu16Interval to pick the boundary moved. Returns true when the
    // boundary actually changed.
    bool MoveParameter(s32 liChannel, u16 lu16Interval, s32 liDelta, f32 lfParameter);

    // Resize one channel's key/interval storage. liKeyDelta/liIntervalDelta grow or
    // shrink the channel's key and interval counts (each clamped into the editable
    // range; a resize that would exceed the range is rejected); liKeyAt/liIntervalAt
    // position the inserted/removed run; liLeftHardCut/liRightHardCut adjust the
    // inserted interval-index run for hard-cut boundaries. Rebuilds a fresh edit
    // buffer, snapshots the prior take onto the undo list, copies it back over the
    // live take and rebinds. Returns true if the take was resized.
    bool ChangeSize(s32 liChannel, s32 liKeyDelta, s32 liKeyAt,
                    s32 liIntervalDelta, s32 liIntervalAt,
                    s32 liLeftHardCut, s32 liRightHardCut);
    // Set channel liChannel to exactly liNumKeys keys / liNumIntervals intervals by
    // delegating the per-count deltas to ChangeSize.
    bool SetSize(s32 liChannel, s32 liNumKeys, s32 liNumIntervals);

    // --- DECLARE-ONLY (editor edit operations; bodies in ICEDataICETake.cpp) ---
    // Insert a key/interval into liChannel at the current playback parameter (no-op
    // if the parameter already lands on a boundary). lbAfter biases the insert side.
    bool Insert(s32 liChannel, u8 lbAfter);
    // Delete the current interval of liChannel (one or two keys depending on the
    // bracket hard-cut state). No-op if the channel has no intervals.
    bool Delete(s32 liChannel);
    // Delete the key on the left side of liChannel's current interval (one key, or
    // two when that boundary is a hard cut). No-op if no keys or current interval 0.
    bool DeleteLeftSideKey(s32 liChannel);
    // Collapse the soft-key run spanning intervals (liFirst, lu16Last) of the
    // assembly channel (10) to a single interval.
    bool DeleteAssemblySoftKeys(s32 liFirst, u16 lu16Last);
    // Split a soft interval boundary into a hard cut by inserting one key (no-op if
    // already a hard cut for this element).
    bool Harden(s32 liChannel, s32 liInterval, s32 liElement);
    // Merge a hard-cut boundary back into a soft one by removing one key (no-op
    // unless it is a hard cut whose duplicated key pair matches).
    bool Soften(s32 liChannel, s32 liInterval, s32 liElement);
    // Copy this take's current-interval element into a destination take (sizes the
    // destination channel to one interval, then copies the bracket-key value).
    bool CopyElement(ICETake* lpDest, s32 liChannel, s16 li16Offset, s32 liElement);
    // Paste a source take's element (its key 0) into this take at the current
    // interval's bracket key (offset by li16Offset).
    bool PasteElement(ICETake* lpSrc, s32 liChannel, s16 li16Offset, s32 liElement);

private:
    // --- DECLARE-ONLY (private machinery; bodies elsewhere) ---
    bool SetParameter(s32 liChannel, f32 lfParameter, bool lbForce);
    f32  GetSlope(s32 liChannel, s32 liElement, u16 lu16Key, s32 liSide) const;
    void MarkChannelFromSubTake(s32 liChannel);
    void FlushUndo();

    // No out-of-line X360 symbol -- the console inlines this predicate into every
    // `CGS_ASSERT(IsEditable(), "IsEditable()")` site. Read straight off the asm at
    // the head of ICETake::Insert @0x8253C288 (and identically in Delete
    // @0x8253C528 and its six siblings):
    //     lwz r11, 4(r3)      ; mpTakeData
    //     beq -> 0            ; null take is not editable
    //     lwz r11, 0x30(r11)  ; mpTakeData->muAllocated
    //     -> (r11 != 0)
    // i.e. an edit buffer is one whose take data is heap-owned (muAllocated set),
    // which is exactly what FreeEditBuffer uses to decide it may free it.
    bool IsEditable() const { return mpTakeData != 0 && mpTakeData->IsAllocated(); }
};

// ---------------------------------------------------------------------------
// ⭐ ICE::InitICEDescriptions @0x82532A08 -- the RUNTIME INITIALISER of the whole ICE
// element-description system: it fills the per-channel element schedules
// (gaICEElementChannels) that ICETake::SetParameter walks to decide WHICH elements to
// evaluate, and runs ICEElementDescription::Prepare over every element.
//
// ⚠️ IT HAD NO DECLARATION AND NO CALLER. The body has been sitting in ICEData.cpp
// (mounted) since that TU landed, and nothing ever called it, so every per-channel
// schedule stayed at miNumKeyElements == 0 and ICETake::SetParameter evaluated ZERO
// elements per channel -- i.e. mValues[] was never written and EVERY authored ICE camera
// element read back as 0 for the whole session. Declared here (its own home header) so its
// real console caller, BrnDirector::ICEWrapper::Prepare @0x8253DD90, can reach it.
// ---------------------------------------------------------------------------
void InitICEDescriptions();

// ---------------------------------------------------------------------------
// ICEGroup (ICEData.hpp:376, DWARF). A named collection of takes + assembly takes.
// Not referenced by-value from ICETake / ICETakeData, so it is intentionally NOT
// reconstructed here -- the body phase can request it when an ICEData.cpp function
// actually needs it (it embeds bTList<ICETakeData>, whose home is not yet
// reconstructed). Kept out per the GROW-don't-fork rule (no speculative types).
// ---------------------------------------------------------------------------

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEDATA_HPP
