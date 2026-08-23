#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h
//
// Canonical DWARF home of the traffic->director output payloads:
//   BrnTraffic::BrnTrafficIO::TrafficDirectorEntity          (DWARF :46)
//   BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface (DWARF :79)
//
// The world Update OUTPUT buffer embeds the output interface BY VALUE
// (BrnWorldIO::UpdateOutputBuffer::mTrafficDirectorOutputInterface, X360 +143040,
// span 143040..146656 == 3616 == 2(count)+14(pad-to-16)+16-aligned entity array
// (32 * 112-byte TrafficDirectorEntity = 3584 + Array count word, console widths)).
// X360 attestation:
//   GetTrafficDirectorOutputInterface() const @ 0x823B6158 -> &member (+143040)
//   SetTrafficDirectorOutputInterface         @ 0x827A8AB0 -> memberwise copy:
//     `lhz/sth` of mu16EntityCount (+0) then the compiler-emitted
//     Array<TrafficDirectorEntity,32u> copy helper (sub_823B2368) on the +16 array --
//     i.e. the implicit (memberwise) copy-assignment of this struct.
//
// Method declarations follow the DWARF list; bodies belong to this type's own TU.

#include "types.hpp"                                     // u16/u32
#include "BrnCommonTypes.h"                              // Matrix44Affine, Vector3, CgsID
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // DWARF: BrnTrafficDirectorInterfaces.h:46 -- one traffic vehicle's snapshot for
    // the director (local transform + velocity + bounds + identity). Member names /
    // order / types verbatim from the DWARF (:63-:67). The two DWARF constructors
    // (:48 default, :55 full) are own-TU/inline-elsewhere and are not declared here.
    struct alignas(16) TrafficDirectorEntity
    {
        Matrix44Affine mLocalTransform;   // :63
        Vector3        mVelocity;         // :64
        Vector3        mHalfExtents;      // :65
        CgsID          mVehicleId;        // :66
        u16            mu16EntityIndex;   // :67
    };

    // DWARF: BrnTrafficDirectorInterfaces.h:79 -- the per-frame traffic->director
    // output list: an entity count plus a fixed 32-slot entity array.
    struct TrafficDirectorOutputInterface
    {
        // DWARF :81
        static const u32 KU_MAX_ENTITIES = 32;

        // ---- methods (DWARF :87-:98; bodies belong to this type's own TU) ----
        void Construct();                                                       // :87
        void Clear();                                                           // :91
        void AddTrafficEntity(const TrafficDirectorEntity&);                    // :95
        const Array<TrafficDirectorEntity, 32u>& GetTrafficDirectorEntityArray() const; // :98

        // ADDITIVE GROW -- non-const overload of the DWARF
        // :98 accessor, header-inline. No new member, no layout change.
        // WHY IT EXISTS: OutputBuffer_PostPhysics::Construct @0x82761908 INLINES
        // `maActiveEntityArray.Construct()` as the single raw `stw r29,0x2650(r31)` at
        // 0x8276193C -- console +9808 == this interface's +6208 seat, +16 (the array) + 32*112
        // (the entity records) == the Array<>::miCount word at +3600. The array is private, so
        // the de-inlined leg in that Construct needs a named way in; this is it.
        // ⚠️ The console does NOT touch mu16EntityCount (+0) there. Do not add that store.
        Array<TrafficDirectorEntity, 32u>& GetTrafficDirectorEntityArray() { return maActiveEntityArray; }

    private:
        // ---- FROZEN LAYOUT (DWARF :102-:103; X360 count @+0, array @+16) ----
        u16                                mu16EntityCount;      // :102
        Array<TrafficDirectorEntity, 32u>  maActiveEntityArray;  // :103
    };
}
}
