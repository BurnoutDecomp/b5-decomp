#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 02.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only).
//
// Functions in this partfile:
//   BoostBurnout5::IsBlueMode               @ 0x822A6C50   (vtable slot 17)
//   BoostBurnout5::GetName                  @ 0x822A6F88   (vtable slot 15)
//   BoostBurnout5::GetIsChainNotifyPending  @ 0x822A70C0   (vtable slot 18)
//
// Offset -> member map used below. The base layout is the wave-P keystone
// header BrnBoostStrategy.h; BoostBurnout5's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host, and every
// B5 member offset here is pinned by BoostBurnout5::Prepare @0x822C1D58:
//   +0x0CB BoostStrategy::mbChainNotifyPending   (base bool block, Prepare stb 0 -> +203)
//   +0x130 BoostBurnout5::meBoostMode            (Prepare stw 0 -> +304, DWARF h:149)
//   +0x144 BoostBurnout5::miChainSize            (Prepare stw 0 -> +324, DWARF h:165)
//
// These three bodies were compile-validated out of tree against a probe header
// carrying the full DWARF member list, which also proved the console offsets
// hold on the host (offsetof meBoostMode == 0x130, miChainSize == 0x144,
// sizeof(BoostBurnout5) == 0x1B0): scratchpad/waveP/probe5/ in the workflow repo.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// IsBlueMode @ 0x822A6C50  (vtable slot 17)
//
//   0x822A6C50  lwz    r11, 0x130(r3)      # meBoostMode
//   0x822A6C54  addi   r11, r11, -1
//   0x822A6C58  cntlzw r11, r11
//   0x822A6C5C  extrwi r3, r11, 1,26       # r3 = (meBoostMode - 1) == 0
//   0x822A6C60  blr
//
// The cntlzw/extrwi pair is the compiler's branchless "== 0" test on the
// decremented value, i.e. a strict equality against 1 -- NOT a `!= 0` test, so
// this is `meBoostMode == E_BOOSTMODE_B2_BLUE` and not `meBoostMode != RED`
// (indistinguishable on a 2-valued enum today, but the asm pins the equality).
// +0x130 is BoostBurnout5's FIRST member (sizeof(BoostStrategy) == 0x130), and
// Prepare @0x822C1D58 stores 0 there (E_BOOSTMODE_B3_RED) as the initial mode.
//
// Feb-2007 (BrnBoostBurnout5.cpp:145) agrees exactly, modulo the enumerator
// spelling: it writes `BoostMode_B2_Blue`; the DecFIGS DWARF (the near-ancestor
// of ARTIST) spells the enumerators E_BOOSTMODE_B3_RED / E_BOOSTMODE_B2_BLUE,
// which is also what the project naming convention requires.
//
// NOTE vs the base class: BoostStrategy::IsBlueMode (slot 17) is `return false`;
// BoostBurnout5 is the strategy that actually has a blue mode.
// ---------------------------------------------------------------------------
bool
BoostBurnout5::IsBlueMode()
{
    return meBoostMode == E_BOOSTMODE_B2_BLUE;
}

// ---------------------------------------------------------------------------
// GetName @ 0x822A6F88  (vtable slot 15; pure in the base)
//
//   0x822A6F88  lis    r11, aBurnout5Rules@ha
//   0x822A6F8C  addi   r3,  r11, aBurnout5Rules@l   # "Burnout 5 rules"
//   0x822A6F90  blr
//
// A single .rdata string address returned in r3; `this` is never touched, which
// is consistent with the DWARF's trailing `const`
// (`virtual const char * GetName() const;`, BrnBoostBurnout5.h). Feb-2007
// (BrnBoostBurnout5.cpp:642) is identical.
// ---------------------------------------------------------------------------
const char*
BoostBurnout5::GetName() const
{
    return "Burnout 5 rules";
}

// ---------------------------------------------------------------------------
// GetIsChainNotifyPending @ 0x822A70C0  (vtable slot 18; overrides the base
// @0x822A5DF0, which just asserts, writes 0 and returns false)
//
//   0x822A70D4  mr     r30, r4                 # lpOutNumChained
//   0x822A70D8  mr     r31, r3                 # this
//   0x822A70DC  cmplwi cr6, r30, 0
//   0x822A70E0  bne    cr6, loc_822A7104
//   0x822A70E4  bl     CgsDev__Assert__BeginAssert
//   0x822A70E8  lis    r11, aDP4B5MainBurno_532@ha
//   0x822A70EC  li     r5,  0x365              # line 869
//   0x822A70F0  addi   r4,  r11, ...@l         # "d:\p4\b5_main\...\BrnBoostBurnout5.cpp"
//   0x822A70F4  lis    r11, aLpoutnumchaine@ha
//   0x822A70F8  addi   r3,  r11, ...@l         # "lpOutNumChained"
//   0x822A70FC  bl     CgsDev__Assert__FireAssert
//   0x822A7100  bl     CgsDev__Assert__EndAssert
//   0x822A7104  lwz    r11, 0x144(r31)         # miChainSize
//   0x822A7108  li     r10, 0
//   0x822A710C  cmpwi  cr6, r11, 0             # SIGNED compare
//   0x822A7110  bgt    cr6, loc_822A711C
//   0x822A7114  stw    r10, 0(r30)             # *lpOutNumChained = 0
//   0x822A7118  b      loc_822A7120
//   0x822A711C  stw    r11, 0(r30)             # *lpOutNumChained = miChainSize
//   0x822A7120  lbz    r3,  0xCB(r31)          # return mbChainNotifyPending
//   0x822A7124  stb    r10, 0xCB(r31)          # ...and clear it (read-and-clear)
//   0x822A713C  blr
//
// Three things the asm pins:
//   * the assert does NOT early-out -- the Begin/Fire/End triple falls straight
//     through into the (unconditional) store through lpOutNumChained. CGS_ASSERT
//     is report-and-continue on this build, so the plain macro matches.
//   * miChainSize is compared SIGNED (`cmpwi`) and only a strictly positive
//     value is published; the out-parameter is uint32_t (DWARF
//     `virtual bool GetIsChainNotifyPending(uint32_t *)`), so the guard is what
//     stops a negative chain size becoming a huge unsigned count.
//   * mbChainNotifyPending is read at +0xCB and cleared in the same breath --
//     the same latch idiom as BoostStrategy::IsATrafficCheckPending @0x822A5E98
//     and HasJustLostBoostChunk @0x822A5EC0.
//
// DIVERGENCE FROM Feb-2007 (asm wins): Feb-2007 declares mbChainNotifyPending as
// a BoostBurnout5 member (BrnBoostBurnout5.h:161) and has no assert at all. In
// retail the flag lives in the BASE at +0xCB (< 0x130, i.e. inside
// BoostStrategy), and the null-pointer assert at BrnBoostBurnout5.cpp:869 was
// added. Everything else -- the signed >0 guard and the read-and-clear -- is
// unchanged from Feb-2007 (BrnBoostBurnout5.cpp:158-174).
// ---------------------------------------------------------------------------
bool
BoostBurnout5::GetIsChainNotifyPending(u32* lpOutNumChained)
{
    // BrnBoostBurnout5.cpp:869 on the console (the assert's line immediate is
    // 0x365); reported, then execution continues into the store below.
    CGS_ASSERT(lpOutNumChained != 0, "lpOutNumChained");

    if (miChainSize > 0)
    {
        *lpOutNumChained = static_cast<u32>(miChainSize);
    }
    else
    {
        *lpOutNumChained = 0;
    }

    const bool lbChainNotifyPending = mbChainNotifyPending;
    mbChainNotifyPending = false;

    return lbChainNotifyPending;
}

} // namespace BrnWorld
