#ifndef CGS_SOUND_PLAYBACK_CGSVOICE_H
#define CGS_SOUND_PLAYBACK_CGSVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<Content>
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"   // Content, ContentSpec, Object
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"    // Name
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h" // ContentType, ContentClass

namespace rw { namespace audio { namespace core { class System; } } }   // the Update-context alias below

// NOTE (fold DONE 2026-08-25, audio-faithfulness wave 3): there is now ONE
// CgsSound::Playback::Object -- the canonical CgsObject.h home, which CgsContent.h
// includes (its old private copy is deleted). Voice derives from that single Object.

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

// Fwd decls for the grown Voice surface (each bodied in its own TU).
// (2026-08-25, phase B4: the former opaque `class System;` fwd decl is now the
// REAL alias -- the per-frame Update context IS the engine System, and the
// environment's UpdateVoices hands the same pointer to both.)
typedef rw::audio::core::System System;
struct SubmixVoice;    // Connect target (Voice subclass, own TU; struct-keyed to match its CgsSubmixVoice.h definition -- a class-keyed fwd mangles divergent symbols)
struct VoiceSpec;      // referenced by ctor/GetAllocationSize (own TU; struct-keyed to match CgsDataStructures.h)
class Factory;         // owning module factory (own TU)

// DWARF CgsVoice.h:43 / :60. Voice playback / removal lifecycle enums.
enum EPlaybackState
{
    E_PLAYBACK_STATE_INVALID = 0,
    E_PLAYBACK_STATE_STOPPED = 1,
    E_PLAYBACK_STATE_PLAYING = 2,
    E_PLAYBACK_STATE_COUNT   = 3,
    E_PLAYBACK_STATE_CHANGED = 128
};

enum EVoiceRemoveState
{
    E_VOICE_REMOVE_INVALID  = 0,
    E_VOICE_REMOVE_ALIVE    = 1,
    E_VOICE_REMOVE_REMOVING = 2,
    E_VOICE_REMOVE_REMOVED  = 3
};

// CgsVoice.h:435 (DWARF). One aux send: a named gain that flags when it changes.
// 12 bytes { Name, f32, bool(pad) }.
struct Send
{
    Name  GetName() const  { return mName; }
    f32   Get() const      { return mf32Gain; }
    void  Set(f32 af32Gain) { mbHasChanged = (mf32Gain != af32Gain); mf32Gain = af32Gain; }
    bool  HasChanged() const { return mbHasChanged; }

    Name mName;         // +0x00
    f32  mf32Gain;      // +0x04
    bool mbHasChanged;  // +0x08   (pad to 12)
};

// CgsVoice.h:515 (DWARF). One input parameter: named [Min,Max] range + current
// value + dirty flag. 20 bytes { Name, min, max, value, bool(pad) }.
struct InputParameter
{
    Name  GetName() const     { return mName; }
    f32   GetValueRaw() const  { return mf32Value; }   // @0xC (Get/SetParameter)
    f32   GetMin() const       { return mf32Min; }
    f32   GetMax() const       { return mf32Max; }
    void  SetValueRaw(f32 af32V) { mf32Value = af32V; } // stfs @0xC
    void  SetChanged(bool ab)    { mbHasChanged = ab; } // stb  @0x10
    bool  HasChanged() const   { return mbHasChanged; }

    Name mName;         // +0x00
    f32  mf32Min;       // +0x04
    f32  mf32Max;       // +0x08
    f32  mf32Value;     // +0x0C
    bool mbHasChanged;  // +0x10   (pad to 20)
};

// CgsVoice.h:608 (DWARF). One output parameter: named value + old value. 12 bytes.
struct OutputParameter
{
    Name  GetName() const  { return mName; }
    f32   Get() const      { return mf32Value; }   // *(p+4)
    bool  IsChanging() const { return mf32Value != mf32OldValue; }
    f32*  GetAddress()     { return &mf32Value; }
    void  Update()         { mf32OldValue = mf32Value; }

    Name mName;          // +0x00
    f32  mf32Value;      // +0x04
    f32  mf32OldValue;   // +0x08
};

// ---------------------------------------------------------------------------
// The type-specific slot behaviour, dispatched by the Slot control surface.
// DWARF (CgsVoice.h:94, full hook list :101/:110/:121/:130/:136): DoPlay/DoStop
// are slots 0/1, DoUpdatePlaying is slot 2, DoPostAttach/DoPreDetach are slots
// 3/4 (byte 0xC/0x10). (2026-08-25 audio-faithfulness wave 6: reconciled to the
// DWARF -- the Slot params are CONST refs, and slot 2 is DoUpdatePlaying, not a
// reserved hole; the three content-slot implementers -- GenericRwacContentSlot /
// AemsContentSlot / SplicerContentSlot -- all override slots 0-2, and only the
// splicer overrides DoPreDetach, so DoPostAttach/DoPreDetach are NON-pure on the
// console base. FLAG: their base default bodies are un-attested; modelled as
// no-ops, the only behaviour consistent with implementers not overriding them.)
// ---------------------------------------------------------------------------
class ISlotImplementation
{
public:
    virtual ~ISlotImplementation() {}

    virtual bool DoPlay(const Slot& arSlot, PlayerVoice& arVoice, Content& arContent,
                        u32 au32Param) = 0;                     // slot 0 (DWARF :101)
    virtual bool DoStop(const Slot& arSlot, PlayerVoice& arVoice, Content& arContent) = 0; // slot 1 (:110)
    virtual bool DoUpdatePlaying(System* apSystem, const Slot& arSlot, PlayerVoice& arVoice,
                                 Content& arContent, f32 af32Dt) = 0; // slot 2 (:121)
    virtual void DoPostAttach(const Slot& arSlot, Voice& arVoice, Content& arContent) {} // slot 3 (:130)
    virtual void DoPreDetach(const Slot& arSlot, Voice& arVoice, Content& arContent) {}  // slot 4 (:136)
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
// ---------------------------------------------------------------------------
// CgsSound::Playback::Voice -- one playing sound instance. Owns the tail-allocated
// slot / send / input-parameter / output-parameter tables (their base offsets kept
// in mOffsets) plus the playback + removal lifecycle bytes. DWARF CgsVoice.h is
// authoritative for the layout + method shapes below.
//
// FLAG (host-width): pointer/reference members (mFactory) widen on the 64-bit host,
// so members are pinned BY NAME + SEQUENCE and NO absolute-offset static_assert is
// emitted. GetSlot/GetSend/GetInputParameter random-access the tail tables at
//   (u8*)this + mOffsets.mu<Kind>Offset + sizeof(<Kind>) * index.
// ---------------------------------------------------------------------------
class Voice : public Object
{
public:
    // DWARF CgsVoice.h:957.
    enum EProfileVoiceType
    {
        E_VOICETYPE_AEMS = 0, E_VOICETYPE_SPLICER = 1,
        E_VOICETYPE_GENERICRWACMASTER = 2, E_VOICETYPE_GENERICRWACSUBMIX = 3,
        E_VOICETYPE_GENERICRWACPLAYER = 4, E_VOICETYPE_COUNT = 5
    };

    // CgsVoice.h:1139. Table offsets embedded in the Voice tail allocation.
    struct Offsets
    {
        u16 muSlotOffset;             // +0x00 (Voice +0x24)
        u16 muSendOffset;             // +0x02 (Voice +0x26)
        u16 muInputParameterOffset;   // +0x04 (Voice +0x28)
        u16 muOutputParameterOffset;  // +0x06 (Voice +0x2A)
    };

    virtual ~Voice();                                            // vtable 0x00
    virtual f32   GetCpuTicks();                                 // vtable 0x04
    virtual void  DisplayVoiceCpu(f32*, f32*, f32, bool);        // vtable 0x08
    virtual EProfileVoiceType GetProfileVoiceType();            // vtable 0x0C
    virtual void  DoDispose();                                   // vtable 0x10
    virtual void  DoUpdate(System* apSystem, f32 af32DeltaTime); // vtable 0x14
    virtual bool  DoConnectSend(u32 au32Index, SubmixVoice* apSubmix); // vtable 0x18
    virtual bool  DoRemove();                                    // vtable 0x1C

    // @ 0x826ACC90. Resolve akName to a Send, then DoConnectSend the submix handle.
    bool Connect(Name akName, Handle<SubmixVoice>& arhSubmix);

    // @ 0x826ACAF8 / B68 / BD8 / CC30. Name -> descriptor (0 if absent).
    Slot*            FindNamedSlot(Name akName);
    Send*            FindNamedSend(Name akName);
    InputParameter*  FindNamedInputParameter(Name akName);
    OutputParameter* FindNamedOutputParameter(Name akName);

    // @ 0x826ACE60 / EC8 / F30. Name -> index (u32(-1) if absent).
    u32 GetIndexOfSlot(Name akName) const;
    u32 GetIndexOfInputParameter(Name akName) const;
    u32 GetIndexOfOutputParameter(Name akName) const;

    // @ 0x826ACDC8 / D38. Parameter value access by name / by index.
    f32  GetParameter(Name akName);
    void SetParameter(s32 as32Index, f32 af32Value, Name akParamName);   // DWARF: void / Name by value

    // @ 0x82680E18. Set low-7-bit playback state, raising 0x80 CHANGED iff it moves.
    void SetPlaybackState(EPlaybackState aeState);

    // Masked playback-state read. INLINE in the original (every Logic::Voice caller
    // reads `*(obj+0x10) & 0x7F`, and the Destruct assert string names it verbatim:
    // "E_PLAYBACK_STATE_PLAYING != GetPlaybackState()" @0x826C4E38) -- so the method
    // existed and this is its recovered shape, not an invention.
    EPlaybackState GetPlaybackState() const
    {
        return static_cast<EPlaybackState>(mu8PlaybackState & 0x7F);
    }

    // DWARF CgsVoice.h:1454 / :1461 -- the CHANGED-bit pair (test / clear of the
    // +0x10 0x80 bit). The environment's UpdateVoices @0x826C01B8 drives the
    // clear each frame after the per-voice tick.
    bool HasPlaybackStateChanged() const
    {
        return (mu8PlaybackState & E_PLAYBACK_STATE_CHANGED) != 0;
    }
    void AcknowledgePlaybackStateChange()
    {
        mu8PlaybackState &= 0x7F;
    }

    // Ident read. INLINE in the original (Logic::Voice::GetIdent @0x826AD988 returns
    // `*(obj+12)` -- this member by name).
    u32 GetIdent() const { return mIdent; }

    // Ident write (DWARF CgsVoice.h:567, SetIdent(StreamBuffer::Ident)). INLINE in
    // the original -- Module::CreateVoice @0x826D7B00 stores `*(voice+12)` after a
    // successful factory CreateVoice; this member by name.
    void SetIdent(u32 au32Ident) { mIdent = au32Ident; }

    // @ (DWARF CgsVoice.h:501). Resolve akName to a Slot, then Slot::Attach the
    // content handle. FLAG (DEFER): declared-only -- bodied with the Voice slices
    // (caller: Module::AttachVoice @0x826D7D80).
    bool Attach(Name akName, Handle<Content>& arhContent);

    // Owning-factory read. INLINE in the original (Environment::GetR @0x826BFE50
    // reads `*(voice+8)` then the factory's name word -- this member by name).
    const Factory& GetFactory() const { return mFactory; }

    // Removal-lifecycle byte access. INLINE in the original (Logic::Voice::Destruct
    // @0x826C4E38 stores `*(obj+0x11) = 2` == E_VOICE_REMOVE_REMOVING; Play/Stop
    // @0x826C5068/0x826C5148 test `*(obj+0x11) == 1` == E_VOICE_REMOVE_ALIVE).
    EVoiceRemoveState GetRemoveState() const
    {
        return static_cast<EVoiceRemoveState>(mu8RemoveState);
    }
    void SetRemoveState(EVoiceRemoveState aeState)
    {
        mu8RemoveState = static_cast<u8>(aeState);
    }

    // @ 0x826A24F0. Per-frame tick (updates slots + DoUpdate, or drives removal).
    void Update(System* apSystem, f32 af32DeltaTime);

    // Random-access the tail tables (bounds-assert index < count). Only
    // GetOutputParameter @0x82693A20 is bodied in this batch; the others are declared
    // here and bodied in the Voice keystone TU (same GetOffsetObject<T> pattern).
    Slot&            GetSlot(u32 au32Index) const;
    Send&            GetSend(u32 au32Index) const;
    InputParameter&  GetInputParameter(u32 au32Index) const;
    OutputParameter& GetOutputParameter(u32 au32Index) const;   // @ 0x82693A20

private:
    const Factory& mFactory;                    // +0x08
    u32            mIdent;                       // +0x0C  (Module::StreamBuffer::Ident)
    u8             mu8PlaybackState;             // +0x10  EPlaybackState | 0x80 CHANGED
    u8             mu8RemoveState;               // +0x11  EVoiceRemoveState
    u32            mu32SlotCount;                // +0x14
    u32            mu32SendCount;                // +0x18
    u32            mu32InputParameterCount;      // +0x1C
    u32            mu32OutputParameterCount;     // +0x20
    Offsets        mOffsets;                     // +0x24 (muOutputParameterOffset @+0x2A)

    Voice& operator=(const Voice&);              // CgsVoice.h:1157 (declared-only)
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

    // Per-frame slot tick, driven by Voice::Update @0x826A24F0 (asm arg regs:
    // r3=slot(this), r4=System*, r5=Voice&, r6=PlayerVoice&, f1=dt). FLAG (DEFER):
    // declared-only -- bodied in the Voice keystone TU.
    void Update(System* apSystem, Voice& arVoice, PlayerVoice& arPlayerVoice, f32 af32DeltaTime);

    // The interned name this slot answers to (the Voice name-lookup loops
    // FindNamedSlot/GetIndexOfSlot compare against it). FLAG (additive grow, by-name):
    // the slot's first word (X360 +0x00, previously modelled as an anonymous pad) is
    // the slot Name; exposed by NAME. Host-width: Name widens on the 64-bit host.
    Name GetName() const { return mName; }

private:
    Name                 mName;          // +0x00  interned slot name
    const ContentClass*  mpContentClass; // +0x04  class this slot accepts
    Handle<Content>      mhContent;      // +0x08  currently-bound content
    ISlotImplementation* mpImpl;         // +0x0C  type-specific behaviour
    u8                   mu8Attach;      // +0x10  0=detached, 2=attached
    u8                   mu8Playing;     // +0x11  0=stopped, 1=playing
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSVOICE_H
