// ============================================================================
//  SDKs/EATech/Apt/Apt.cpp -- the EA APT middleware PUBLIC ENTRY POINTS.
//
//  The Apt SDK groups its public C API in Apt/Apt.cpp (the 3.02.02 tree's
//  source/Apt/Apt.cpp); this TU mirrors that home for the entries the X360
//  build keeps out-of-line. Currently homed here:
//
//    AptLoadAnimation @ 0x82B07AC8  -- "load a movie onto a target path", the
//      entry CgsGui::AptAux::LoadFlashAnimation @0x82849080 calls with the
//      "_level%d" target. (The old BrnGuiAptRuntime PC stand-in that parsed the
//      level back out of the path and drove host movie slots is RETIRED; this
//      is the faithful engine body.)
// ============================================================================

#include "SDKs/EATech/include/Apt/Apt.h"

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC (RAII string nodes)
#include "SDKs/EATech/include/Apt/AptTarget.h"            // gpAptTarget (the live target context)
#include "SDKs/EATech/include/Apt/AptLinker.h"            // AptLinker::Load (the per-target file linker)

// The BackgroundColour once-per-load latch (byte_8324D807; defined in
// SDKs/EATech/AptGlobals.cpp; set by the doFrameControls tag-5 arm).
extern unsigned char gbAptBackgroundColourSet;

// ----------------------------------------------------------------------------
// AptLoadAnimation @0x82B07AC8 -- queue a movie load onto a target path.
//
// X360 body, in order:
//   1. build the target-path string           (EAStringC::InitFromBuffer)
//   2. reset the BackgroundColour latch       (byte_8324D807 = 0 -- "Each
//      Animation can only have one background color. This value is reset every
//      time the game (or viewer) loads a new animation." per the SDK source)
//   3. build the movie-name string and STRIP a ".swf" suffix
//      (EAStringC::EndWithRemoveIgnoreCase -- Paradise movie names carry no
//      extension; NOTE the 3.02.02 SDK drifted the opposite way, APPENDING
//      ".swf" -- the X360 asm strips, so strip)
//   4. gpAptTarget->mpLinker->Load(&name, &target) -- the linker resolves the
//      target path ("_level%d") as an AS variable to the level's CIH
//      placeholder and links the (loading) AptFile onto it; AptUpdate's
//      per-frame mpLinker->Update() mounts it when the load completes.
//
// The X360's explicit EAStringC refcount choreography (an extra
// IncreaseInternalRefCount on the target before Load, manual
// DecreaseInternalRefCount on both nodes at the tail) balances Load's internal
// drop of the target node; this reconstruction keeps Load non-consuming (see
// the note at AptLinker::Load's tail), so plain RAII locals reproduce the same
// net refcounts. The console returns its final DecreaseInternalRefCount result
// (meaningless; every caller ignores it) -- returned as 1.
// ----------------------------------------------------------------------------
int AptLoadAnimation(const char* pName, const char* pTargetPath)
{
    EAStringC lTarget(pTargetPath);

    gbAptBackgroundColourSet = 0;

    EAStringC lName(pName);
    lName.EndWithRemoveIgnoreCase(".swf");

    gpAptTarget->mpLinker->Load(&lName, &lTarget);
    return 1;
}
