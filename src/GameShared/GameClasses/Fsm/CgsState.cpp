#include "GameShared/GameClasses/Fsm/CgsScriptedState.h"   // CgsFsm::State + CgsFsm::ScriptedState

// CgsFsm::State / ScriptedState base bodies. The X360 keeps these out-of-line in CgsState.cpp; the virtuals
// are non-pure no-op defaults (derived states override the ones they use). Reconstructed minimally to close
// the link for the GUI HUD-flow states (e.g. BrnGui::BootVideos).

namespace CgsFsm
{
    State::State() {}
    void State::OnEnter()    {}
    void State::OnLeave()    {}
    void State::Update()     {}
    void State::PreUpdate()  {}
    void State::PostUpdate() {}
    void State::Render()     {}

    ScriptedState::ScriptedState()
        : mId()
        , mpFsm(0)
    {
    }

    // NOTE: ScriptedState::Construct is bodied (authoritatively, asm-verified @0x82835FF0) in
    // CgsScriptedState.cpp -- not duplicated here. (It once lived here too, but the duplicate was
    // masked only because a stale `class ScriptedFsm` forward decl mangled this TU's copy
    // differently from CgsScriptedState.cpp's `struct` copy. With the tag unified to `struct`, the
    // two collapsed to one symbol -> keep the single definition in CgsScriptedState.cpp.)
}
