// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::Prepare  @ 0x822C1680   (vtable slot 0)
//
// This is the last function of the wave-P BoostBurnout3 group; it was parked
// only because Attrib::Gen::boostparamsasset had no way to read its attribute
// data area. That is fixed: the generated class now carries the 34 named
// accessors the X360 inlines at this call site, so this body reads the record
// through them and contains no offset arithmetic of its own.
//
// The sibling BoostBurnout3 bodies live in BoostBurnout3_wP_01..07.cpp; member
// layout and vtable slot order come from the wave-P keystone BrnBoostStrategy.h
// and from BrnBoostBurnout3.h. Nothing here re-derives either.
//
// DIVERGENCES FROM THE Feb-2007 SOURCE (rung 1 wins; each one walked in the asm):
//   * Feb-2007's Prepare is `BoostStrategy::Prepare(); miBoostLevel = 0;
//     UpdateMaxBoost(); mfBoostAmount = 0; return true;`. Retail EARLY-OUTS when
//     the base returns false (0x822C16A4 bne / 0x822C16A8 li r3,0), seeds
//     miBoostLevel = 2 rather than 0, additionally clears miOldBoostLevel /
//     mfBoostChunkAmount / mfTimeBoosting, loads the whole 34-parameter
//     boostparamsasset record (Feb-2007 loads no asset at all), and clears
//     mbIsBoostFull.
//   * The UpdateMaxBoost it calls is BoostBurnout3's OWN virtual (bool
//     parameter, vtable slot 50), not Feb-2007's private no-argument helper.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"                 // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                       // rw::core::stdc::ConvertI64ToA
#include "types.hpp"                                                                 // f32 / s64

namespace BrnWorld
{

// The boostparams asset GUID this strategy resolves its tuning collection from.
// NAME, TYPE and VALUE are all DecFIGS-attested verbatim -- the DWARF dump of
// BrnBoostBurnout3.cpp:43 declares
//   const int64_t KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID = 576011;
// and the X360 stages exactly that value inline at 0x822C16DC/0x822C16E8
// (`lis r3,8` + `ori r3,r3,0xCA0B` == 0x0008CA0B == 576011). int64_t is also
// what ConvertI64ToA takes. BoostBurnout2 uses 576005; each strategy has its own
// collection.
const s64 KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID = 576011;

// ----------------------------------------------------------------------------
// Prepare -- vtable slot 0.  X360 @0x822C1680.
//
// Chain the base Prepare (bailing out if it fails -- Feb-2007 ignores the
// result), seed the Burnout-3 chunk state, then fill the base's 34 shared tuning
// parameters from this strategy's boostparamsasset collection.
// ----------------------------------------------------------------------------
bool BoostBurnout3::Prepare()
{
    // 0x822C1698-16AC: `bl BrnWorld__BoostStrategy__Prepare / clrlwi r11,r3,24 /
    // cmplwi cr6,r11,0 / bne`; the fall-through arm is `li r3,0; b <epilogue>`.
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    // 0x822C16B4/16C8: `li r10,2` / `stw r10,0x134(r31)`. The header pins
    // KI_BOOST_LEVELS == 3 (OnTakedown @0x822A6650 `cmpwi cr6,r11,3`), so the
    // original may well have written `KI_BOOST_LEVELS - 1`; the constant is
    // folded in the image and the spelling is not recoverable, so the literal
    // the binary carries is what is written here.
    miBoostLevel = 2;
    miOldBoostLevel = 0;        // 0x822C16CC: stw r30(=0), 0x138(r31)

    // 0x822C16B0/16BC-16D4: `lwz r11,0(r31) / li r4,0 / lwz r11,0xC8(r11) /
    // mtctr / bctrl` -- vtable slot 50 (0xC8/4), BoostBurnout3's OWN virtual
    // UpdateMaxBoost(bool). It runs BEFORE the three zero stores below.
    UpdateMaxBoost(false);

    // 0x822C16EC-16F8: one `lfs f0, flt_82001CC0` (0.0f) stored three times.
    mfBoostAmount = 0.0f;       // stfs f0, 0xA0(r31)
    mfBoostChunkAmount = 0.0f;  // stfs f0, 0x130(r31)
    mfTimeBoosting = 0.0f;      // stfs f0, 0x13C(r31)

    // 0x822C16DC-1714: print the asset GUID as decimal text (`li r5,0xA` is the
    // radix), hash the text into an attribsys key, and construct the
    // boostparamsasset over it.
    //
    // The key is LIVE, not dead: boostparamsasset's ctor @0x822B8C88 never
    // WRITES r4, so the value handed in here arrives whole at FindCollection and
    // is consumed there as the COLLECTION key (derivation in boostparamsasset.h).
    // Attrib::StringToKey returns the full 64-bit hash and the ctor parameter is
    // u64, so nothing is narrowed -- a static_cast<u32> here would discard the
    // high word and make every collection lookup miss.
    //
    // Buffer size: the console frame places this buffer at +0x70 and the first
    // saved register (var_18, `std r30`) at +0x278, i.e. a 0x208-byte gap == 512
    // plus the frame's 8 bytes of alignment slack. 512 is also the DWARF-attested
    // size of the identical ConvertI64ToA scratch buffer in the sibling
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey (DWARF cpp:81).
    char lacGuidText[512];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID, lacGuidText, 10);

    // 0x822C1708-1714: `mr r4,r3` (the whole doubleword, no clrldi), `li r5,0`
    // for the owner, then the ctor.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // 0x822C1718-1858 -- the 34 record -> member stores, in ascending member
    // order (which is the order the X360 emits them). Each one goes through the
    // generated accessor; the record offsets live in boostparamsasset.h.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();
    mfDriftEarning            = lBoostParams.DriftEarning();
    mfAirEarning              = lBoostParams.AirEarning();
    // The only two Int32 attributes in the record, and the only two stores the
    // X360 converts (lwz + extsw + std/lfd + fcfid + frsp, 0x822C1734-176C).
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning());
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning());
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();
    mfTakedownEarning         = lBoostParams.TakedownEarning();
    mfShuntEarning            = lBoostParams.ShuntEarning();
    mfSlamEarning             = lBoostParams.SlamEarning();
    mfNudgeEarning            = lBoostParams.NudgeEarning();
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();
    mfGrindingEarning         = lBoostParams.GrindingEarning();
    mfRubbingEarning          = lBoostParams.RubbingEarning();
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();
    mfTrafficCheck            = lBoostParams.TrafficCheck();
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();
    mfCleanLanding            = lBoostParams.CleanLanding();
    mfFakieLanding            = lBoostParams.FakieLanding();
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();
    mfComboModifier           = lBoostParams.ComboModifier();
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();
    mfBoostChainMin           = lBoostParams.BoostChainMin();
    mfBoostOnComing           = lBoostParams.OnComing();
    mfBeingSlammed            = lBoostParams.BeingSlammed();
    // BoostBurnout3 KEEPS the three stunt earnings the asset ships; BoostBurnout5
    // re-zeroes them immediately after its own load (@0x822C1F8C).
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();
    mfOnWrecked               = lBoostParams.OnWrecked();

    // 0x822C1854: `stb r30(=0), 0xC3(r31)`. The scheduler slotted it between the
    // last two parameter stores; it is the only non-parameter store in the tail.
    mbIsBoostFull = false;

    // 0x822C185C: `bl Attrib::Instance::~Instance` -- lBoostParams leaving scope.
    return true;                // 0x822C1860: li r3,1
}

} // namespace BrnWorld
