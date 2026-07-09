#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterSpriteInstBase: the base of the sprite/movie-clip
// character instances.
//
// A character of type 5 (the Flash sprite / movie-clip) gets one of these from
// AptCharacterInst::CreateCharacterInst (@0x82AFFF70: `if (mnType == 5)` ->
// Allocate(36) -> AptCharacterSpriteInstBase::AptCharacterSpriteInstBase). It is
// the shared base for the concrete sprite instances -- AptCharacterSpriteInst and
// AptCharacterAnimationInst both derive from it and chain to its ctor/dtor.
//
// It extends the render-spine instance AptCharacterInst (AptCIH -> AptCharacterInst
// -> AptRenderItem) with the movie-clip's own per-instance state: a pending
// goto-frame, the packed clip-event/state flags word, and the embedded child
// display list (AptDisplayList) the clip places its sub-characters into. Its ctor
// also brings up the instance's property hash (the base mpProperties slot) and
// wires that hash's __proto__ to the registered sprite class's prototype.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (only forward
// declarations survive in the Feb-2007 AptValue.h). SHAPE + BODIES are therefore
// reconstructed STRICTLY from the X360 ARTIST.XEX (the authoritative spine), like
// the sibling X360-only Apt classes (AptCIHNone / AptPseudoDisplayList):
//     AptCharacterSpriteInstBase::AptCharacterSpriteInstBase  @ 0x82AFF820
//     AptCharacterSpriteInstBase::~AptCharacterSpriteInstBase @ 0x82AFF910
//     AptCharacterSpriteInstBase::PreDestroy                  @ 0x82AFE9E8
//     AptCharacterSpriteInstBase::HasClipAction               @ 0x82AD50B8
//     AptCharacterSpriteInstBase::SetClipAction               @ 0x82AD5080
//     AptCharacterSpriteInstBase::RemoveClipAction            @ 0x82AD5098
//     AptCharacterSpriteInstBase::SetCreatedDynamic           @ 0x82AE4438
//     AptCharacterSpriteInstBase::`vector deleting destructor'@ 0x82B00110 (thunk -- dropped)
//
// LAYOUT (sizeof = 36 / 0x24, pinned by CreateCharacterInst's Allocate(36) and
// AptCharacterSpriteInst::`scalar deleting destructor''s Deallocate(.,36)):
//   AptCharacterInst base ......... 16 bytes (vtable + mpRenderItem + mTypeFlags +
//                                             mpProperties) -- the ctor fills the
//                                             inherited mpProperties slot itself.
//   +0x10  mnGotoFrame      int32  pending goto-frame target; the ctor sets it to
//                                  -1 (no pending goto). [role inferred from the
//                                  -1 sentinel + the sprite/movie-clip context;
//                                  not read within this TU's eight functions.]
//   +0x14  mnClipActionFlags u32   packed: the HIGH 24 bits (>>8) are the AS
//                                  clip-event-handler mask (Has/Set/RemoveClipAction);
//                                  the LOW 8 bits are sprite state flags, which the
//                                  ctor seeds to 0xC0 (bits 6,7).
//   +0x18  mpClipEventHandlers  ptr  the registered clip-event-handler list (null-
//                                  init). RECONCILED from int32 mnCurrentFrame: the
//                                  slot is only ever zero-initialised, never read as a
//                                  frame, and AptDisplayList::_addToSetCaches walks it
//                                  as the handler-list pointer.
//   +0x1C  mDisplayList     AptDisplayList  the child display list (4 bytes), the
//                                  ctor default-constructs it (pool head node).
//   +0x20  mnLastActionFrame int32 frame bookkeeping; the ctor zeroes it. [role
//                                  inferred; only zero-initialised here.]
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCharacterInst base
#include "SDKs/EATech/include/Apt/AptDisplayList.h"      // embedded child display list (by value)

// One registered AS clip-event handler. This IS the serialised clipActions record
// from the place command's clipActions block (the placement stores the block pointer
// into mpClipEventHandlers): console 12-byte {mask, keyId, streamPtr}; GUIAPT64
// stride-16 with the 8-byte relocated+parsed action-stream pointer at +0x08 (the
// exact records AptMovie::resolve64 case-3 relocates + _parseStream parses).
// queueClipEvents scans the masks and enqueues &mpActionStream (the slot ADDRESS)
// as the action's event-stream slot; RunActions dereferences it at drain time.
struct AptClipEventHandler
{
    uint32_t mnEventFlags;                 // +0x00  packed event mask (tested & 0x201C7 / & 0x200C0)
    uint32_t mnKeyFrameId;                 // +0x04  keyframe id (mask 0x20000: matches frameId>>17)
    const unsigned char* mpActionStream;   // +0x08  the parsed action stream (native-8 slot)
};

// The sprite instance's registered-clip-event-handler list -- the placement's
// clipActions BLOCK view, the XB1 native-8 layout: {i32 count @0; pad;
// recArray ptr 8-aligned @+0x08}. (The 4-packed "converter accommodation" view
// matched the retired apt_convert data; the GUIAPT64 drive set is uniformly
// XB1-form now -- apt8_repair.py normalizes every naturally-packed clipActions
// block -- so the faithful natural layout is restored, 2026-07-09.)
struct AptClipEventHandlerList
{
    int32_t             mnCount;       // +0x00
    AptClipEventHandler* mpHandlers;   // +0x08 (natural 8-aligned, XB1-form)
};

struct AptCharacterSpriteInstBase : public AptCharacterInst
{
    int32_t        mnGotoFrame;        // +0x10  (-1 = none)
    uint32_t       mnClipActionFlags;  // +0x14  high 24 bits = clip-event mask
    // +0x18: the registered clip-event-handler list (null until handlers are set).
    // RECONCILED: this slot was modelled as `int32_t mnCurrentFrame`, but it is only
    // ever zero-initialised (never read as a frame) and AptDisplayList::_addToSetCaches
    // dereferences it as a pointer to the handler list -- so it is the handler-list
    // pointer, not a frame counter.
    AptClipEventHandlerList* mpClipEventHandlers;   // +0x18
    AptDisplayList mDisplayList;       // +0x1C  child display list (4 bytes)
    int32_t        mnLastActionFrame;  // +0x20

    // @0x82AFF820 -- construct a sprite/movie-clip instance over pCharacter:
    // chain the AptCharacterInst base ctor, default-construct the child display
    // list, seed the frame/flags state, allocate the property hash into the
    // inherited mpProperties slot and wire its __proto__ to the registered sprite
    // class's prototype.
    explicit AptCharacterSpriteInstBase(AptCharacter* pCharacter);

    // @0x82AFF910 -- reset the vtable, destroy the embedded display list, then
    // chain to ~AptCharacterInst. NOT a C++ `virtual` (the AptCharacterInst family
    // models its console vtable as the manual mpVTable_unused member at [0], so the
    // base ~AptCharacterInst is non-virtual too); declaring it virtual here would
    // add a second, C++-synthesised vtable pointer and diverge from the base's
    // manual-vtable layout. The destructor still chains the embedded mDisplayList +
    // the AptCharacterInst base teardown.
    ~AptCharacterSpriteInstBase();

    // @0x82AFE9E8 -- pre-teardown: forward to the embedded display list's
    // PreDestroy (release the placed children before the GC tears down). Non-virtual
    // for the same reason as the destructor (manual-vtable family).
    void PreDestroy();

    // ---- packed clip-event handlers (the high 24 bits of mnClipActionFlags) ----
    // @0x82AD50B8 -- true (nMask-masked) when any of the queried clip-event bits
    // are registered.
    int32_t HasClipAction(int32_t nMask) const;     // @0x82AD50B8
    // @0x82AD5080 -- register the nMask clip-event bits.
    AptCharacterSpriteInstBase* SetClipAction(int32_t nMask);      // @0x82AD5080
    // @0x82AD5098 -- clear the nMask clip-event bits.
    AptCharacterSpriteInstBase* RemoveClipAction(int32_t nMask);   // @0x82AD5098

    // @0x82AE4438 -- flag the owned render item as created dynamically (the
    // createEmptyMovieClip path): set/clear the render item's "created dynamic"
    // flag bit through the writable render item.
    void SetCreatedDynamic(bool bDynamic);          // @0x82AE4438
};
