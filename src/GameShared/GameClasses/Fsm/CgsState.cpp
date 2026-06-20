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

    void ScriptedState::Construct(CgsID liId, ScriptedFsm* lpFsm)
    {
        mId = liId;
        mpFsm = lpFsm;
    }
}
