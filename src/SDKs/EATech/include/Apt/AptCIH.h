#pragma once

// ===========================================================================
// EATech Apt -- AptCIH (Character Instance Handle): the Apt scene-graph node.
//
// The runtime tree of a playing movie is built from AptCIHs. Each one is an
// AptValue (so ActionScript can reference movie clips), holds an instance name +
// packed state/flags, the display-list sibling/parent links, and points at its
// AptCharacterInst (which owns the renderable AptRenderItem). The spine is:
//   AptCIH (scene node) -> AptCharacterInst -> AptRenderItem (renderable).
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (6AptCIH, ~80 methods). LAYOUT
// (named, x64-width; console dwords): AptValueGC base, then
//   [2] mInstanceName          EAStringC
//   [3] mFlagsA                CIHState@29-30 / genProcDirty@24 / inCtor@27 /
//                              zombieCount@7-22 / dirty bits ...
//   [4] mFlagsB                createdOnFrame@18-31 / ...
//   [5] mpDisplayListPrevious  AptCIH*
//   [6] mpDisplayListNext      AptCIH*
//   [7] mpDisplayListParent    AptCIH*   (the parent node; ref-counted)
//   [8] mpCharacterInst        AptCharacterInst*
//   [9] mpAssetString          lazily-allocated asset-name string (freed by dtor)
//
// SCOPE: the node core -- ctor/dtor, the GC virtuals, and the state/flag/link/
// name accessors. The ~60 behavioural methods (tick / Process* / render /
// InsertChild/RemoveChild / the display-list ops / objectMember / _gotoAndX /
// the IsXInst type queries) are follow-ons.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // AptValueGC base
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // mInstanceName

struct AptCharacter;
struct AptCharacterInst;
struct AptMatrix;
struct AptCXForm;
struct AptNativeHash;

struct AptCIH : public AptValueGC
{
    EAStringC         mInstanceName;          // [2]
    uint32_t          mFlagsA;                // [3]
    uint32_t          mFlagsB;                // [4]
    AptCIH*           mpDisplayListPrevious;  // [5]
    AptCIH*           mpDisplayListNext;      // [6]
    AptCIH*           mpDisplayListParent;    // [7]
    AptCharacterInst* mpCharacterInst;        // [8]
    void*             mpAssetString;          // [9]

    AptCIH(AptCharacter* pCharacter, AptCIH* pParent);   // @0x824C7C
    virtual ~AptCIH();                                    // @0x804E68

    // ---- AptValue object-model virtual overrides --------------------------
    virtual void RegisterReferences();   // @0x7E9C78 (GC mark: parent + char props)
    virtual void DestroyGCPointers();    // @0x80553C (release parent + char inst)
    virtual void PreDestroy();           // @0x7ECBAC
    // FLAG: AptCIH also overrides GetHasClass/SetHasClass (@0x7E1E18/0x7E1DF0) +
    // objectMemberSet/Lookup; deferred (AptValue's defaults inherited for now).

    // ---- name -------------------------------------------------------------
    const EAStringC& GetInstanceName() const { return mInstanceName; }   // @0x7DF0A8
    void SetInstanceName(const EAStringC& strName) { mInstanceName = strName; }  // @0x7F56B4

    // ---- character instance / delegated visual reads ----------------------
    AptCharacterInst* GetCharacterInst() const { return mpCharacterInst; }  // @0x7DF174
    int16_t GetDepth() const;   // @0x7EA024 (through the char inst's render item)

    // ---- delegated mask / property reads (through the char inst) -----------
    AptNativeHash* GetNativeHash() const;   // @0x82AD5B28 (char inst's property hash)
    bool IsMask() const;                    // @0x82AD5BA0 (render item's mask flag)
    bool HasMask() const;                   // @0x82AD5BB8 (render item's has-mask flag)

    // ---- character-type predicates (through the char inst's type tag) -----
    // Each tests mpCharacterInst's type tag (AptCharacterInst::GetTypeTag); IsNone
    // tests this value's own AptValue vtable index (AptCIHNone == 37). Bodies in
    // AptCIH.cpp (need the full AptCharacterInst type).
    bool IsShapeInst() const;          // @0x82AD5A30  type 1
    bool IsDynamicTextInst() const;    // @0x82AD5A50  type 2
    bool IsButtonInst() const;         // @0x82AD5A10  type 4
    bool IsSpriteInst() const;         // @0x82AD59B0  type 5 or 16
    bool IsSpriteInstBase() const;     // @0x82AD59E0  type 5 or 9
    bool IsMorphInst() const;          // @0x82AD5A90  type 8
    bool IsAnimationInst() const;      // @0x82AD5AB0  type 9
    bool IsLevelInst() const;          // @0x82AD5AD0  type 15
    bool IsCustomControlInst() const;  // @0x82AD5B08  type 16
    bool IsNone() const;               // @0x82AD5AF0  AptValue vtable index 37 (AptCIHNone)

    // ---- display-list links -----------------------------------------------
    AptCIH* GetDisplayListPrevious() const { return mpDisplayListPrevious; }  // @0x7DF184
    AptCIH* GetDisplayListNext() const     { return mpDisplayListNext; }      // @0x7DF17C
    AptCIH* GetDisplayListParent() const   { return mpDisplayListParent; }    // @0x7DF1B4
    void SetDisplayListPrevious(AptCIH* p) { mpDisplayListPrevious = p; }     // @0x7DF1C4
    void SetDisplayListNext(AptCIH* p)     { mpDisplayListNext = p; }         // @0x7DF1BC
    void SetDisplayListParent(AptCIH* p)   { mpDisplayListParent = p; }       // @0x7DF1CC

    // ClearCIH @0x82AC... (X360-attested behavioural follow-on; body in its own
    // TU) -- tear down this node's character instance / placed state. Declared so
    // the display-list teardown (AptDisplayList::clear) can call it by name.
    void ClearCIH(bool bClearGCRoots);

    // ---- packed state / flags (mFlagsA bit-fields) -----------------------
    uint32_t GetCIHState() const;       void SetCIHState(uint32_t eState);    // @0x7DF12C/0x7DF110
    int16_t  GetZombieCount() const;    void IncZombieCount();  void DecZombieCount();  // @0x7DF160/0x7DF138/0x7FB648
    bool     IsInCtor() const;          void SetInCtor(uint32_t b);           // @0x7DF0E8/0x7DF0F4
    int      GetCreatedOnFrame() const; void SetCreatedOnFrame(int nFrame);   // @0x7DF1D4/0x7DF1E4

    // ActionScript-changed flag (bit 31). @0x82AD50E8/0x82AD50C8
    bool GetASChanged() const  { return (mFlagsA >> 31) != 0; }
    void SetASChanged(bool b)  { mFlagsA = (mFlagsA & 0x7FFFFFFFu) | (b ? 0x80000000u : 0u); }
    // Dirty-state flag (bit 25). @0x82AD5C10 / @0x82AD76B8
    bool GetDirtyState() const { return ((mFlagsA >> 25) & 1u) != 0; }
    // SetDirtyState @0x82AD76B8 -- set/clear the dirty bit; container/leaf types
    // (shape/dyn-text/static-text) and the empty placeholder never carry it. When
    // dirtying with bPropagate, mark up the parent chain to the first dirty ancestor.
    void SetDirtyState(bool bDirty, bool bPropagate);
    // "In remove list" flag (bit 26). @0x82AD5188/0x82AD5170
    bool GetInRemList() const  { return ((mFlagsA >> 26) & 1u) != 0; }
    void SetInRemList(bool b)  { mFlagsA = (mFlagsA & 0xFBFFFFFFu) | (b ? 0x04000000u : 0u); }

    // ---- AptValue object-model overrides (mFlagsA bit 28) -----------------
    // @0x82AD7438/0x82AD7418 -- whether this CIH has an attached AS class.
    virtual bool GetHasClass() const override { return ((mFlagsA >> 28) & 1u) != 0; }
    virtual void SetHasClass(int b) override  { mFlagsA = (mFlagsA & 0xEFFFFFFFu) | (b ? 0x10000000u : 0u); }
};
