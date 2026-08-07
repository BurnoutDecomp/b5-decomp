// ===========================================================================
// EATech Apt -- AptKey: the ActionScript "Key" object (keyboard / pad input).
//
// Reconstructed from the X360 ARTIST.XEX pseudocode + assembly:
//     AptKey::AptKey                         @ 0x82AF0980
//     AptKey::~AptKey                        @ 0x82AF0A38
//     AptKey::objectMemberLookup             @ 0x82AF8780
//     AptKey::operator new                   @ 0x82AE6800
//     AptKey::operator delete                @ 0x82AF09E0
//     AptKey::CleanNativeFunctions           @ 0x82AD6188
//     AptKey::sMethod_isDown                 @ 0x82ADC628
//     AptKey::sMethod_isToggled              @ 0x82AD6318
//     AptKey::sMethod_getCode                @ 0x82AECE98
//     AptKey::sMethod_getController          @ 0x82AECF30
//     AptKey::sMethod_addListener            @ 0x82ADC6E0
//     AptKey::sMethod_getAnalogStickInfo     @ 0x82AF55F0
//     AptKey::sMethod_getAnalogTriggerInfo   @ 0x82AF5748
// See AptKey.h for the shape / layout derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptKey.h"

#include "SDKs/EATech/include/Apt/AptObject.h"             // AptObject::Create / operator new
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH::mFlagsA (the listener marker test)
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"     // the cached native methods
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // Set on the built Object
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // toInteger / setGCRoot
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"   // AptBoolean::Create (the true/false singletons)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC keys
#include "SDKs/EATech/Apt/AptKeyMembersIndex.h"            // KeyMembersIndex::in_word_set
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"         // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"           // AptValueGC_MemItem
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager::Allocate/Deallocate

#include <new>        // placement new (the lazily-built method singletons)
#include <ctype.h>    // toupper (the key-code case fold)

// ---------------------------------------------------------------------------
// FLAG (homed by the AS-globals layer, not yet reconstructed): the shared AS
// "undefined" value `Key.addListener` returns (X360 global off_8324D814). Null
// until the AS globals are built.
// ---------------------------------------------------------------------------
extern AptValue* gpUndefinedValue;   // off_8324D814

// ---------------------------------------------------------------------------
// FLAG (homed by the apt VM native-call dispatch, not yet reconstructed): the
// native ActionScript method argument stack. isDown / addListener read their
// single argument off the top of this stack (X360 globals dword_8324E760 =
// count, off_8324E768 = array). Modelled as named externs (the project rule for
// module statics without a home yet); the dispatch layer pushes the call args
// here before invoking the method.
// ---------------------------------------------------------------------------
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt input subsystem, not yet reconstructed): the packed
// most-recent input event the `Key` methods report on (X360 dword_8324E7A8).
// Bit layout recovered from the X360 shifts/masks:
//   bits [0..1]   event type      (1 == a key/button DOWN event)   (& 3)
//   bits [2..9]   controller index ( >> 2  & 0xFF )
//   bits [17..31] virtual key code ( >> 17 )
// Zero when no event is pending. Defined here (null until the input layer wires
// it) so the `Key` methods compile and read the event by name.
// ---------------------------------------------------------------------------
extern uint32_t gAptKeyLastEvent;   // dword_8324E7A8

// ---------------------------------------------------------------------------
// Virtual-key-code translation table (X360 dword_82143400, 20 entries). Maps the
// low pad/extended scancodes [0..19] to their Windows virtual-key codes; -1 means
// "no mapping". Used by getCode / isDown to translate a raw low code (< 0x14) that
// is not an ASCII-range printable.
// PROVENANCE: the asm loads dword_82143400 (getCode @0x82AECED0, isDown
// @0x82ADC690) but the current dossier does not dump the .rdata bytes. The values
// below are the canonical Scaleform/Flash `Key` scancode->VK mapping (LEFT/RIGHT/
// HOME/END/INS/DEL/BACKSPACE/ENTER/UP/DOWN/PGUP/PGDN/TAB/ESC), which AptKey (an
// EATech Scaleform-GFx workalike) uses; every non-sentinel entry is the exact VK
// code for that slot and the -1s sit where Flash has no mapping. TODO: confirm the
// 20 words byte-for-byte against the raw X360 .rdata when a dump is available.
// ---------------------------------------------------------------------------
static const int32_t kAptKeyCodeTranslate[20] =
{
    -1, 37, 39, 36, 35, 45, 46, -1,  8, -1,   // [0..9]
    -1, -1, -1, 13, 38, 40, 33, 34,  9, 27    // [10..19]
};

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt input subsystem, not yet reconstructed): the per-pad
// analog snapshot tables the getAnalog* methods read (X360 .data; populated by
// the input layer each frame). Each is an array of 16-byte (4-float) records
// indexed by the pad index:
//   gAptKeyAnalogStickL / gAptKeyAnalogStickR (unk_8324D750 / unk_8324E2D8) --
//       the left / right stick {x, y, ...} per pad (getAnalogStickInfo selects
//       by the event's stick id 501/502 and reads [0]=x, [1]=y).
//   gAptKeyAnalogTrigger (flt_8324E200) -- the {leftTrigger, rightTrigger, ...}
//       per pad (getAnalogTriggerInfo reads [0]=left, [1]=right).
// Declared as named externs (null/zero until the input layer fills them) so the
// methods read the snapshot by name rather than by raw console offset.
// ---------------------------------------------------------------------------
extern const float gAptKeyAnalogStickL[];     // unk_8324D750  (stick id 501)
extern const float gAptKeyAnalogStickR[];     // unk_8324E2D8  (stick id 502)
extern const float gAptKeyAnalogTrigger[];    // flt_8324E200

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt input subsystem, not yet reconstructed): the interned
// "controller" property key the getAnalog* methods store the pad index under
// (X360 unk_8324E614, an EAStringC constant). Declared extern so Set compiles.
// ---------------------------------------------------------------------------
extern const EAStringC gAptKeyControllerKey;   // unk_8324E614

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt input subsystem, not yet reconstructed): registering a
// Key listener. addListener's X360 body reaches the input/Key manager
// (off_8324E574) and walks a small membership vector ({count, array} at a fixed
// offset) to add pListener iff it is not already registered. The manager + its
// listener-vector type are not yet reconstructed, so the raw-offset walk is
// expressed here through this named helper (declared extern, defined by the input
// TU); it performs the "add if absent" the X360 inlines.
// ---------------------------------------------------------------------------
extern void AptKeyManagerAddListener(AptValue* pListener);

// ---- the nine class-wide cached native-method singletons -------------------
AptNativeFunction* AptKey::spMethod_isDown               = 0;   // off_8324E394
AptNativeFunction* AptKey::spMethod_isToggled            = 0;   // off_8324E398
AptNativeFunction* AptKey::spMethod_getCode              = 0;   // off_8324E39C
AptNativeFunction* AptKey::spMethod_getAscii             = 0;   // off_8324E3A0
AptNativeFunction* AptKey::spMethod_getController        = 0;   // off_8324E3A4
AptNativeFunction* AptKey::spMethod_addListener          = 0;   // off_8324E3A8
AptNativeFunction* AptKey::spMethod_removeListener       = 0;   // off_8324E3AC
AptNativeFunction* AptKey::spMethod_getAnalogStickInfo   = 0;   // off_8324E3B0
AptNativeFunction* AptKey::spMethod_getAnalogTriggerInfo = 0;   // off_8324E3B4

// ---------------------------------------------------------------------------
// operator new @ 0x82AE6800
//
// Allocate the AptKey block from the GC pool and mark its AptValueGC_MemItem
// "allocated" flag -- exactly the AptGlobal / AptObject GC-new path. The X360
// calls DOGMA_PoolManager::Allocate directly on the GC pool (off_8324D834 ==
// gpGCPoolManager) then SetIsAllocated(block, gAptValueGCSizeOffset, 1). Guarded
// for a null pool until the Apt runtime startup wires gpGCPoolManager (FLAG).
// ---------------------------------------------------------------------------
void* AptKey::operator new(size_t size)
{
    if (gpGCPoolManager == 0)
        return 0;

    void* lpBlock = gpGCPoolManager->Allocate(size);
    if (lpBlock != 0)
        reinterpret_cast<AptValueGC_MemItem*>(lpBlock)
            ->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpBlock;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF09E0
//
// Free the GC block; on a successful free clear the AptValueGC_MemItem
// "allocated" flag. The X360 does DOGMA_PoolManager::Deallocate(gpGCPoolManager,
// p, size) then, iff it freed, SetIsAllocated(p, gAptValueGCSizeOffset, 0) --
// exactly AptValueGC_PoolManager::DeallocateAptValueGC. Guarded for a null pool.
// ---------------------------------------------------------------------------
void AptKey::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @ 0x82AF0980
//
// AptValueWithHash(AptVFT_Key, 8) -- the X360 calls AptValueWithHash::
// AptValueWithHash(this, 16, 8), zeroes the class-flags word at +0x1C (and clears
// bits 22-23 via `rlwinm 0,10,7`, the optimizer's logical flags-zero on freshly
// pooled memory -- modelled as `mClassFlags = 0`), installs the AptKey vtable,
// then pins the object as a GC root.
// ---------------------------------------------------------------------------
AptKey::AptKey()
    : AptValueWithHash(AptVFT_Key, KI_HASH_CAPACITY)
    , mClassFlags(0)
{
    setGCRoot(1);
}

// ---------------------------------------------------------------------------
// dtor @ 0x82AF0A38 -- empty: the X360 stores the vtable then tail-calls the base
// teardown (~AptValueWithHash, which the linker ICF-folded with ~AptObject). The
// embedded property hash destroys itself.
// ---------------------------------------------------------------------------
AptKey::~AptKey()
{
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @ 0x82AD6188 -- Release the cached native-method
// singletons and clear the slots (Apt shutdown / pool clear). The X360 walks the
// nine method-singleton globals in their .data order (E394, E398, E39C, E3A4,
// E3A8, E3AC, E3B0, E3B4, then E3A0 last) calling Release (vtbl[1]) on each
// non-null one and nulling it.
// ---------------------------------------------------------------------------
void AptKey::CleanNativeFunctions()
{
    if (spMethod_isDown)               { spMethod_isDown->Release();               spMethod_isDown = 0; }
    if (spMethod_isToggled)            { spMethod_isToggled->Release();            spMethod_isToggled = 0; }
    if (spMethod_getCode)             { spMethod_getCode->Release();              spMethod_getCode = 0; }
    if (spMethod_getController)        { spMethod_getController->Release();         spMethod_getController = 0; }
    if (spMethod_addListener)          { spMethod_addListener->Release();          spMethod_addListener = 0; }
    if (spMethod_removeListener)       { spMethod_removeListener->Release();       spMethod_removeListener = 0; }
    if (spMethod_getAnalogStickInfo)   { spMethod_getAnalogStickInfo->Release();   spMethod_getAnalogStickInfo = 0; }
    if (spMethod_getAnalogTriggerInfo) { spMethod_getAnalogTriggerInfo->Release();  spMethod_getAnalogTriggerInfo = 0; }
    if (spMethod_getAscii)             { spMethod_getAscii->Release();             spMethod_getAscii = 0; }
}

// ---------------------------------------------------------------------------
// Lazily build + GC-root one native-method singleton (the per-member tail the
// X360 inlines into objectMemberLookup eight times): if the slot is empty, pool-
// allocate an AptNativeFunction wrapping pFn, GC-root it, then AddRef it (the
// X360's `(**slot)(slot)` == AptValue::AddRef, vtbl[0]). Returns the slot.
// ---------------------------------------------------------------------------
static AptValue* GetOrBuildKeyMethod(AptNativeFunction*& rpSlot, AptExtFunctionPtr pFn)
{
    if (rpSlot == 0)
    {
        void* pMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));
        rpSlot = pMem ? ::new (pMem) AptNativeFunction(pFn) : 0;
        rpSlot->setGCRoot(1);
        rpSlot->AddRef();
    }
    return rpSlot;
}

// ---------------------------------------------------------------------------
// objectMemberLookup @ 0x82AF8780
//
// Resolve a member of `Key`: only when pThis is a defined Key value (isKey()),
// run the gperf member-name index. A hit's Entry::miData drives a switch:
//   miData 1..18 -> box a virtual-key-code CONSTANT in an AptInteger
//                   (Key.ENTER==13, Key.LEFT==37, ...; the X360 word-table maps
//                    each member to its `li r3, <const>`).
//   miData 100   -> the isDown method.
//   miData 101..108 -> isToggled / getCode / getController / addListener /
//                      removeListener / getAnalogStickInfo / getAnalogTriggerInfo /
//                      getAscii (the X360 byte-table order).
// Each method case lazily builds + GC-roots its cached AptNativeFunction. Any
// miss returns null (the base hash handles the rest).
//
// The X360 a2 is the value being queried (checked isKey) and a3 is the name
// pointer; the gperf lookup is in_word_set(name buffer, name length).
// ---------------------------------------------------------------------------
AptValue* AptKey::objectMemberLookup(AptValue* const pThis,
                                     const AptNativeString* const pName) const
{
    const KeyMembersIndex::Entry* pEntry = 0;

    // Only a defined Key value resolves named members.
    if (pThis != 0
        && pThis->getVtblIndex() == AptVFT_Key
        && pThis->getIsDefined())
    {
        pEntry = KeyMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
    }

    if (pEntry == 0)
        return 0;

    switch (pEntry->miData)
    {
    // ---- key-code constants (boxed integers) ----------------------------
    case  1: return AptInteger::Create(0x08);   // BACKSPACE
    case  2: return AptInteger::Create(0x14);   // CAPSLOCK
    case  3: return AptInteger::Create(0x11);   // CONTROL
    case  4: return AptInteger::Create(0x2E);   // DELETEKEY
    case  5: return AptInteger::Create(0x28);   // DOWN
    case  6: return AptInteger::Create(0x23);   // END
    case  7: return AptInteger::Create(0x0D);   // ENTER
    case  8: return AptInteger::Create(0x1B);   // ESCAPE
    case  9: return AptInteger::Create(0x24);   // HOME
    case 10: return AptInteger::Create(0x2D);   // INSERT
    case 11: return AptInteger::Create(0x25);   // LEFT
    case 12: return AptInteger::Create(0x22);   // PGDN
    case 13: return AptInteger::Create(0x21);   // PGUP
    case 14: return AptInteger::Create(0x27);   // RIGHT
    case 15: return AptInteger::Create(0x10);   // SHIFT
    case 16: return AptInteger::Create(0x20);   // SPACE
    case 17: return AptInteger::Create(0x09);   // TAB
    case 18: return AptInteger::Create(0x26);   // UP

    // ---- native methods (lazily built + GC-rooted singletons) -----------
    case 100: return GetOrBuildKeyMethod(spMethod_isDown,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_isDown));
    case 101: return GetOrBuildKeyMethod(spMethod_isToggled,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_isToggled));
    case 102: return GetOrBuildKeyMethod(spMethod_getCode,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_getCode));
    case 103: return GetOrBuildKeyMethod(spMethod_getController,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_getController));
    case 104: return GetOrBuildKeyMethod(spMethod_addListener,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_addListener));
    case 105: return GetOrBuildKeyMethod(spMethod_removeListener,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_removeListener));
    case 106: return GetOrBuildKeyMethod(spMethod_getAnalogStickInfo,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_getAnalogStickInfo));
    // X360 jump-table (byte_82145308 rodata): miData 107 -> getAscii (off_8324E3A0),
    // 108 -> getAnalogTriggerInfo (off_8324E3B4).
    case 107: return GetOrBuildKeyMethod(spMethod_getAscii,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_getAscii));
    case 108: return GetOrBuildKeyMethod(spMethod_getAnalogTriggerInfo,
                  reinterpret_cast<AptExtFunctionPtr>(&AptKey::sMethod_getAnalogTriggerInfo));

    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Translate the last event's raw virtual key code to its reported code (shared
// by isDown / getCode). An ASCII-range printable ([0x20 .. 0x7E]) is upper-cased;
// otherwise a low code (< 0x14) is mapped through kAptKeyCodeTranslate; any other
// code is returned unchanged. (The X360 does exactly this: `(code-0x20) <= 0x5E`
// -> toupper; else `code < 0x14` -> table[code].)
// ---------------------------------------------------------------------------
static int AptKeyTranslateCode(int nCode)
{
    if ((unsigned int)(nCode - 0x20) <= 0x5Eu)
        return toupper(nCode);
    if (nCode < 0x14)
        return kAptKeyCodeTranslate[nCode];
    return nCode;
}

// ---------------------------------------------------------------------------
// sMethod_isDown @ 0x82ADC628 -- is the last event a DOWN of the queried key?
//
// The queried key value is the top of the native arg stack. Only a key DOWN
// event ((event & 3) == 1) can match: the event's translated key code is compared
// to the queried value's integer; equal -> the true singleton, else the false
// singleton. (A non-key event returns false immediately.)
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_isDown()
{
    AptValue* pQueried = gppAptNativeArgStack[gnAptNativeArgCount - 1];

    if ((gAptKeyLastEvent & 3) != 1)
        return AptBoolean::Create(false);

    int nCode = AptKeyTranslateCode((int)(gAptKeyLastEvent >> 17));

    return AptBoolean::Create(nCode == pQueried->toInteger());
}

// ---------------------------------------------------------------------------
// sMethod_isToggled @ 0x82AD6318 -- always returns the false singleton (the X360
// body is a single `return off_8324E414`; toggle state is not tracked).
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_isToggled()
{
    return AptBoolean::Create(false);
}

// ---------------------------------------------------------------------------
// sMethod_getCode @ 0x82AECE98 -- box the last event's translated virtual key
// code in an AptInteger.
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_getCode()
{
    int nCode = AptKeyTranslateCode((int)(gAptKeyLastEvent >> 17));
    return AptInteger::Create(nCode);
}

// ---------------------------------------------------------------------------
// sMethod_getController @ 0x82AECF30 -- box the last event's controller (pad)
// index, biased by -2 (the AS Key reports pad 0 as controller index N-2). The
// X360 reads `(event >> 2) & 0xFF` then subtracts 2.
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_getController()
{
    int nController = (int)((gAptKeyLastEvent >> 2) & 0xFF) - 2;
    return AptInteger::Create(nController);
}

// ---------------------------------------------------------------------------
// sMethod_addListener @ 0x82ADC6E0 -- register a Key listener.
//
// Reads the listener value off the top of the native arg stack (only when called
// with exactly one argument). It is registered only when it is a defined value of
// a CIH-like type (the X360 tests `mbIsDefined` and value-type 12 (CIH) or 37
// (CIHNone), and rejects a CIH whose flags carry the 0x60000000 marker). The
// actual "add if not already present" walk over the input/Key manager's listener
// vector is the un-homed input subsystem (see the AptKeyManagerAddListener FLAG).
// Always returns the AS undefined value.
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_addListener(AptKey* /*pThis*/, int nArgCount)
{
    if (nArgCount == 1)
    {
        AptValue* pListener = gppAptNativeArgStack[gnAptNativeArgCount - 1];

        // The X360 reads the value word: bit 4-from-MSB is mbIsDefined; the low 7
        // bits are meValueType (sign-extended in the asm). A non-defined value is
        // rejected outright.
        if (pListener->getIsDefined())
        {
            const AptVirtualFunctionTable_Indices eType = pListener->getVtblIndex();
            const bool bIsCIH = (eType == AptVFT_CharacterInstHandle)   // 12
                             || (eType == AptVFT_CIHNone);              // 37

            // A CIH-like listener (type 12 or 37) that carries the CIHState marker in
            // mFlagsA is rejected (the X360 `lwz r11, 0xC(r4); rlwinm. 0,1,2` reads
            // the CIHState pair and bails; x64 position bits 1-2, mask 0x6). A non-CIH
            // value skips the marker test entirely (the X360 flag is 0) and proceeds to
            // the membership scan / add. So: add unless it is a marked CIH.
            const bool bRejected =
                bIsCIH && (static_cast<AptCIH*>(pListener)->mFlagsA & 0x6u) != 0;

            if (!bRejected)
                AptKeyManagerAddListener(pListener);   // add iff not already present
        }
    }

    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// sMethod_getAnalogStickInfo @ 0x82AF55F0 -- build an AS Object snapshotting the
// last event's pad analog stick (left == stick id 501, right == 502).
//
// The pad index and stick id come from the last event (`(event>>2)&0xFF` and
// `event>>17`); with no event the defaults are pad 0 / stick id 501. The result
// Object carries:
//   controller  = AptInteger(pad - 2)
//   fXAxisValue = AptFloat(stick[pad].x)
//   fYAxisValue = AptFloat(stick[pad].y)
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_getAnalogStickInfo()
{
    int          nPad     = 0;
    unsigned int nStickId = 501;
    if (gAptKeyLastEvent)
    {
        nStickId = gAptKeyLastEvent >> 17;
        nPad     = (int)((gAptKeyLastEvent >> 2) & 0xFF);
    }

    AptObject* pResult = AptObject::Create(8);

    pResult->Set(gAptKeyControllerKey, AptInteger::Create(nPad - 2));

    // Select the left (501) or right (502) stick table, indexed by pad.
    const float* pStick = 0;
    if (nStickId == 501)
        pStick = &gAptKeyAnalogStickL[4 * nPad];
    else if (nStickId == 502)
        pStick = &gAptKeyAnalogStickR[4 * nPad];

    AptValue* pXValue = AptFloat::Create(pStick[0]);
    AptValue* pYValue = AptFloat::Create(pStick[1]);

    pResult->Set("fXAxisValue", pXValue);
    pResult->Set("fYAxisValue", pYValue);

    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_getAnalogTriggerInfo @ 0x82AF5748 -- build an AS Object snapshotting the
// last event's pad analog triggers.
//
// The pad index comes from the last event (`(event>>2)&0xFF`; default 2 with no
// event). The result Object carries:
//   controller    = AptInteger(pad - 2)
//   fLeftTrigger  = AptFloat(trigger[pad].left)
//   fRightTrigger = AptFloat(trigger[pad].right)
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_getAnalogTriggerInfo()
{
    int nPad = 2;
    if (gAptKeyLastEvent)
        nPad = (int)((gAptKeyLastEvent >> 2) & 0xFF);

    AptObject* pResult = AptObject::Create(8);

    pResult->Set(gAptKeyControllerKey, AptInteger::Create(nPad - 2));

    const float* pTrigger = &gAptKeyAnalogTrigger[4 * nPad];

    pResult->Set("fLeftTrigger",  AptFloat::Create(pTrigger[0]));
    pResult->Set("fRightTrigger", AptFloat::Create(pTrigger[1]));

    return pResult;
}

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt input subsystem, not yet reconstructed): the sibling of
// AptKeyManagerAddListener -- remove pListener from the input/Key manager's
// listener membership vector (off_8324E574 / the gpCurrentTargetSim +0x18 manager,
// vector at +16). The X360/PS3 inline the {count, array} scan-and-remove (PS3
// 0xF303D8: find the entry == pListener, Release it (vtbl[1]), null the slot, and
// decrement the count). The manager + its listener-vector type are not yet
// reconstructed, so the raw-offset walk is expressed through this named helper
// (declared extern, defined by the input TU). Returns true iff it removed one.
// ---------------------------------------------------------------------------
extern bool AptKeyManagerRemoveListener(AptValue* pListener);

// ---------------------------------------------------------------------------
// sMethod_removeListener @ 0x82ADC??? (PS3 EXTERNAL DecFIGS 0xF303D8) -- unregister
// a Key listener. DECOMPILED from the PS3 body, cross-checked vs the X360 listener-
// vector `remove` helper (ARTIST 0x82ADBC28).
//
// Mirrors addListener: only a single-argument call acts. The listener value is the
// top of the native arg stack; it is removed only when it carries the listener
// marker bit (PS3 `*(value+4) & 0x8000000` -- the same packed value-flag word the
// add path tests, here AptValue::mnValueData bit 27). The actual {count, array}
// scan-find-Release-null-and-decrement over the manager's listener vector is the
// un-homed input subsystem (see the AptKeyManagerRemoveListener FLAG above).
// Returns the AS true/false singleton (true iff a listener was removed; every early
// bail returns false).
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_removeListener(AptKey* /*pThis*/, int nArgCount)
{
    if (nArgCount != 1)
        return AptBoolean::Create(false);                 // PS3: a2 != 1

    AptValue* pListener = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // PS3: top of the interp value stack

    // PS3: (*(value + 4) & 0x8000000) == 0 -> not a registered listener value.
    // Bit 27 of the packed value word (mnValueData). Only a value carrying the
    // listener marker is eligible; otherwise nothing to remove.
    if ((pListener->mnValueData & 0x8000000u) == 0)
        return AptBoolean::Create(false);

    // The find-Release-null-decrement walk over the manager's listener vector. The
    // manager/vector types are un-homed (see addListener); a miss returns false.
    const bool bRemoved = AptKeyManagerRemoveListener(pListener);
    return AptBoolean::Create(bRemoved);
}

// ---------------------------------------------------------------------------
// sMethod_getAscii @ 0x82?????? (PS3 EXTERNAL DecFIGS 0xF3E078) -- box the last
// key event's ASCII/virtual key code as an AptInteger.
//
// DECOMPILED from the PS3 body: take the last event's raw key code (event >> 17);
// only when the event actually carries a controller field (event >> 2 != 0) is the
// low code (<= 0x13) folded through the pad->keycode translate table -- a raw code
// with no controller (a pure ASCII event) is reported verbatim. (PS3:
//   v2 = dword_1071B118 >> 17;
//   if ( dword_1071B118 >> 2 ) { if ( v2 <= 0x13 ) v2 = BUTTONACTIONCODE_TO_KEYCODE[v2]; }
//   return AptInteger::Create(v2); )
// BUTTONACTIONCODE_TO_KEYCODE is the same 20-entry low-code translate table the
// X360 getCode/isDown use (kAptKeyCodeTranslate). The header (and the X360 method
// slot) take no arguments; the PS3 a2 is the AptInteger::Create allocation arg the
// caller threads, reproduced by the Create call here.
// ---------------------------------------------------------------------------
AptValue* AptKey::sMethod_getAscii()
{
    int nCode = (int)(gAptKeyLastEvent >> 17);
    if ((gAptKeyLastEvent >> 2) != 0)
    {
        if ((unsigned int)nCode <= 0x13u)
            nCode = kAptKeyCodeTranslate[nCode];
    }
    return AptInteger::Create(nCode);
}
