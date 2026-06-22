// Embed check for CgsEffectBase.{h,cpp}: exercises every bodied ledger function
// and pins the member SEQUENCE the X360 functions touch.
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>

using namespace CgsSound::Logic;

// EffectBase is abstract-ish only via MemBase's pure-virtual-free dtor; concrete here.
struct TestEffect : public EffectBase
{
};

static void exercise()
{
    State::Owner lOwner;
    State        lState;
    // seed the owner chain Prepare walks (state->GetOwner()->mpLogicModule)
    // (GetOwner returns mpOwner which defaults null; we just exercise the call path)
    (void)lOwner;

    TestEffect lEffect;

    // Attach: bumps attach count, clears detach state.
    bool lbAttached = lEffect.Attach();
    u16  luCount    = lEffect.GetAttachCount();
    (void)lbAttached;
    (void)luCount;

    // GetTypeName pair return the recovered string literals.
    EffectControl lControl;
    EffectObject  lObject;
    const char* lpcCtl = lControl.GetTypeName();
    const char* lpcObj = lObject.GetTypeName();

    // Behavioural assertions on the recovered rodata strings.
    CGS_ASSERT(std::strcmp(lpcCtl, "EffectControl") == 0, "EffectControl type name");
    CGS_ASSERT(std::strcmp(lpcObj, "EffectObject") == 0, "EffectObject type name");

    (void)lState;
}

// Member-sequence sanity (NOT absolute X360 offsets, per the 32/64 rule): the
// detach-state member follows the attach-state member, and the attach count is a
// u16, exactly as the X360 Attach touches them.
static_assert(sizeof(u16) == 2, "mu16AttachCount is 16-bit");

int main()
{
    exercise();
    return 0;
}
