// ============================================================================
// GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp
//
// LINK-CLOSURE DEFINITIONS for the render-state factories' two unreconstructable
// virtuals. Same role and same conventions as GameSource/Director/DirectorLinkStubs.cpp
// and GameSource/World/WorldLinkStubs.cpp: every symbol carries WHY it is a stub and a
// DELETE-WHEN note, and a real TU landing for any of them makes the removal enforced by
// the build (a duplicate definition is a link error).
//
// WHY THESE SIX EXIST AT ALL. CgsBlendStateFactory, CgsDepthStencilStateFactory and --
// as of this wave -- CgsRasterizerStateFactory are polymorphic (DecFIGS DWARF: vptr,
// Construct/Destruct/Prepare virtual, in that vtable order). A polymorphic class emits its vtable in whatever TU constructs it, and that
// vtable names EVERY virtual -- so the day either class is embedded by value in something
// the boot exe constructs (BrnRendererModule::mBlendStateFactory /
// ::mDepthStencilStateFactory, reached from `static BrnGame::BrnGameModule gGameModule;`
// at BrnMain.cpp:45), a virtual with no definition anywhere in the link is an LNK2019,
// not a deferred debt. `cl /c` cannot see that; only a real link can. These four
// definitions are what make that mount a link question with a known answer instead of a
// discovery.
//
// WHY THEY ARE STUBS AND NOT RECONSTRUCTIONS -- this is the honest part. Neither
// Destruct nor Prepare exists in the X360 image as far as anything we have can show:
// a scan of all 30,095 exported functions finds only
//     CgsBlendStateFactory::Construct        (0x827EB2D8, itself an export hole)
//     CgsDepthStencilStateFactory::Construct (0x827EBBA0)
//     CgsRasterizerStateFactory::Construct   (0x827EBF30)
// and no Destruct / Prepare / GetState symbol for any of the three, and no callee's
// xrefs_to anywhere names one (which is how Construct's own name was recovered for the
// blend factory). So there is no body to reconstruct and NOTHING IS INVENTED HERE: the
// DWARF says the two slots exist and gives their signatures; it says nothing about what
// the bodies do, and neither does the assembly.
//
// THEY ARE ALSO UNREACHABLE. Nothing in the tree calls Destruct or Prepare on any of the
// three factories, and nothing calls Construct either -- BrnRendererModule::Construct
// does not touch the factories on this build. So these definitions are pure vtable
// filler.
//
// DELETE-WHEN: an X360 body for CgsBlendStateFactory::Destruct / ::Prepare or
// CgsDepthStencilStateFactory::Destruct / ::Prepare is recovered (it would need a fresh
// headless dump the way 0x827EB2D8 did) -> write it in the owning .cpp and delete the
// matching definition here.
//
// CgsRasterizerStateFactory IS NOW COVERED. The note that used to stand here said this
// class was "still declared TU-LOCALLY and NON-POLYMORPHICALLY inside
// CgsRasterizerStateFactory.cpp:6-10 ... When that reconciliation happens it must add
// CgsRasterizerStateFactory::Destruct / ::Prepare here in the same change." This IS that
// change: the class now has a real header at the DWARF shape (vptr, virtual
// Construct/Destruct/Prepare, private static saRasterizerStates[3]), so it has a vtable,
// so it has the same two undefined virtuals its two siblings have -- and they are defined
// below, in the same change, exactly as that note required. The reconciliation was forced
// by BrnPostFx::Render, which pushes saRasterizerStates[2] and could not name it while the
// class was TU-local.
// ============================================================================

#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"

// CgsBlendStateFactory.cpp:235 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsBlendStateFactory::Destruct()
{
}

// CgsBlendStateFactory.cpp:250 (DWARF). No X360 symbol, no X360 body, no caller.
// Returns true because the DWARF types it `bool` and every Prepare-shaped virtual in the
// tree's module protocol answers "ready"; nothing observes it today.
bool CgsBlendStateFactory::Prepare()
{
    return true;
}

// CgsDepthStencilStateFactory.cpp:116 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsDepthStencilStateFactory::Destruct()
{
}

// CgsDepthStencilStateFactory.cpp:131 (DWARF). No X360 symbol, no X360 body, no caller.
bool CgsDepthStencilStateFactory::Prepare()
{
    return true;
}

// CgsRasterizerStateFactory.cpp:90 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsRasterizerStateFactory::Destruct()
{
}

// CgsRasterizerStateFactory.cpp:105 (DWARF). No X360 symbol, no X360 body, no caller.
bool CgsRasterizerStateFactory::Prepare()
{
    return true;
}
