#include "SDKs/Realmc/RealmcTrc.h"

#include "SDKs/Realmc/RealmcLocale.h"   // RealmcCore::Locale::GetString(int)

// ===========================================================================
// RealmcCore::Trc -- reconstructed from BURNOUT_X360_ARTIST.XEX (no leak source /
// DWARF). SHAPE and BODIES come from the X360 pseudocode + asm; see RealmcTrc.h
// for the layout map. Every string operation routes through the shared String16
// (RealmcContainers.h) exactly as the X360 folded the eastl::basic_string bodies
// inline.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// Trc::Trc (copy constructor) @ 0x82C465C0
//
//   *this = source scalars (miMessageId, miNumOptions)
//   mMessage.RangeInitialize(src.mMessage.begin, src.mMessage.end)
//   for each option k in 0..3:
//     option[k] default-constructed (miCode 0, mText empty singleton)   [init loop]
//     option[k].miCode = src.option[k].miCode
//     if (&src.option[k].mText != &option[k].mText)                     [inlined op= guard]
//       option[k].mText.assign(src range)
//
// The four options are unrolled in the asm and each carries the basic_string
// operator= self-assignment guard (always true here -- distinct objects), kept
// verbatim. The leading scalar copy (*a1 = *a2) is miMessageId; miNumOptions is
// the a1[5] = a2[5] copy that precedes the option init loop.
// ---------------------------------------------------------------------------
Trc::Trc(const Trc& rOther)
    : miMessageId(rOther.miMessageId)
    , mMessage(rOther.mMessage.Begin(), rOther.mMessage.End())   // RangeInitialize
    , miNumOptions(rOther.miNumOptions)
    // maOptions[] default-constructed: each miCode 0, each mText the empty string --
    // the X360 init loop that zeroes the value words and seats the strings on the
    // shared empty singleton before the per-option copies below.
{
    for (int i = 0; i < 4; ++i)
    {
        maOptions[i].miCode = rOther.maOptions[i].miCode;

        // Inlined basic_string operator= self-assignment guard, then the range-assign.
        if (&maOptions[i].mText != &rOther.maOptions[i].mText)
            maOptions[i].mText.assign(rOther.maOptions[i].mText.Begin(),
                                      rOther.maOptions[i].mText.End());
    }
}

// ---------------------------------------------------------------------------
// Trc::~Trc @ 0x82B555D8
//
//   for each option k in 3..0: free option[k].mText if it owns a buffer
//   free mMessage if it owns a buffer
//
// The X360 loops the four option strings from the last down to the first and then
// frees the main message string -- exactly the reverse-member-order teardown the
// compiler generates for an (otherwise empty) destructor, so the member String16
// destructors (RealmcContainers.cpp) reproduce it: maOptions[3..0] then mMessage.
// ---------------------------------------------------------------------------
Trc::~Trc()
{
}

// ---------------------------------------------------------------------------
// Trc::_SetMsgOptions @ 0x82C466E0
//
//   miNumOptions = 0
//   for (rem = nPackedCodes; rem != 0; rem >>= 8)          [arithmetic shift, signed]
//   {
//     k = miNumOptions ; miNumOptions = k + 1
//     code = rem & 0xFF                                     [clrlwi -- zero-extended byte]
//     maOptions[k].miCode = code
//     text = Locale::GetString(code)
//     maOptions[k].mText.assign(text, text + wcslen16(text))
//   }
//
// nPackedCodes packs up to four option codes, one per byte, low byte first. The
// per-option string is fetched fresh from the localizer and range-assigned over the
// option's current text. The X360 returns the last assign's result in r3; it is a
// setter with no meaningful return, so this is void.
// ---------------------------------------------------------------------------
void Trc::_SetMsgOptions(int nPackedCodes)
{
    miNumOptions = 0;

    for (int nRemaining = nPackedCodes; nRemaining != 0; nRemaining >>= 8)
    {
        const int nIndex = miNumOptions;
        miNumOptions = nIndex + 1;

        TrcMessageOption& rOption = maOptions[nIndex];

        const int nCode = nRemaining & 0xFF;
        rOption.miCode = nCode;

        // The localized option text: a NUL-terminated u16 wide string. The X360 does
        // not null-check the result (it reads the first char16 immediately), so nor do
        // we. u16 and char16_t are the same 2-byte code unit across the vendor boundary.
        u16* pText = Locale::GetString(nCode);
        u16* pTerm = pText;
        while (*pTerm)
            ++pTerm;

        rOption.mText.assign(reinterpret_cast<const char16_t*>(pText),
                             reinterpret_cast<const char16_t*>(pTerm));
    }
}

// ---------------------------------------------------------------------------
// MessageTrc::MessageTrc (value constructor) @ 0x82C467D0
//
//   Message()                    -- base ctor: install base vtable off_821BA2CC,
//                                    atomically zero the lock word at +4 (the X360
//                                    interrupt-masked mfmsr/lwarx/stwcx. idiom)
//   *this = off_821BA37C          -- install the final MessageTrc vtable
//   mTrc(rTrc)                    -- Trc::Trc copy ctor on this+8 (a1 + 2)
//   miValue = nValue              -- a1[28] = a3
//
// The X360 folds the Message base init inline: it stores the base vtable, runs the
// atomic lock-zero, then overwrites the vtable with the final one and copy-builds
// the Trc payload, finally storing the id word at +0x70. IDA reports a spurious
// fourth register argument (a4) the asm never reads; it is not modelled.
// ---------------------------------------------------------------------------
MessageTrc::MessageTrc(const Trc& rTrc, int nValue)
    : Message()
    , mTrc(rTrc)          // Trc copy ctor @ 0x82C465C0 on the embedded payload
    , miValue(nValue)
{
}

// ---------------------------------------------------------------------------
// MessageTrc::~MessageTrc @ 0x82C46428
//
//   *this = off_821BA37C          -- (re)install the MessageTrc vtable at entry
//   mTrc.~Trc()                   -- Trc::~Trc on this+8 (a1 + 2)
//   *this = off_821BA2CC          -- Message base dtor restores the base vtable
//
// The member Trc destructor and the inlined Message base destructor reproduce the
// two vtable toggles the asm shows; the body itself is empty.
// ---------------------------------------------------------------------------
MessageTrc::~MessageTrc()
{
}

// ---------------------------------------------------------------------------
// MessageTrc::Apply @ 0x82C44CF8
//
//   return pTarget->DisplayTrcMessage(pThis);   // pTarget->vtable[+0x48](pTarget, pThis)
//
// A static double-dispatch thunk: the X360 takes the message in r3 and the target
// in r4, swaps them so the target becomes `this`, loads the target's vtable and
// tail-calls its slot +0x48 (18) with (target, message). Mirrors
// RealmcCore::Message::Apply, only the dispatched slot differs (+0x48 vs +0x54).
// ---------------------------------------------------------------------------
int MessageTrc::Apply(MessageTrc* pThis, IRealmcTrcTarget* pTarget)
{
    return pTarget->DisplayTrcMessage(pThis);
}

} // namespace RealmcCore
