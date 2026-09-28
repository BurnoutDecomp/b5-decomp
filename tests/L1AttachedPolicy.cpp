// L1 (owner's list 2026-09-28, item 2 "the camera is behind walls / below the map", piece 7): the car-attached camera
// collision policy BrnDirector::Camera::CollisionPolicyAttachedToVehicle against the console's own answers
// (tests/L1AttachedPolicyData.h, the ARTIST words run on emu64 by scratch/OWNERLIST_0927/L1/emu/
// gen_attached_policy.py). run_l1_attached_policy.py compiles this against the revision's BrnCollisionPolicy.h and
// includes the revision's BrnCollisionPolicyAttachedToVehicle.cpp as l1_attached_policy.inc.
//
// GROUP C -- Construct @0x82224890, the store set. The console's Construct is run from a policy filled with
//   KU_L1_PATTERN; the data lists every field it stores and the FINAL bytes it left (big-endian). Here the policy is
//   placement-constructed, re-filled with the pattern past the host vptr, and Constructed; then
//     C1/C4  the base's failure latch is clear (the console's `stb 0, 4`; on the host the latch sits after the
//            8-byte vptr, so it is checked by name),
//     C2/C5  every field the console stores holds the console's final value (a 4-byte field compared as a
//            big-endian word on the console and a host word here; a byte as a byte),
//     C3/C6  every other byte of +0x10..+0x24F still holds the pattern (Construct writes nothing the console
//            does not).
//   for Construct(false) and Construct(true). The layout is the console's from +0x10 on (the class's own
//   _AssertLayout pins it), which is what lets the bytes be compared at the console offsets.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"

#include "L1AttachedPolicyData.h"

static int giAsserts = 0;
namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The revision's BrnCollisionPolicyAttachedToVehicle.cpp.
#include "l1_attached_policy.inc"

using BrnDirector::Camera::CollisionPolicyAttachedToVehicle;

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
    else
    {
        std::printf("PASS  %s\n", lpcName);
    }
}

static u32 BigEndianWord(const u8* lpu)
{
    return (static_cast<u32>(lpu[0]) << 24) | (static_cast<u32>(lpu[1]) << 16) | (static_cast<u32>(lpu[2]) << 8)
         | static_cast<u32>(lpu[3]);
}

static u32 HostWord(const u8* lpu)
{
    u32 lu;
    std::memcpy(&lu, lpu, 4);
    return lu;
}

// One Construct(lbArgument) against the console's store list.
static void CheckConstruct(bool lbArgument, const L1ConstructStore* lpStores, u32 luCount, const char* lpcTag)
{
    alignas(16) static u8 saBuffer[sizeof(CollisionPolicyAttachedToVehicle) + 16];
    std::memset(saBuffer, KU_L1_PATTERN, sizeof(saBuffer));
    CollisionPolicyAttachedToVehicle* lpPolicy = new (saBuffer) CollisionPolicyAttachedToVehicle;
    // The host vptr fills +0x00..+0x07; everything after it goes back to the pattern (a member's own default
    // initialisation, if any, must not count as Construct's).
    std::memset(saBuffer + sizeof(void*), KU_L1_PATTERN, sizeof(CollisionPolicyAttachedToVehicle) - sizeof(void*));

    lpPolicy->Construct(lbArgument);

    char lacName[160];
    std::snprintf(lacName, sizeof(lacName), "%s: the base's failure latch is clear (console `stb 0, 4`)", lpcTag);
    Check(!lpPolicy->HasFailed(), lacName);

    bool labStored[0x250] = {};
    int  liBad = 0;
    for (u32 i = 0; i < luCount; ++i)
    {
        const L1ConstructStore& lrStore = lpStores[i];
        if (lrStore.muOffset < 0x10u)
            continue;   // the base sub-object (the console's vptr + latch): checked by name above
        for (u32 b = 0; b < lrStore.muWidth; ++b)
            labStored[lrStore.muOffset + b] = true;
        bool lbOk = true;
        if (lrStore.muWidth == 1u)
        {
            lbOk = saBuffer[lrStore.muOffset] == lrStore.mauBytes[0];
        }
        else
        {
            for (u32 w = 0; w < lrStore.muWidth; w += 4u)
                lbOk = lbOk && HostWord(saBuffer + lrStore.muOffset + w) == BigEndianWord(lrStore.mauBytes + w);
        }
        if (!lbOk)
        {
            ++liBad;
            std::printf("  %s +0x%03X/%u: console", lpcTag, lrStore.muOffset, lrStore.muWidth);
            for (u32 b = 0; b < lrStore.muWidth; ++b)
                std::printf(" %02X", lrStore.mauBytes[b]);
            std::printf("  host(LE)");
            for (u32 b = 0; b < lrStore.muWidth; ++b)
                std::printf(" %02X", saBuffer[lrStore.muOffset + b]);
            std::printf("\n");
        }
    }
    std::snprintf(lacName, sizeof(lacName), "%s: every field the console stores holds the console's value (%d wrong)",
                  lpcTag, liBad);
    Check(liBad == 0, lacName);

    int liTouched = 0;
    for (u32 o = 0x10u; o < 0x250u; ++o)
    {
        if (!labStored[o] && saBuffer[o] != KU_L1_PATTERN)
        {
            if (liTouched < 8)
                std::printf("  %s +0x%03X written (%02X) where the console writes nothing\n", lpcTag, o, saBuffer[o]);
            ++liTouched;
        }
    }
    std::snprintf(lacName, sizeof(lacName), "%s: no byte the console leaves is written (%d written)", lpcTag,
                  liTouched);
    Check(liTouched == 0, lacName);
}

int main()
{
    static_assert(sizeof(CollisionPolicyAttachedToVehicle) == 0x250, "the console stride");

    CheckConstruct(false, kaL1ConstructStores0, sizeof(kaL1ConstructStores0) / sizeof(kaL1ConstructStores0[0]),
                   "C1-C3 Construct(false)");
    CheckConstruct(true, kaL1ConstructStores1, sizeof(kaL1ConstructStores1) / sizeof(kaL1ConstructStores1[0]),
                   "C4-C6 Construct(true)");

    std::printf("asserts fired: %d\n", giAsserts);
    std::printf("L1AttachedPolicy: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
