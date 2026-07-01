#pragma once

// Canonical declarations of the AttribSys collection-resolve helpers the generated
// Attrib::Gen::* ctors/accessors bind against.
//
// These are real AttribSys runtime functions (bodies owned by the SDK / their own TUs;
// declaration-only under the per-TU cl /c gate). They live here in ONE place so the many
// generated-class headers that call them share a single declaration: C++ forbids
// re-stating a default argument for the same function in one TU (MSVC C2572), so each
// header must NOT re-declare `FindCollection(int, void* = nullptr)` locally — include
// this header instead.
//
//   FindCollection            @ (SDK)        — resolve the collection for a class/group key.
//   FindCollectionWithDefault @ 0x82808400   — resolve the collection *with its default*.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
    Collection* FindCollection(int liKey, void* lpOwner = nullptr);
    Collection* FindCollectionWithDefault(int liKey);
}
