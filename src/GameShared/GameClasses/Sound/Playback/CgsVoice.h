#ifndef CGS_SOUND_PLAYBACK_CGSVOICE_H
#define CGS_SOUND_PLAYBACK_CGSVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<Content>
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"   // Content, ContentSpec, Object
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h" // ContentType, ContentClass

// NOTE: Voice derives from the CgsContent.h `Object` (vptr + mu32RefCount), NOT the
// CgsObject.h one -- including both in one TU is an ODR clash (two definitions of
// CgsSound::Playback::Object). The two share the identical X360 layout; CONDUCTOR
// note in CgsObject.h tracks the eventual fold of the two Object copies into one.

// ============================================================================
// CgsVoice.h  (HOME for CgsSound::Playback::Slot + the minimal Voice/PlayerVoice
// slice the Slot control surface reaches).
//
// A Slot is one content-binding point on a Voice: it owns a Handle<Content>, a
// pluggable ISlotImplementation (the type-specific play/stop/attach behaviour), and
// two latch bytes (attach state, playing flag). The reconstructed Slot methods
// (Attach/Detach/Play/Stop/HandleAttach/HandleDetach/Release) drive that surface.
//
// LAYOUT (X360 byte offsets, recovered from the method member reads; host-width FLAG:
// pointer/handle members widen on the 64-bit host, so members are pinned BY NAME +
// SEQUENCE and NO absolute-offset static_assert is emitted):
//   Slot:
//     +0x04  mpContentClass  const ContentClass*   (the class this slot accepts)
//     +0x08  mhContent       Handle<Content>       (currently bound content)
//     +0x0C  mpImpl          ISlotImplementation*  (type-specific behaviour)
//     +0x10  mu8Attach       u8                    (0=detached, 2=attached)
//     +0x11  mu8Playing      u8                    (0=stopped, 1=playing)
//
// FLAG: MINIMAL FLAGGED HOME. The full Voice / PlayerVoice hierarchy (their bases
// Object/Voice, refcount, factory ref, offset tables) is DEFERRED to the Voice
// keystone TU; here Voice/PlayerVoice carry only the members the Slot methods read
// (the Release slot-disposer walk reaches the disposer through raw offsets off the
// Voice, documented inline). ISlotImplementation/ISlotDisposer are the polymorphic
// collaborators dispatched through the slot; only the vtable slots the methods call
// are declared.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

class Slot;
class Voice;
class PlayerVoice;

// ---------------------------------------------------------------------------
// The type-specific slot behaviour, dispatched by the Slot control surface.
// DWARF (CgsVoice.h): DoPlay/DoStop are slots 0/1, DoPostAttach/DoPreDetach are
// slots 3/4 (byte 0xC/0x10). Declared-only -- concrete implementations own their TU.
// ---------------------------------------------------------------------------
class ISlotImplementation
{
public:
    virtual ~ISlotImplementation() {}

    virtual bool DoPlay(Slot& arSlot, PlayerVoice& arVoice, Content& arContent,
                        u32 au32Param) = 0;                     // slot 0
    virtual bool DoStop(Slot& arSlot, PlayerVoice& arVoice, Content& arContent) = 0; // slot 1
    virtual void DoReserved2() = 0;                             // slot 2 (unused here)
    virtual void DoPostAttach(Slot& arSlot, Voice& arVoice, Content& arContent) = 0; // slot 3
    virtual void DoPreDetach(Slot& arSlot, Voice& arVoice, Content& arContent) = 0;  // slot 4
};

// ---------------------------------------------------------------------------
// The slot disposer the Voice's owning module hands a released slot back to. The
// request is a 20-byte block { mpImpl, 0, 0, 0, 0 }; DisposeSlot is vtable slot 5
// (byte 0x14).
// ---------------------------------------------------------------------------
struct SlotDisposeRequest
{
    ISlotImplementation* mpImpl;   // +0x00
    u32                  mau32Reserved[4];  // +0x04..0x13 (zeroed)
};

class ISlotDisposer
{
public:
    virtual ~ISlotDisposer() {}
    virtual void DoReserved0() = 0;
    virtual void DoReserved1() = 0;
    virtual void DoReserved2() = 0;
    virtual void DoReserved3() = 0;
    virtual void DisposeSlot(SlotDisposeRequest* apRequest) = 0;   // slot 5 / byte 0x14
};

// ---------------------------------------------------------------------------
// Minimal Playback::Voice / PlayerVoice slice. Voice derives from Object; the Slot
// Release path reaches the slot disposer through raw offsets off the Voice (mFactory
// @+8 -> +0xC -> +0x30), documented at the call site. Full hierarchy DEFERRED.
// ---------------------------------------------------------------------------
class Voice : public Object
{
    // FLAG: the Voice header words the Slot::Release disposer walk reads (mFactory
    // @+8 and onward) are DEFERRED to the Voice keystone; the walk stays a raw,
    // offset-faithful load sequence in CgsVoice.cpp rather than fabricating them.
};

class PlayerVoice : public Voice
{
};

// ---------------------------------------------------------------------------
// CgsVoice.h. Slot -- one content-binding point on a Voice.
// ---------------------------------------------------------------------------
class Slot
{
public:
    // @ 0x826C77F8. Bind ahContent iff its ContentClass matches mpContentClass.
    bool Attach(Voice& arVoice, Handle<Content> ahContent);

    // Drop any currently-bound content (declared-only; bodied in the Voice TU).
    void Detach(Voice& arVoice);

    // @ 0x82693428 / 0x826934F8. Start/stop playback through the impl.
    bool Play(PlayerVoice& arVoice, u32 au32Param);
    bool Stop(PlayerVoice& arVoice);

    // @ 0x826935D0 / 0x82693690. Commit the attach/detach latch through the impl.
    void HandleAttach(Voice& arVoice);
    void HandleDetach(Voice& arVoice);

    // @ 0x826C0810. Detach then hand this slot's impl to the Voice's slot disposer.
    void Release(Voice& arVoice);

private:
    u8                   mau8Pad0[4];    // +0x00
    const ContentClass*  mpContentClass; // +0x04  class this slot accepts
    Handle<Content>      mhContent;      // +0x08  currently-bound content
    ISlotImplementation* mpImpl;         // +0x0C  type-specific behaviour
    u8                   mu8Attach;      // +0x10  0=detached, 2=attached
    u8                   mu8Playing;     // +0x11  0=stopped, 1=playing
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSVOICE_H
