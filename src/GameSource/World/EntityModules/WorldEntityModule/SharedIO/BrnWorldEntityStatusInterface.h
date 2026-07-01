#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityStatusInterface.h
//
// Canonical DWARF home of BrnWorld::WorldEntityIO::StatusInterface
// (DWARF: BrnWorldEntityStatusInterface.h:37) -- the world-entity module's
// five-flag streaming/collision status published each frame. Sibling of the
// committed RequestInterface (BrnWorldEntityRequestInterface.h).
//
// The world Update OUTPUT buffer embeds it BY VALUE
// (BrnWorldIO::UpdateOutputBuffer::mWorldEntityStatusInterface, X360 +169088,
// span 169088..169093 == exactly the five 1-byte bools below). X360 attestation:
//   GetWorldEntityStatusInterface() const @ 0x823B6548 -> &member (+169088)
//   SetWorldEntityStatusInterface         @ 0x827A4BD8 -> the compiler-inlined
//     memberwise copy (a 5-iteration byte-copy loop) == this struct's implicit
//     copy-assignment.
//
// Method declarations follow the DWARF list (:41-:59); bodies belong to this
// type's own TU.

namespace BrnWorld
{
namespace WorldEntityIO
{
    // DWARF: BrnWorldEntityStatusInterface.h:37
    struct StatusInterface
    {
        // ---- methods (DWARF :41-:59; bodies belong to this type's own TU) ----
        void Construct();                            // :41
        void Clear();                                // :44
        bool GetCollisionWorldInvalid() const;       // :46
        void SetCollisionWorldInvalid(bool);         // :47
        bool GetCollisionWorldInvalidating() const;  // :49
        void SetCollisionWorldInvalidating(bool);    // :50
        bool GetCollisionWorldValidating() const;    // :52
        void SetCollisionWorldValidating(bool);      // :53
        bool GetImmediateStreamed() const;           // :55
        void SetImmediateStreamed(bool);             // :56
        bool GetAllStreamed() const;                 // :58
        void SetAllStreamed(bool);                   // :59

    private:
        // ---- FROZEN LAYOUT (DWARF :63-:67; X360 sizeof == 5) ----
        bool mbCollisionWorldInvalid;      // :63
        bool mbCollisionWorldInvalidating; // :64
        bool mbCollisionWorldValidating;   // :65
        bool mbImmediateStreamed;          // :66
        bool mbAllStreamed;                // :67
    };
}
}
