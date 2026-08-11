#pragma once

// ===========================================================================
// EATech Apt -- AptKey: the ActionScript "Key" object (keyboard / pad input).
//
// AptKey : AptValueWithHash -- the AS `Key` singleton value. Like the other
// scriptable input/value objects it is a property-bearing, garbage-collected
// value (AptValueGC base via AptValueWithHash) with a trailing class-flags word,
// and it lives as a GC root for the life of the VM (the ctor pins setGCRoot(1)).
//
// It overrides objectMemberLookup so that resolving a member of `Key` returns
// either an integer key-code constant (Key.ENTER / Key.LEFT / ... ) or one of
// its native methods (isDown / isToggled / getCode / getController /
// addListener / removeListener / getAnalogStickInfo / getAnalogTriggerInfo /
// getAscii). The native methods read the current input event off the Apt input
// state (the most-recent key/pad event + the per-pad analog snapshot).
//
// SHAPE + BODIES from the X360 ARTIST.XEX pseudocode + assembly:
//     AptKey::AptKey                         @ 0x82AF0980  (ctor; GC root)
//     AptKey::~AptKey                        @ 0x82AF0A38  (empty; base teardown)
//     AptKey::objectMemberLookup             @ 0x82AF8780
//     AptKey::operator new                   @ 0x82AE6800  (GC pool)
//     AptKey::operator delete                @ 0x82AF09E0  (GC pool)
//     AptKey::CleanNativeFunctions           @ 0x82AD6188  (static; pool teardown)
//     AptKey::sMethod_isDown                 @ 0x82ADC628
//     AptKey::sMethod_isToggled              @ 0x82AD6318
//     AptKey::sMethod_getCode                @ 0x82AECE98
//     AptKey::sMethod_getController          @ 0x82AECF30
//     AptKey::sMethod_addListener            @ 0x82ADC6E0
//     AptKey::sMethod_getAnalogStickInfo     @ 0x82AF55F0
//     AptKey::sMethod_getAnalogTriggerInfo   @ 0x82AF5748
//     AptKey::`vector deleting destructor'   @ 0x82AF0A48  (compiler thunk -- dropped)
//
// LAYOUT (sizeof = 32 / 0x20, pinned by the deleting-dtor thunk's
// `AptKey::operator delete(this, 32)`):
//   AptValueWithHash base ... 28 bytes (vtable + bitfield + AptNativeHash)
//   mClassFlags  uint32_t .... +0x1C   (class / implemented-object flags; the
//                                       same family as AptObject::mClassFlags)
//
// vtable object-type index = AptVFT_Key (16), confirmed by the ctor's
// `li r4, 0x10` argument to the AptValueWithHash base.
//
// removeListener / getAscii: their disassembly was NOT part of this TU's dossier
// (only their symbol addresses, referenced by objectMemberLookup). They are
// declared so the lookup can wire the cached AptNativeFunction; their bodies are
// their own follow-on leaves and are NOT fabricated here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"   // AptValueWithHash base + AptVFT_Key

class AptNativeFunction;   // SDKs/EATech/include/Apt/AptNativeFunction.h (the cached
                           // method singletons; held by pointer only)

class AptKey : public AptValueWithHash
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE6800 / delete @0x82AF09E0)
    // Both route through the garbage-collected value pool (gpGCPoolManager) and flip
    // the AptValueGC_MemItem "allocated" flag, exactly like the rest of the GC value
    // family (AptGlobal / AptObject). Defined out-of-line in AptKey.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct the AS `Key` value and pin it as a GC root.   @ 0x82AF0980
    AptKey();

    // dtor -- empty: the X360 stores the vtable then tail-calls the base teardown
    // (the embedded property hash destroys itself). @ 0x82AF0A38
    virtual ~AptKey();

    // ---- AptValue object-model virtual override ---------------------------
    // Resolve a member of `Key`: a key-code constant (boxed AptInteger) or one of
    // the native methods (each a lazily-built, GC-rooted AptNativeFunction
    // singleton); any unrecognised name returns null. @ 0x82AF8780
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;

    // Release the cached native-method singletons back to the pool (Apt shutdown /
    // pool clear). Static -- the singletons are class-wide. @ 0x82AD6188
    static void CleanNativeFunctions();

    // ---- native ActionScript methods (static; the apt VM call dispatch invokes
    // them with the script arg count). Each returns an AptValue* result. Their
    // addresses back the cached AptNativeFunction singletons.
    //   isDown        @0x82ADC628 -- is the last key event the queried key down?
    //   isToggled     @0x82AD6318 -- (stub) always returns the false singleton.
    //   getCode       @0x82AECE98 -- the last key event's virtual key code.
    //   getController @0x82AECF30 -- the last event's controller (pad) index.
    //   addListener   @0x82ADC6E0 -- register a Key listener object.
    //   getAnalogStickInfo    @0x82AF55F0 -- pad analog-stick snapshot Object.
    //   getAnalogTriggerInfo  @0x82AF5748 -- pad analog-trigger snapshot Object.
    static AptValue* sMethod_isDown();
    static AptValue* sMethod_isToggled();
    static AptValue* sMethod_getCode();
    static AptValue* sMethod_getController();
    static AptValue* sMethod_addListener(AptKey* pThis, int nArgCount);
    static AptValue* sMethod_getAnalogStickInfo();
    static AptValue* sMethod_getAnalogTriggerInfo();

    // removeListener / getAscii: bodies HOMED in AptKey.cpp (decompiled from the
    // PS3 DecFIGS EXTERNAL twins @0xF303D8 / @0xF3E078 -- see the .cpp headers).
    static AptValue* sMethod_removeListener(AptKey* pThis, int nArgCount);
    static AptValue* sMethod_getAscii();

private:
    // +0x1C -- class / implemented-object flags (the same bitfield family as
    // AptObject::mClassFlags). The ctor starts it cleared. Note: the X360 ctor only
    // partially clears the word (zero the low byte, clear bits 22-23 via
    // `rlwinm 0,10,7`) rather than zeroing it whole -- that is the optimizer's
    // codegen of a logical flags-zero-init on freshly pooled memory; modelled here
    // as the honest `mClassFlags = 0`, matching the committed AptObject precedent.
    uint32_t mClassFlags;   // +0x1C

    // Reserve room for this many native members in the base property hash. The
    // X360 ctor passes `8` (li r5, 8) to AptValueWithHash.
    static const int32_t KI_HASH_CAPACITY = 8;

    // The nine cached native-method values, shared by every `Key` reference (the AS
    // methods are class-wide singletons created on first objectMemberLookup,
    // GC-rooted, AddRef'd, and torn down by CleanNativeFunctions). X360 globals
    // off_8324E394 (isDown) / E398 (isToggled) / E39C (getCode) / E3A0 (getAscii) /
    // E3A4 (getController) / E3A8 (addListener) / E3AC (removeListener) /
    // E3B0 (getAnalogStickInfo) / E3B4 (getAnalogTriggerInfo).
    static AptNativeFunction* spMethod_isDown;                // off_8324E394
    static AptNativeFunction* spMethod_isToggled;             // off_8324E398
    static AptNativeFunction* spMethod_getCode;               // off_8324E39C
    static AptNativeFunction* spMethod_getAscii;              // off_8324E3A0
    static AptNativeFunction* spMethod_getController;         // off_8324E3A4
    static AptNativeFunction* spMethod_addListener;           // off_8324E3A8
    static AptNativeFunction* spMethod_removeListener;        // off_8324E3AC
    static AptNativeFunction* spMethod_getAnalogStickInfo;    // off_8324E3B0
    static AptNativeFunction* spMethod_getAnalogTriggerInfo;  // off_8324E3B4
};
