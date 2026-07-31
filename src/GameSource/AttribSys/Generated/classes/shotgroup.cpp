// Attrib::Gen::shotgroup -- generated AttribSys class, out-of-line function bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::shotgroup::Num_ShotList @ 0x821F5948
//
// See shotgroup.h for the class shape. This file exists because Num_ShotList is a REAL
// out-of-line X360 function with ~20 already-committed callers and, until 2026-07-31, no
// body and nowhere to put one. The convention it follows is the one songlist.cpp already
// established in this directory: a per-generated-class .cpp holding exactly the functions
// the X360 ledger attests as out of line, with the inline/accessor surface left in the
// header. (The ctor stays inline in the header -- it is a two-call forwarder and every
// consumer of the class already sees it.)

#include "GameSource/AttribSys/Generated/classes/shotgroup.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h" // Attrib::Attribute cursor

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Tears down the stack-resident Attrib::Attribute cursor Num_ShotList builds. IDA
    // resolves the call to CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct
    // @0x8284CB38 -- an ICF fold of every empty/trivial destructor in the image, so the
    // name is an artifact of identical-code folding and the real callee is ~Attribute.
    // Declared as the same free-function seam the sibling generated classes
    // (surfacelist / speechdata / languagestreamcollection) already use.
    void BaseCollisionGenerator_Destruct(void* lpThis);
}
}

namespace Attrib
{
namespace Gen
{

// ============================================================================
// Attrib::Gen::shotgroup::Num_ShotList @ 0x821F5948
// ============================================================================
//   addi r3,r1,var_20                 ; the 16-byte stack Attrib::Attribute cursor
//   mr   r4,r3(this)                  ; the instance
//   lis 0x1524 / ori 0x6B49 / lis 0x7533 / ori 0xC0E2 / insrdi r5,r11,32,0
//                                     ; r5 = 0x7533C0E2_15246B49, the FULL ShotList key
//   bl Attrib__Instance__Get          ; resolve the attribute into the cursor
//   bl Attrib__Attribute__GetLength   ; r31 = the element count
//   bl <cursor teardown>
//   return r31
//
// The 64-bit key matters: Collection::GetNode hashes the whole doubleword, so the low word
// alone misses. Attrib::Instance::Get's key parameter was `int` (and therefore truncating)
// until 2026-07-31; it is u64 now.
u32 shotgroup::Num_ShotList() const
{
    AttributeValue lCursor; // 16 bytes on the console; the Attrib::Attribute cursor
    Instance* lpSelf = const_cast<shotgroup*>(this);
    Attribute* lpAttribute = reinterpret_cast<Attribute*>(
        lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_SHOTLIST_ATTRIBUTE_KEY));
    const int liLength = lpAttribute->GetLength();
    CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
    return static_cast<u32>(liLength);
}

}
}
