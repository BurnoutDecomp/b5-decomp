#pragma once

// Attrib::Private — the 8-byte array-length header that precedes a generated
// variable-length array attribute's payload inside an instance's data block.
//
// DWARF-attested (references/DecFIGS/dwarfdump/.../attribsys.h:792 / :2500):
//   struct Attrib::Private { uint8_t mData[8]; unsigned int GetLength() const;
//                            Attrib::Array* ToArray(); const Attrib::Array* ToArray() const; };
//
// The generated array accessors (worldemitterlist::mWorldEmitters,
// languagestreamconfiguration::GetContentSpecsData, speechdata::OnlineVoiceOvers,
// songlist::Songs, sampletags::*, burnoutglobaldata::*) reinterpret an array
// attribute's data pointer as an Attrib::Private* and ask it for the live element
// count before indexing. Only GetLength() is exercised on the X360 ledger; its body
// is a separate AttribSys-runtime TU, so it is declaration-only under the cl /c gate.
//
// Homed here (a single canonical definition) so the many generated-class headers that
// need it share one type rather than each forking a local Attrib::Private — which would
// otherwise clash (struct vs. namespace) when several are pulled into one TU.
#include "types.hpp"

namespace Attrib
{
    struct Private
    {
        u8           mData[8];              // +0 : 8-byte count/capacity header
        unsigned int GetLength() const;     // element count of the array this header fronts
    };
}
