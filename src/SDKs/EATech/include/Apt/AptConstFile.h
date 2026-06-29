#pragma once

// ===========================================================================
// EATech Apt -- AptConstFile: the serialised .apt file image.
//
// A loaded .apt is a single relocatable blob: a header (this AptConstFile)
// followed by the movie's data, all cross-referenced by 32-bit FILE-RELATIVE
// offsets. The loader resolves it by adding the load base to each offset
// (AptCharacterAnimation::Resolve/Fixup) and hands the resolved AptCharacter
// Animation root to the AptFile.
//
// 32-BIT-FORMAT / x64 FORK (see the Apt notes): on the 32-bit console the loader
// relocates these offset slots IN PLACE and uses the blob directly as the runtime
// data. On x64 that is impossible (a 64-bit pointer will not fit a 32-bit file
// slot), so the loader instead COMPUTES the absolute addresses without writing
// them back, and the deep recursive Fixup transcodes the serialised records into
// the native 64-bit runtime structs. This header defines the SERIALISED header
// fields the load-completion path reads; the full serialised record layout (the
// per-character / movie / frame records) + the transcode are the Fixup follow-on.
//
// SHAPE from the PS3 EXTERNAL ELF (AptLoader::CompleteLoad @0x80EF2C reads +20;
// AptCharacterAnimation::Resolve @0x80EEC4 reads +28).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptConstFile
{
    // FLAG: the leading header fields (+0..+16) are not yet decoded; reserved so
    // the decoded offsets land at their file positions for the read path.
    uint32_t maHeaderReserved[5];   // +0..+16

    // +20 -- file-relative offset to the movie's data root (the embedded
    // AptCharacterAnimation sits at root+16). Read by CompleteLoad.
    uint32_t mnDataRootOffset;      // +0x14

    uint32_t maMid[1];              // +24

    // +28 -- a secondary relocated offset (a character/string table base).
    // Read by AptCharacterAnimation::Resolve.
    uint32_t mnSecondaryOffset;     // +0x1C

    // --- .apt FORMAT DISCRIMINATORS (in the leading signature header) ----------
    // AptCharacterAnimationInst::GetSwfVersion (@0x7E6F60) parses the version out of a
    // signature string: a ':' marker followed by an ASCII version digit. The POINTER
    // SIZE is the adjacent format byte: the 32-bit console never reads it (its .apt is
    // always the 4-byte serialised form), but 64-bit (8-byte-pointer) .apt images exist
    // in other builds of the game, and the x64 loader branches on it to pick the
    // relocate strategy (see AptCharacterAnimation::Fixup -- 4 => transcode, 8 =>
    // verbatim in-place relocate).
    //
    // FLAG: the signature byte offsets here are PROVISIONAL -- the version offset is
    // reconstructed from GetSwfVersion's ':'+digit read (signature byte 10 == ':',
    // byte 11 == version), and the pointer-size byte is the inferred-adjacent field
    // (byte 12) per the observed 8-byte .apts. The signature is assumed to start at the
    // file image (this+0). Confirm both against a real .apt (and a real 8-byte one)
    // before trusting the non-default path. Both accessors fall back to the console
    // defaults (version 6 / 4-byte) when the ':' marker is absent, so absent/unknown
    // headers behave exactly as today.
    const char* GetSignature() const { return reinterpret_cast<const char*>(this); }

    int GetVersion() const
    {
        const char* sig = GetSignature();
        return (sig[10] == ':') ? (sig[11] - '0') : 6;
    }

    // 4 = 32-bit serialised format (the console default), 8 = 64-bit.
    int GetPointerSizeBytes() const
    {
        const char* sig = GetSignature();
        const int nSize = (sig[10] == ':') ? (sig[12] - '0') : 4;
        return (nSize == 8) ? 8 : 4;   // anything but a clear 8 => the safe 4-byte path
    }
};
