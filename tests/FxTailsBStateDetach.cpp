// FX-TAILS-B item 7 (crash parity 2026-09-24): CgsSound::Logic::State::Detach @0x826C4C20 -- the base
// detach of every sound-logic state -- against the ARTIST machine code.
//
//   lwz r10, 0x38 ; cmpwi 5 ; beq ok ; li r3, 0 ; blr      not E_UPDATE_ATTACHED -> false, nothing stored
//   ok: stw 0, 0x10 (mpvAttachment) ; stb 0, 0x48 (mbIsAttached)
//       lwz r10, 0x38 ; stw 6, 0x38 ; stw r10, 0x3C          meUpdateState.Set(E_UPDATE_DETATCHING)
//       li r3, 1
// VehicleState::Detach @0x826C9FF8 inlines the same body before its Clear(). CollisionState's vtable
// (off_820B25D0) holds 0x826C4C20 at +0x18 too: its DWARF override is identical-code-folded with the
// base, so it may not touch meLifetime.
//
// The PC detached from ANY update state, kept the attachment and never wrote the history word; its
// CollisionState::Detach wrote E_NONE into meLifetime on success.
//
// run_fxtailsb_state_detach.py extracts the PRODUCTION State::Detach (CgsState.cpp) and
// CollisionState::Detach (BrnCollisionState.cpp) and compiles them against the REAL CgsState.h State
// (its other out-of-line members are stubbed below) and a fixture CollisionState with the real
// DataPoint<ELifetime>.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"      // CgsSound::Utils::DataPoint
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"     // the real State

#include <cstdio>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- the real State's other out-of-line members, stubbed ------------------------------------------
namespace CgsSound {
MemBase::~MemBase() {}
namespace Logic {
State::State()
    : miInstNum(0), meMapState(0), miStateInstType(0), mpvAttachment(0), mpPrevState(0), mpNextState(0),
      mpHeadEffectControl(0), mpHeadEffectObject(0), mpStateManager(0), mpLogicModule(0), miSFXFlags(0),
      miNumLoadedEffectObjects(0), miNumLoadedEffectControls(0), mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS),
      mpCurrentEffect(0), mbIsAttached(false), mfCurTime(0.0f), mfDeltaTime(0.0f)
{
    mauUpdateState[0] = 0u;
    mauUpdateState[1] = 0u;
}
State::~State() {}
bool State::IsAttachedToThis(void* apvAttachment) { return mpvAttachment == apvAttachment; }
ClassTypeInfo<State>* State::GetTypeInfo() const { return nullptr; }
const char* State::GetTypeName() const { return "State"; }
void State::Attach(void*) {}
void State::UpdateParams(f32) {}
void State::ProcessUpdate() {}

#include "fxtailsb_state_detach_body.inc"
} }

// ---- a CollisionState with the real lifetime history ------------------------------------------------
namespace BrnSound { namespace Logic { namespace Collision {
class CollisionState : public CgsSound::Logic::State
{
public:
    enum ELifetime { E_NONE = 0, E_COLLISION = 1, E_SCRAPE = 2 };
    virtual bool Detach() override;
    CgsSound::Utils::DataPoint<ELifetime> meLifetime;
};
#include "fxtailsb_collision_detach_body.inc"
} } }

using CgsSound::Logic::State;
using BrnSound::Logic::Collision::CollisionState;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

static int gaiToken[4];

static void Prime(State& lrState, u32 luUpdateState, u32 luHistory)
{
    lrState.mauUpdateState[0] = luUpdateState;
    lrState.mauUpdateState[1] = luHistory;
    lrState.mpvAttachment = &gaiToken[0];
    lrState.mbIsAttached = true;
}

int main()
{
    // Attached -> detaching, attachment dropped, the old state kept in the history word.
    {
        State lState;
        Prime(lState, State::E_UPDATE_ATTACHED, State::E_INITIALIZE_EFFECTS_UPDATE);
        Check(lState.Detach(), "Detach: an ATTACHED state detaches (true)");
        Check(lState.mauUpdateState[0] == State::E_UPDATE_DETATCHING, "Detach: meUpdateState -> E_UPDATE_DETATCHING");
        Check(lState.mauUpdateState[1] == State::E_UPDATE_ATTACHED,
              "Detach: the DataPoint history word takes the old state (stw r10, 0x3C)");
        Check(lState.mpvAttachment == nullptr, "Detach: mpvAttachment cleared (stw 0, 0x10)");
        Check(!lState.mbIsAttached, "Detach: mbIsAttached cleared (stb 0, 0x48)");
        Check(!lState.IsAttachedToThis(&gaiToken[0]), "Detach: the old attachment no longer matches IsAttachedToThis");
    }
    // Every other update state: false and nothing stored.
    {
        const u32 kau[] = { State::E_UPDATE_UNATTACHED, State::E_INITIALIZE_CONTROLS, State::E_INITIALIZE_CONTROLS_UPDATE,
                            State::E_INITIALIZE_EFFECTS, State::E_INITIALIZE_EFFECTS_UPDATE, State::E_UPDATE_DETATCHING };
        int liRefused = 0;
        int liUntouched = 0;
        for (u32 luState : kau)
        {
            State lState;
            Prime(lState, luState, 3u);
            if (!lState.Detach())
                ++liRefused;
            if (lState.mauUpdateState[0] == luState && lState.mauUpdateState[1] == 3u &&
                lState.mpvAttachment == &gaiToken[0] && lState.mbIsAttached)
                ++liUntouched;
        }
        Check(liRefused == 6, "Detach: UNATTACHED / INITIALIZE_* / DETATCHING -> false (`cmpwi 5 ; beq`)");
        Check(liUntouched == 6, "Detach: a refused detach stores nothing (attachment, flag, both state words kept)");
    }
    // An initialising state cannot be stolen by the managers' "detach the lowest / farthest" searches.
    {
        State lState;
        Prime(lState, State::E_INITIALIZE_CONTROLS_UPDATE, State::E_INITIALIZE_CONTROLS);
        const bool lbStolen = lState.Detach();
        Check(!lbStolen && lState.mbIsAttached, "Detach: a state still initialising is not handed back to a steal");
    }
    // A second detach in the same frame is refused (the state is DETATCHING now).
    {
        State lState;
        Prime(lState, State::E_UPDATE_ATTACHED, State::E_UPDATE_ATTACHED);
        lState.Detach();
        Check(!lState.Detach() && lState.mauUpdateState[1] == State::E_UPDATE_ATTACHED,
              "Detach: detaching twice -> false the second time, history kept");
    }
    // CollisionState: the base, nothing else.
    {
        CollisionState lState;
        Prime(lState, State::E_UPDATE_ATTACHED, State::E_UPDATE_ATTACHED);
        lState.meLifetime.Flush(CollisionState::E_SCRAPE);
        lState.meLifetime.Update(CollisionState::E_COLLISION);
        Check(lState.Detach() && lState.mauUpdateState[0] == State::E_UPDATE_DETATCHING && lState.mpvAttachment == nullptr,
              "CollisionState::Detach: the base detach (identical-code-folded at 0x826C4C20)");
        Check(lState.meLifetime.GetCurrent() == CollisionState::E_COLLISION &&
                  lState.meLifetime.GetPrevious() == CollisionState::E_SCRAPE,
              "CollisionState::Detach: meLifetime untouched (the console writes no E_NONE)");
        CollisionState lInit;
        Prime(lInit, State::E_INITIALIZE_EFFECTS, State::E_INITIALIZE_CONTROLS_UPDATE);
        Check(!lInit.Detach() && lInit.mbIsAttached, "CollisionState::Detach: refused while initialising, like the base");
    }
    std::printf("FxTailsBStateDetach: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
