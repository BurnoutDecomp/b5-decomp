#pragma once

// AttribSys live link: the tool-to-runtime attribute edit channel. A live-link message
// names one attribute ("<class>.<collection>.<attrib>.<index>") followed by an edit
// opcode and its payload; DecodeLiveLinkMessage applies it to the loaded attribute data
// and remembers the original bytes so the edit can be reverted. Bodies live in
// runtime/common/attriblivelink.cpp.

#include "types.hpp"

namespace Attrib
{
    struct Collection;

    // The outcome of decoding one live-link message.
    enum EDecodeResult
    {
        kDecodeSuccessful                    = 0,
        kDecodeMalformedObjectName           = 1,
        kDecodeMalformedFixupList            = 2,
        kDecodeUnableToAllocateEditRecord    = 3,
        kDecodeCannotFindObject              = 4,
        kDecodeObjectAlreadyExists           = 5,
        kDecodeFailedOnInternalInconsistency = 6,
        kDecodeInvalidOperation              = 7,
        kDecodeFailedToRemoveCollection      = 8,
        kDecodeFailedToRemoveAttribute       = 9,
        kDecodeFailedToAddCollection         = 10,
        kDecodeFailedToAddAttribute          = 11,
        kDecodeFailedToSetAttributeLength    = 12,
    };

    // Called after a successful (non-loop) edit with the edited collection and the
    // attribute key. The key is the full 64-bit attribute hash.
    void SetEditNotifier(void (*lpfnNotifier)(const Collection*, u64));

    // Decode and apply one live-link message (the content of the GameTalk "Update" key).
    EDecodeResult DecodeLiveLinkMessage(const char* lpcMessage);
}
