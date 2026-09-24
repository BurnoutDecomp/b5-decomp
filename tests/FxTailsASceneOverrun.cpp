// FX-TAILS-A item 7 (crash parity 2026-09-24): BaseCollisionGenerator::CollideLineAgainstPolySoupList (ARTIST
// 0x82812AE0) keeps the console's unguarded result writes, and a [DIAG] tripwire -- NOT IN THE X360 BINARY -- reports
// the first overrun of its result block once, default on. run_fxtailsa_scene_overrun.py extracts the production body,
// KF_SHORT_LINE_LENGTH_SQ, LeafOverlapsBoxXYZ and NoteLineSoupListOverrun verbatim.
//
// The console: PrepareNewPrimitiveTestResultsList @0x82810798 Mallocs (5 * max) << 4 = 80 * max bytes (0x828108A0..
// 0x828108AC, the 80-byte PrimitiveTestResult stride); both arms call the soup kernel with results + 0x70 * found and
// max - found, unguarded (`subf r5, found, max` 0x82812CE4 / 0x82813178); the kernel writes a 112-byte record, then
// tests its limit. So at max 32 the 23rd record (22 * 112 = 2464 <= 2560 < 2576) ends past the block, and once the
// list is full every hitting leaf still writes one more. The writes and the count are the console's and stay; the
// tripwire only prints `[scene] CollideLineAgainstPolySoupList overran its result block: ...` the first time.
//
// The kernel here is a recording fake with the real one's write-then-test loop; the leaf gather is a stand-in.
// The result block is a real 80 * max region inside a larger arena, so the writes past it are observable.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static unsigned gAsserts = 0;
static std::string gPrinted;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log {
StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gPrinted += (lpcText != nullptr) ? lpcText : "<null>"; return *this; }
static DebugPrint gCapture;
DebugPrint* gpDebugPrint = &gCapture;
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- the kernel fake: the real kernel's write-a-record-then-test-the-limit loop ----------------------------------
static std::map<const CgsGeometric::PolygonSoup*, s32> gaHits;
namespace CgsGeometric {
s32 IntersectLinePolygonSoupSingleSided(const PolygonSoup& lPolygonSoup, const Vector3&, const Vector3&,
                                        PolySoupLineNearestResult* lpResultBuffer, s32 liMaxResults)
{
    const s32 liHits = gaHits.count(&lPolygonSoup) ? gaHits[&lPolygonSoup] : 0;
    s32 liNumResults = 0;
    for (s32 li = 0; li < liHits; ++li)
    {
        std::memset(&lpResultBuffer[liNumResults], 0xAB, sizeof(PolySoupLineNearestResult));
        ++liNumResults;
        if (liNumResults >= liMaxResults) { return liNumResults; }
    }
    return liNumResults;
}

struct PolygonSoupListSpatialMap
{
    std::vector<u16> maOutput;
    std::vector<PolygonSoupLeafNode> maLeaves;
    s32 RunQuery(const AxisAlignedBox&) { return static_cast<s32>(maOutput.size()); }
    s32 RunQuery(const Line&) { return static_cast<s32>(maOutput.size()); }
    const u16* GetOutputQueryBuffer() const { return maOutput.data(); }
    const PolygonSoupLeafNode* GetLeafNodes() const { return maLeaves.data(); }
};
}

// ---- the generator stand-in: one result list whose block is exactly 80 * max bytes at the head of a bigger arena --
static unsigned char gaArena[80 * 64 + 112 * 16];
namespace CgsSceneManager {
namespace CgsCollision {
struct BaseCollisionGenerator
{
    CollisionResultList  mList;
    CollisionResultList* mapCollisionResultLists[1];
    s32 PrepareNewPrimitiveTestResultsList(u16 lu16Max, u32 luTagA, u16 lu16TagB)
    {
        std::memset(gaArena, 0, sizeof(gaArena));
        mList.mpResults = reinterpret_cast<CollisionResult*>(gaArena);
        mList.mu32UserTagA = luTagA; mList.mu16UserTagB = lu16TagB;
        mList.mu16MaxNumResults = lu16Max; mList.mu16NumResults = 0xBEEF; mList.meResultType = 0;
        mapCollisionResultLists[0] = &mList;
        return 0;
    }
    u16 CollideLineAgainstPolySoupList(const CgsGeometric::Line& lrLine, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                       u16 lu16MaxNumResults, u32 lu32UserTagA, u16 lu16UserTagB);
};
}
}

#include "restored_methods.inc"

using CgsSceneManager::CgsCollision::BaseCollisionGenerator;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }
    int Count(const std::string& lrText, const char* lpcNeedle)
    {
        int n = 0;
        for (size_t p = lrText.find(lpcNeedle); p != std::string::npos; p = lrText.find(lpcNeedle, p + 1)) ++n;
        return n;
    }

    CgsGeometric::PolygonSoup gaSoups[4];
    CgsGeometric::PolygonSoupLeafNode Leaf(int liSoup)
    {
        CgsGeometric::PolygonSoupLeafNode n;
        std::memset(&n, 0, sizeof(n));
        n.mBox.mMin = Vector4{ -10.0f, -10.0f, -10.0f, 0.0f };
        n.mBox.mMax = Vector4{ 10.0f, 10.0f, 10.0f, 0.0f };
        n.mpPolygonSoup = &gaSoups[liSoup];
        return n;
    }
    // A 10 m vertical line through three overlapping leaves (the short arm, 0x82812C20..0x82812CFC).
    u16 Run(BaseCollisionGenerator& lrGen, s32 liHits0, s32 liHits1, s32 liHits2, u16 lu16Max)
    {
        CgsGeometric::PolygonSoupListSpatialMap lMap;
        lMap.maLeaves = { Leaf(0), Leaf(1), Leaf(2) };
        lMap.maOutput = { 0, 1, 2 };
        gaHits.clear();
        gaHits[&gaSoups[0]] = liHits0; gaHits[&gaSoups[1]] = liHits1; gaHits[&gaSoups[2]] = liHits2;
        CgsGeometric::Line lLine;
        lLine.mStart.x = 0.0f; lLine.mStart.y = 5.0f;  lLine.mStart.z = 0.0f; lLine.mStart.w = 0.0f;
        lLine.mEnd.x   = 0.0f; lLine.mEnd.y   = -5.0f; lLine.mEnd.z   = 0.0f; lLine.mEnd.w   = 0.0f;
        return lrGen.CollideLineAgainstPolySoupList(lLine, &lMap, lu16Max, 0u, 0);
    }
    bool Written(s32 liRecord) { return gaArena[liRecord * 112 + 111] == 0xAB; }
}

int main()
{
    const char* KAC_LINE = "[scene] CollideLineAgainstPolySoupList overran its result block";
    BaseCollisionGenerator lGen;

    // ---- 22 records at max 32: 2464 <= 2560, inside the block -> silent --------------------------------------------
    gPrinted.clear();
    Run(lGen, 10, 12, 0, 32);
    Check(lGen.mList.mu16NumResults == 22 && Written(21) && !Written(22), "22 records at max 32: count 22, all inside");
    Check(Count(gPrinted, KAC_LINE) == 0, "22 records (2464 bytes) fit the 32 x 80 = 2560-byte block: no line");

    // ---- 30 records at max 32: the 23rd crosses the block's end ---------------------------------------------------
    Run(lGen, 10, 10, 10, 32);
    Check(lGen.mList.mu16NumResults == 30, "30 records at max 32: the count is 30 -- no clamp (console behaviour kept)");
    Check(Written(29) && 30 * 112 > 32 * 80, "... and record 30 was written past the 2560-byte block, as on the console");
    Check(Count(gPrinted, KAC_LINE) == 1, "the tripwire prints `[scene] ... overran` once");
    Check(gPrinted.find("30 records x 112 bytes = 3360 bytes into a block of 32 x 80 = 2560 bytes") != std::string::npos,
          "the line names the records, the bytes written and the block (80 * max)");

    // ---- a full list: max 4, leaves of 4 and 3 hits -> the kernel still writes one record at max - found = 0 --------
    Run(lGen, 4, 3, 0, 4);
    Check(lGen.mList.mu16NumResults == 5 && Written(4), "max 4: 4 + 1 records (the second leaf runs with max 0), as shipped");
    Check(Count(gPrinted, KAC_LINE) == 1, "a second overrun prints nothing more: one-shot");
    Check(gAsserts == 0, "no assertion");

    std::printf("FxTailsASceneOverrun: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
