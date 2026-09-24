// FX-DIRECTOR (crash parity 2026-09-24): MomentPlayerStunt stages its "World_Signature_%i" ICE take by the
// name's CRC32 ZERO-extended to 64 bits. CgsResource::ID::HashString @0x828D84A8 returns through
// `nor r11, r9, r9 ; clrldi r3, r11, 0x20` (0x828D854C), and MomentPlayerStunt::Update @0x82272750 hands
// that register straight to DirectorResourceManager::GetKeyAnim (0x82272B04 `mr r4, r3`). The PC widened
// the s32 return straight to u64 -- a SIGN extension for every CRC with its top bit set:
// "World_Signature_305" is 0x9CA2368A, so the take dictionary missed, the "lpIceTake != NULL" assert fired
// and the guid read faulted at address 8 on the first live run with the moment tick on
// (scratch/bugtest/runs/fxdirector_moment_tick/20260924_180957).
//
// The extracted production MomentPlayerStunt_StageTakeReference is compiled with the production HashString
// (CgsResourceID.cpp) and SPrintf (CgsStringUtils.cpp); the take dictionary, the guid read and the
// AttribSys reference are stand-ins that record what the production body asked for.
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include <cstdio>
#include <cstring>
#include <string>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// ---- the stand-ins -------------------------------------------------------------------------------
static s64 gxTakeField = 0;                  // the game state's staged take (+0xE8)
static u64 guAskedId = 0;                    // the id GetKeyAnim was asked for
static int giKeyAnimCalls = 0;
static const void* gpGuidTake = nullptr;     // the take the guid read was handed
static std::string gGuidKey;                 // the text StringToKey was handed
alignas(16) static unsigned char gaManager[64];
alignas(16) static unsigned char gaTake[64];

// The dictionary holds exactly one take, keyed like the console's bundle entries: the zero-extended CRC.
static u64 KeyOf(const char* lpcName)
{
    return static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcName))));
}
static u64 guStoredKey = 0;

namespace BrnDirector
{
namespace detail
{
    s64 MomentSharedInfo_GetStuntTakeField(const void*) { return gxTakeField; }
    const DirectorResourceManager* MomentSharedInfo_GetDirectorResourceManager(const void*)
    {
        return reinterpret_cast<const DirectorResourceManager*>(gaManager);
    }
    s32 ICETakeData_GetGuid(const ICE::ICETakeData* lpTakeData)
    {
        gpGuidTake = lpTakeData;
        return 581828;
    }
}
ICE::ICETakeData* DirectorResourceManager::GetKeyAnim(CgsResource::ID lKeyAnimID) const
{
    ++giKeyAnimCalls;
    guAskedId = lKeyAnimID.GetHash();
    return (guAskedId == guStoredKey) ? reinterpret_cast<ICE::ICETakeData*>(gaTake) : nullptr;
}
}

namespace Attrib
{
    u64 StringToKey(const char* lpcText) { gGuidKey = lpcText; return 0x1234; }
    RefSpec& RefSpec::operator=(const RefSpec& lrOther)
    {
        mClassKey = lrOther.mClassKey;
        mCollectionKey = lrOther.mCollectionKey;
        return *this;
    }
    void RefSpec::Clean() {}
}

// The production helper (and the detail:: declarations it calls), extracted verbatim.
namespace BrnDirector
{
#include "stunt_take_reference.inc"
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

int main()
{
    // The take the live run asked for: the jump posted cut=305, so the take is "World_Signature_305".
    const u64 luCrc305 = KeyOf("World_Signature_305");
    Check(luCrc305 == 0x9CA2368Aull, "HashString(\"World_Signature_305\") is 0x9CA2368A (lowercased CRC32) -- its top bit is set");

    guStoredKey = luCrc305;
    gxTakeField = 305;
    Attrib::RefSpec lShotRef;
    BrnDirector::MomentPlayerStunt_StageTakeReference(nullptr, lShotRef, 0);
    Check(giKeyAnimCalls == 1 && guAskedId == 0x000000009CA2368Aull,
          "the take id is the CRC ZERO-extended (0x000000009CA2368A), as HashString's clrldi leaves it -- not 0xFFFFFFFF9CA2368A");
    Check(gpGuidTake == gaTake && gAsserts == 0,
          "the take is found: no \"lpIceTake != NULL\" assert, and the guid is read from the take (not from address 8)");
    Check(gGuidKey == "581828" && lShotRef.GetClassKey() == static_cast<u64>(Attrib::Gen::iceanim::ClassKey()) &&
          lShotRef.GetCollectionKey() == 0x1234,
          "the shot reference is {iceanim ClassKey, StringToKey(\"%i\" of the take's guid)}");

    // A name whose CRC has the top bit clear was never affected (the widening is only visible above 2^31).
    const u64 luCrc42 = KeyOf("World_Signature_42");
    guStoredKey = luCrc42;
    gxTakeField = 42;
    giKeyAnimCalls = 0;
    gpGuidTake = nullptr;
    const unsigned luAssertsBefore42 = gAsserts;
    BrnDirector::MomentPlayerStunt_StageTakeReference(nullptr, lShotRef, 0);
    Check(luCrc42 < 0x80000000ull && guAskedId == luCrc42 && gpGuidTake == gaTake && gAsserts == luAssertsBefore42,
          "\"World_Signature_42\" (CRC 0x0515A43C, top bit clear) resolves the same either way");

    // A take the dictionary does not hold still asserts, exactly like the console (0x82272B1C, cpp:248).
    guStoredKey = 0;
    const unsigned luAssertsBeforeMissing = gAsserts;
    BrnDirector::MomentPlayerStunt_StageTakeReference(nullptr, lShotRef, 0);
    Check(gAsserts == luAssertsBeforeMissing + 1 && gLastAssert == "lpIceTake != NULL",
          "a missing take still raises the console's assert");

    std::printf("FxDirectorStuntTake: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
