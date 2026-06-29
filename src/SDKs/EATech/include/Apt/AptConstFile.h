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
    // The .apt signature carries a "<n>:<version>:<pointerSize>" token -- USER-CONFIRMED:
    // "1:7:4" = SWF v7 with 4-byte (32-bit) pointers, "1:7:8" = v7 with 8-byte (64-bit)
    // pointers. AptCharacterAnimationInst::GetSwfVersion (@0x7E6F60) reads the version: a
    // ':' at signature byte 10, then the ASCII version digit at byte 11. The pointer-size
    // field follows after a SECOND ':' (byte 12) as the digit at byte 13.
    //
    // The 32-bit console never reads the pointer size (its .apt is always 4-byte), but
    // 64-bit .apt images exist in other builds of the game, so the x64 loader branches on
    // it to pick the relocate strategy (AptCharacterAnimation::Fixup -- 4 => transcode,
    // 8 => verbatim in-place relocate).
    //
    // The version byte offsets are from GetSwfVersion (authoritative); the pointer-size
    // offsets follow the confirmed "n:v:p" format. FLAG: the signature is assumed to start
    // at the file image (this+0) -- worth a glance at a real .apt header. Both accessors
    // fall back to the console defaults (version 6 / 4-byte) when the markers are absent,
    // so unknown headers behave exactly as today.
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
        // "<n>:<version>:<pointerSize>": version ':' @byte10 (digit @11), pointer-size
        // ':' @byte12 (digit @13). e.g. "1:7:4" => 4, "1:7:8" => 8.
        const int nSize = (sig[12] == ':') ? (sig[13] - '0') : 4;
        return (nSize == 8) ? 8 : 4;   // anything but a clear 8 => the safe 4-byte path
    }
};
