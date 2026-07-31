#pragma once

// Canonical declarations of the AttribSys collection-resolve helpers the generated
// Attrib::Gen::* ctors/accessors bind against.
//
// These are real AttribSys runtime functions. They live here in ONE place so the many
// generated-class headers that call them share a single declaration.
//
//   FindCollection            @ 0x82808378   — resolve the collection for a (class, collection)
//                                              key pair. BODY: attribsupport.cpp.
//   FindCollectionWithDefault @ 0x82808400   — resolve the collection *with its default*.
//                                              Declaration-only (no recovered body yet; the
//                                              stub lives in GameSource/World/WorldLinkStubs.cpp).
//
// ⚠️ SIGNATURE CORRECTED (2026-07-31, asm-verified @0x82808378). This header previously
// declared `FindCollection(int liKey, void* lpOwner = nullptr)` — a one-key resolve with an
// "owner" second argument. That was wrong in a way that silently mis-resolved EVERY lookup:
//
//   0x82808378  mr  r30, r3   ; a1 = CLASS key      (64-bit; the ctors build it lis/ori+insrdi)
//               mr  r29, r4   ; a2 = COLLECTION key
//               lwz r11, 4(r11)          ; Database::sThis->mPrivates
//               addi r3, r11, 8          ; &mPrivates->mClasses  (the class registry)
//               mr  r4, r30              ; classKey
//               bl  Attrib::Class::TablePolicy_::Find   -> Class*
//               (null -> return 0)
//               lwz r11, 8(r3)           ; Class::mpPrivates
//               addi r3, r11, 0x1C       ; &ClassPrivate::mCollections
//               mr  r4, r29              ; collectionKey
//               bl  Attrib::Class::Ta...::Find          -> Collection*
//
// The DecFIGS DWARF agrees (attribsupport.cpp:35):
//     extern const Attrib::Collection* FindCollection(Attribute::Key, Attribute::Key);
// There is NO owner parameter — the generated ctors thread the owner to Attrib::Instance,
// not to this resolve. Verified in the two ctors that carry a key:
//     Attrib::Gen::shotgroup::shotgroup       @0x82208620  — sets ONLY r3 (the class key);
//     Attrib::Gen::cameradefaults::cameradefaults @0x82208770 — likewise.
// r4 is never written in either, so the CALLER's key argument passes straight through as the
// collection key. With the old `(int, void*)` shape every construction resolved the same
// (wrong) collection, because the group name key was being dropped on the floor.
#include "types.hpp"                                                                // u64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
    // Resolve a collection: look luClassKey up in the process attribute database's class
    // registry, then luCollectionKey in that class's collection table. NULL when either
    // lookup misses. Both keys are 64-bit (the ctors stage a full doubleword in r3; a
    // 32-bit StringToKey result zero-extends into r4).
    Collection* FindCollection(u64 luClassKey, u64 luCollectionKey);
    Collection* FindCollectionWithDefault(int liKey);
}
