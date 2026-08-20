// b5-decomp/src/GameSource/GameState/Offences/StuntManagerDebugComponent_gUI_00.cpp
//
// Partfile of the BrnGameState::StuntManagerDebugComponent TU (owning header
// BrnStuntManagerDebugComponent.h; the ctor + Construct live in BrnStuntManagerDebugComponent.cpp).
//
// THE FOUR REMAINING VIRTUAL OVERRIDES. They are here for ONE reason and it is a link reason, not
// a gameplay one: StuntManager holds a StuntManagerDebugComponent BY VALUE at its +0x000 and
// StuntManager::Construct calls its Construct + Register, so mounting BrnStuntManager.cpp mounts
// this class -- and MSVC emits the vtable in the TU that defines the constructor, which references
// EVERY virtual override. Without these four bodies the gateui mount is four LNK2019s that
// `cl /c` cannot see (the gate-green != closeable trap).
//
// Everything this class does is DEBUG-MENU ONLY: it draws the stunt manager's last three latched
// positions and offers six "complete all X" cheat buttons. Nothing here is on the smash ->
// billboard -> HUD path the gateui wave proves.
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetName @ X360 0x82359000 -- REAL (the whole console body is this literal).
    //     `const char *GetName() { return "Stunt Manager"; }`
    // It is the label the debug menu lists the component under.
    // ------------------------------------------------------------------------
    const char* StuntManagerDebugComponent::GetName() const
    {
        return "Stunt Manager";
    }

    // ------------------------------------------------------------------------
    // GetPath (DWARF BrnStuntManagerDebugComponent.h:204 -- declared, and the DWARF attests the
    // override exists) -- ⚠️ [gateui] FLAG: THE CONSOLE STRING IS UNRECOVERED.
    // There is NO `StuntManagerDebugComponent::GetPath` in the X360 export set or in
    // progress/identity.json, while its sibling GetName IS there at 0x82359000 -- i.e. the linker
    // ICF-FOLDED this body onto some other function with identical code (a one-instruction
    // "load a rodata pointer and return"), so the fold target carries a different name and the
    // string itself cannot be attributed from the export set. Recovering it needs a headless-IDA
    // pass over the class's vtable slot on a private .i64 copy; it is a debug-MENU PATH (which
    // submenu the component is filed under) with no gameplay effect, so it is parked rather than
    // guessed. Delegating to the base keeps the behaviour defined (DebugComponent::GetPath returns
    // "" -- top level) instead of inventing a path.
    // DELETE-WHEN the vtable slot is dumped.
    // ------------------------------------------------------------------------
    const char* StuntManagerDebugComponent::GetPath() const
    {
        return DebugComponent::GetPath();
    }

    // ------------------------------------------------------------------------
    // OnActivate @ X360 0x82388820 -- ⚠️ [gateui] PARKED, NOT FABRICATED.
    // The console body is six RegisterFunction calls, nothing else:
    //     RegisterFunction(CompleteAllJumpsButOneCallback,      this, "Complete All Jumps but one");
    //     RegisterFunction(CompleteAllSmashesButOneCallback,    this, "Complete All Smashes but one");
    //     RegisterFunction(CompleteAllBillboardsButOneCallback, this, "Complete All Billboards but one");
    //     RegisterFunction(CompleteAllJumps,                    this, "Complete All Jumps");
    //     RegisterFunction(CompleteAllSmashes,                  this, "Complete All Smashes");
    //     RegisterFunction(CompleteAllStunt,                    this, "Complete All Stunts");
    // (`sub_8282F720` == CgsDev::DebugComponent::RegisterFunction(callback, userData, name), which
    //  IS bodied and mounted -- CgsDebugComponent.cpp.)
    // What is NOT bodied is the SIX CALLBACKS themselves: CompleteAllJumps @0x82358DF0,
    // CompleteAllSmashes @0x82358EA0, CompleteAllStunt @0x82358F50,
    // CompleteAllJumpsButOneCallback @0x8237A3A8, CompleteAllSmashesButOneCallback @0x8237A3B0,
    // CompleteAllBillboardsButOneCallback @0x8237A3B8 -- all declaration-only in this class's
    // header, each its own TU, and all of them reach CompleteAllStuntTypeButOne @0x8237A0E8 which
    // is also bodiless. Taking their ADDRESSES here would add six LNK2019s to buy six debug-menu
    // cheat buttons. Lands as one piece the day that group is reconstructed.
    // ------------------------------------------------------------------------
    void StuntManagerDebugComponent::OnActivate()
    {
    }

    // ------------------------------------------------------------------------
    // RenderHUD @ X360 0x82358D00 -- ⚠️ [gateui] PARKED, NOT FABRICATED.
    // The console body is a 3-iteration loop (stride 264 == sizeof(CgsDev::SimpleStrStream)) that
    // draws maStrStreams[i]'s text at x=100, y=100 + i*20, scale 16:
    //     for (i = 0; i < 3; ++i)
    //         lpRender->DrawText(<maStrStreams[i]'s buffer>, 100.0f, 100.0f + i*20.0f, 16.0f, <colour>);
    // Two things stop a faithful body:
    //   * the text argument is the STREAM'S INTERNAL BUFFER, and CgsDev::SimpleStrStream exposes
    //     no committed accessor for it (the console passes `this + 24 + 264*i`, a raw offset into
    //     the stream object -- exactly the raw-offset-hack AGENTS.md forbids);
    //   * the colour argument is not attributable from the pseudocode (the decompiler dropped the
    //     register), and Debug2DImmediateRender::DrawText takes an rw::RGBA by value.
    // AND NOTHING EVER FILLS maStrStreams: the writer is Update/Construct code in the unbodied
    // half of this class, so a faithful body would draw three empty strings anyway.
    // Debug overlay only. DELETE-WHEN SimpleStrStream grows a buffer accessor and the colour is
    // dumped from the .i64.
    // ------------------------------------------------------------------------
    void StuntManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    // ------------------------------------------------------------------------
    // RenderWorld @ X360 0x82358D98 -- ⚠️ [gateui] PARKED, NOT FABRICATED.
    // The console body is a 3-iteration loop over maLastPositions (`r30 = this + 816`, stride 16)
    // calling `CgsDev::Debug3DImmediateRender::DrawSphere(<position in v1>, 10.0f, <colour>)`.
    // Blocked at both ends:
    //   * DrawSphere is DECLARATION-ONLY in this tree (CgsDebug3DImmediateRender.h:31; its TU
    //     CgsDebug3DImmediateRender.cpp IS mounted and bodies only SetDebugFont), so calling it
    //     would add an LNK2019 in the CgsDev debug-render lane -- outside this wave's ownership;
    //   * the committed signature takes a third `const rw::RGBA&` colour that the pseudocode does
    //     not surface, so it cannot be written without inventing one.
    // Debug overlay only (three 10 m wireframe spheres on the last latched trigger positions).
    // DELETE-WHEN Debug3DImmediateRender::DrawSphere is bodied.
    // ------------------------------------------------------------------------
    void StuntManagerDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        (void)lpRender;
    }
}
