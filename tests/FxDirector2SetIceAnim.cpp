// FX-DIRECTOR2 (crash parity 2026-09-25): SimpleIceTakedownPlayer::SetIceAnim @0x821F58C8 tests the WHOLE u64 class key.
//
// The runner (run_fxdirector2_set_ice_anim.py) lifts the revision's PRODUCTION body of SetIceAnim into
// fxd2_seticeanim_body.inc and compiles it as a member of a fixture that carries the player's mpIceAnim. The shots are
// real Attrib::RefSpec records (the 24-byte ShotList element the takedown shot group hands over).
//
// Against the ARTIST body (0x821F58C8):
//   0x821F58E4  cmplwi r31, 0 ; beq <assert>            -- a null shot asserts
//   0x821F58F0  ld r11, 0(r31)                          -- the RefSpec's whole u64 class key
//   0x821F58EC..0x821F5900  r10 = 0x4644E379A997C1EE    -- lis/ori 0xA997C1EE, lis/ori 0x4644E379, insrdi 32,0
//   0x821F5904  cmpld r11, r10 ; beq <store>            -- anything else asserts
//               ("lpIceAnim && lpIceAnim->GetClassKey()==Attrib::Gen::iceanim::ClassKey()", line 0xEC)
//   0x821F592C  stw r31, 0x18(r30)                      -- mpIceAnim = the shot, on every path
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/AttribSys/Generated/classes/iceanim.h"
#include <cstdio>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnDirector
{
// The player's member the body names, and the body itself.
struct SetIceAnimFixture
{
    const Attrib::RefSpec* mpIceAnim;

    void SetIceAnim(const Attrib::RefSpec* lpIceAnim)
#include "fxd2_seticeanim_body.inc"
};
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

// One call: returns the number of asserts it fired; the stored pointer is checked by the caller.
static unsigned Call(BrnDirector::SetIceAnimFixture& lrPlayer, const Attrib::RefSpec* lpShot)
{
    const unsigned luBefore = gAsserts;
    lrPlayer.SetIceAnim(lpShot);
    return gAsserts - luBefore;
}

int main()
{
    const u64 KU_ICEANIM   = 0x4644E379A997C1EEull;   // the console's cmpld constant == iceanim::ClassKey()
    const u64 KU_TAKE_SHUT = 554362u;                // Takedown_ICE_Shut's take (the collection key is not tested)

    BrnDirector::SetIceAnimFixture lPlayer;
    lPlayer.mpIceAnim = nullptr;

    Check(static_cast<u64>(Attrib::Gen::iceanim::ClassKey()) == KU_ICEANIM,
          "K1 Attrib::Gen::iceanim::ClassKey() is the console's constant 0x4644E379A997C1EE (0x821F58EC..0x821F5900)");

    const Attrib::RefSpec lIceShot(KU_ICEANIM, KU_TAKE_SHUT);
    Check(Call(lPlayer, &lIceShot) == 0 && lPlayer.mpIceAnim == &lIceShot,
          "K2 an iceanim shot (the takedown group's Takedown_ICE_Shut) binds with NO assert (`cmpld ; beq`)");

    // The two-dword test ("word +4 == word +0 | 0xC1EE") passes a key whose halves are equal; the console does not.
    const Attrib::RefSpec lEqualHalves(0xA997C1EEA997C1EEull, KU_TAKE_SHUT);
    Check(Call(lPlayer, &lEqualHalves) == 1 && lPlayer.mpIceAnim == &lEqualHalves,
          "K3 a key with equal halves 0xA997C1EEA997C1EE asserts once, and is still stored (`stw r31, 0x18(r30)`)");

    const Attrib::RefSpec lHighOnly(0x0000E379A997C1EEull, KU_TAKE_SHUT);
    Check(Call(lPlayer, &lHighOnly) == 1,
          "K4 a key that differs from iceanim's only in the high dword asserts (the compare is 64-bit)");

    const Attrib::RefSpec lLowOnly(0x4644E379A997C1EFull, KU_TAKE_SHUT);
    Check(Call(lPlayer, &lLowOnly) == 1, "K5 a key that differs only in the low bit asserts");

    const Attrib::RefSpec lUnset(0u, 0u);
    Check(Call(lPlayer, &lUnset) == 1, "K6 an unset reference (class key 0) asserts");

    Check(Call(lPlayer, nullptr) == 1 && lPlayer.mpIceAnim == nullptr,
          "K7 a null shot asserts once (`cmplwi r31, 0 ; beq <assert>`) and null is stored");

    Check(Call(lPlayer, &lIceShot) == 0 && lPlayer.mpIceAnim == &lIceShot,
          "K8 binding the iceanim shot again after the failures is still silent");

    std::printf("FxDirector2SetIceAnim: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
