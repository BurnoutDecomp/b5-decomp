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

    // ---- packed state / flags --------------------------------------------
    uint32_t GetCIHState() const;       void SetCIHState(uint32_t eState);    // @0x7DF12C/0x7DF110
    int16_t  GetZombieCount() const;    void IncZombieCount();  void DecZombieCount();  // @0x7DF160/0x7DF138/0x7FB648
    bool     IsInCtor() const;          void SetInCtor(uint32_t b);           // @0x7DF0E8/0x7DF0F4
    int      GetCreatedOnFrame() const; void SetCreatedOnFrame(int nFrame);   // @0x7DF1D4/0x7DF1E4
};
