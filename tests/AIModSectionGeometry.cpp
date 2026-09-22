// FX-AIMOD (crash parity 2026-09-22), G07-D5: AISection::PassesThrough @0x826772A8.
// The runner extracts the PRODUCTION IsInside / PassesThrough bodies and the KF_INTERSECTION_EPSILON
// definition from SharedClasses/AI/AISection.cpp (restored_methods.inc).
//
// The console keeps an edge only when |cross| > FLT_EPSILON (rodata 0x820A36A0 == 0x34000000, splat
// at 0x8267743C, `vcmpgtfp` at 0x82677480). The degenerate quad below has two edges on the X axis
// and two zero-length edges; a ray with |cross| == 2e-8 against both real edges crosses them at
// edge parameter 0.5. The console skips both edges (2e-8 <= 1.19e-7) and answers false; a
// FLT_MIN threshold intersects them and answers true.
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    Check(KF_INTERSECTION_EPSILON == 1.1920928955078125e-07f, "epsilon is the rodata word 0x820A36A0 (0x34000000)");

    // A proper square (the winding IsInside accepts): corner[3] -> corner[0] is the first edge.
    AISection::Vector2 laSquare[KI_AI_SECTION_EDGES] = { { 0.0f, 0.0f }, { 0.0f, 10.0f }, { 10.0f, 10.0f }, { 10.0f, 0.0f } };
    AISection lSquare{};
    lSquare.mpaCorners = laSquare;
    Check(lSquare.IsInside(5.0f, 5.0f), "square centre is inside");
    Check(!lSquare.IsInside(15.0f, 5.0f), "point right of the square is outside");
    Check(lSquare.PassesThrough({ -5.0f, 5.0f }, { 15.0f, 5.0f }), "a segment straight through the square passes through");
    Check(lSquare.PassesThrough({ 5.0f, 5.0f }, { 50.0f, 50.0f }), "a segment starting inside passes through");
    Check(!lSquare.PassesThrough({ -5.0f, 20.0f }, { 15.0f, 20.0f }), "a segment above the square does not");

    // The degenerate quad: edges (0,0)->(10,0) and (10,0)->(0,0) on the X axis, two zero-length edges.
    AISection::Vector2 laFlat[KI_AI_SECTION_EDGES] = { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 10.0f, 0.0f }, { 0.0f, 0.0f } };
    AISection lFlat{};
    lFlat.mpaCorners = laFlat;
    const AISection::Vector2 lStart = { -15.0f, -1.0e-9f };
    const AISection::Vector2 lEnd   = { 25.0f, 1.0e-9f };
    Check(!lFlat.IsInside(lStart.x, lStart.y) && !lFlat.IsInside(lEnd.x, lEnd.y), "both ends of the near-parallel ray are outside");
    Check(!lFlat.PassesThrough(lStart, lEnd), "|cross| = 2e-8 <= FLT_EPSILON: both near-parallel edges are skipped");
    // A steeper ray through the same edges (|cross| = 2e-4) is intersected by both thresholds.
    Check(lFlat.PassesThrough({ -15.0f, -1.0e-5f }, { 25.0f, 1.0e-5f }), "|cross| = 2e-4 > FLT_EPSILON: the edge is intersected");

    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModSectionGeometry: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
