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
        // ---- methods (DWARF :41-:59) ----
        // ALL TWELVE ARE X360 HEADER-INLINES: none of them has an out-of-line symbol in the
        // ARTIST export set -- every caller's asm stores/loads the flag byte directly. So the
        // bodies live here, matching the console. (They previously sat in WorldLinkStubs.cpp as
        // five assert TRAPS, which would have fired the moment the world streamer published its
        // per-frame status.)
        //
        // Construct's seed is attested by the caller that inlines it,
        // OutputBuffer_PostPhysics::Construct @0x822EDFF0:
        //     stb 0,+0 ; stb 0,+1 ; stb 0,+2 ; stb 1,+3 ; stb 1,+4
        // i.e. the three collision-world flags clear and both "streamed" flags set.
        void Construct()                             // :41
        {
            mbCollisionWorldInvalid      = false;
            mbCollisionWorldInvalidating = false;
            mbCollisionWorldValidating   = false;
            mbImmediateStreamed          = true;
            mbAllStreamed                = true;
        }
        // :44 -- same five-flag seed as Construct (the DWARF pair; the X360 emits no distinct
        // body for either, so both reduce to the one attested store set).
        void Clear() { Construct(); }

        bool GetCollisionWorldInvalid() const       { return mbCollisionWorldInvalid; }       // :46
        void SetCollisionWorldInvalid(bool lbFlag)  { mbCollisionWorldInvalid = lbFlag; }     // :47
        bool GetCollisionWorldInvalidating() const  { return mbCollisionWorldInvalidating; }  // :49
        void SetCollisionWorldInvalidating(bool lbFlag) { mbCollisionWorldInvalidating = lbFlag; } // :50
        bool GetCollisionWorldValidating() const    { return mbCollisionWorldValidating; }    // :52
        void SetCollisionWorldValidating(bool lbFlag) { mbCollisionWorldValidating = lbFlag; } // :53
        bool GetImmediateStreamed() const           { return mbImmediateStreamed; }           // :55
        void SetImmediateStreamed(bool lbFlag)      { mbImmediateStreamed = lbFlag; }         // :56
        bool GetAllStreamed() const                 { return mbAllStreamed; }                 // :58
        void SetAllStreamed(bool lbFlag)            { mbAllStreamed = lbFlag; }               // :59

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
