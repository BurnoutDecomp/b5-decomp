// GameSource/Physics/PropManager/BrnPropDebugComponent.cpp
//
// BrnPhysics::Props::PropDebugComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//     Construct                            @ 0x825BAD58   (29 asm)
//     Destruct                             -- inlined into PropManager::Destruct @0x825E3398
//     GetName                              @ 0x825BADD0   (4 asm)
//     OnRegister                           @ 0x822A9750   (2 asm)
//     RenderHUD                            @ 0x82628EB0   (6 asm)
//     OnActivate                           @ 0x825E3628   (259 asm)
//     RenderStats                          @ 0x826131E8   (239 asm)
//     OnChangeGravityScale                 @ 0x825BAEF0   (14 asm)
//     OnChangeInertiaScale                 @ 0x825BAF28   (14 asm)
//     OnChangeAntiHerdUpwardScale          @ 0x825BAF60   (14 asm)
//     OnChangeAntiHerdSideScale            @ 0x825BAF98   (14 asm)
//     OnChangeAntiHerdHighSpeedSideScale   @ 0x825BAFD0   (14 asm)
//     OnChangeMaxSpeedForSideForce         @ 0x825BB008   (14 asm)
//     OnChangeAntiHerdSpeedClamp           @ 0x825BB040   (14 asm)
//
// =========================================================================================
// ⚠️ CORRECTION -- THE TWELVE TUNING GLOBALS THIS FILE USED DID NOT EXIST
//
// The previous revision of this file drove OnActivate off twelve `extern f32 gf*` globals
// declared in an in-tree-only header (BrnPropTuning.h). Every one of the twelve names, and
// the TYPE of seven of them, is refuted by the image:
//
//   * The seven "anti-herd / scale" values are 16-byte VecFloats, not f32. Each OnChange*
//     callback ends `vspltw v0,v0,0 ; stvx128 v0,r0,r11` -- a 4-lane SPLAT store to a
//     16-byte-aligned global. An f32 global cannot be the target of an stvx128.
//   * Their real names come from the DecFIGS PS3 build of the same seven functions, which
//     stores through NAMED relocations:
//       KVF_GRAVITY_SCALE / KVF_INERTIA_SCALE / KVF_ANTI_HERD_UPWARD_SCALE /
//       KVF_ANTI_HERD_SIDE_SCALE / KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE /
//       KVF_MAX_SPEED_FOR_SIDE_FORCE / KVF_SPEED_CLAMP        (PS3 0x6B5AA4..0x6B5C7C)
//     and the five plain floats are KF_PROP_LINEAR_DRAG / KF_PROP_ANGULAR_DRAG /
//     KF_PROP_MAX_LINEAR_VEL / KF_PROP_MAX_ANGULAR_VEL / KF_PROP_RESTITUTION
//     (PS3 OnActivate 0x6B54F0). The dwarfdump confirms all twelve at namespace scope in
//     BrnPropManager.cpp:45..51 -- which is where they are now DEFINED.
//   * The X360 addresses line up one-for-one with the PS3 names, function by function
//     (0x825BAEF0 -> flt_82FB94E0 == KVF_GRAVITY_SCALE, and so on), so the mapping is not
//     carried across from the PS3 build on faith -- each X360 callback's own member offset
//     (0x14, 0x18, 0x1C, ...) matches its PS3 twin's (0x14, 0x18, 0x1C, ...) exactly.
//
// BrnPropTuning.h is therefore deleted, not edited: it had no correct name in it.
//
// ⚠️ The X360 seeds each member from lane 0 of the VecFloat (`lfs f0, flt_82FB93E0@l(r11)`)
//    while the callback writes ALL FOUR lanes. That asymmetry is the console's, and it is
//    reproduced here rather than tidied into a symmetric accessor pair.
// =========================================================================================

#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"

#include "BrnCommonTypes.h"                                            // ::VecFloat (== Vector4)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"      // GetNumberOfPropTypes, KU_MAX_PROP_TYPES

namespace BrnPhysics
{
namespace Props
{
    // X360 0x825BAD58. The leading `bl BaseCollisionGenerator::Destruct` in the shipped image is
    // the ICF fold of the empty CgsDev::DebugComponent::Construct -- three separate sites in this
    // subsystem call that same address where three DIFFERENT empty `void f(T*)` bodies belong
    // (here, OnRegister @0x822A9750, and PropManager::Destruct's tail @0x825E33E4), which is what
    // identifies it as a fold rather than a real base-class call.
    void PropDebugComponent::Construct(PropManager* lpPropManager)
    {
        CgsDev::DebugComponent::Construct();
        CGS_ASSERT(lpPropManager, "lpPropManager != NULL");            // baked line 52

        mpPropManager = lpPropManager;
        mbRenderStats = true;
        mbRenderWorldContacts = false;
    }

    // No out-of-line X360 symbol: PropManager::Destruct @0x825E3398 carries it inlined --
    // assert *(this+0xC), null it, then the folded-empty base Destruct.
    void PropDebugComponent::Destruct()
    {
        CGS_ASSERT(mpPropManager, "mpPropManager != NULL");            // baked line 69

        mpPropManager = NULL;
        CgsDev::DebugComponent::Destruct();
    }

    // X360 0x825BADD0.
    const char* PropDebugComponent::GetName() const
    {
        return "Prop Manager";
    }

    // X360 0x825E3628. Registration order, group membership, ranges and steps are all read off
    // the asm; every float literal below was read out of the image, not chosen:
    //   0.01f  flt_82002138   0.1f   flt_82004014   0.001f flt_82013F90
    //   0.0f   flt_82001CC0   1.0f   flt_82001C98   10.0f  flt_82004A20   1000.0f flt_82009E10
    // Note the two variables that deliberately get NO SetStep ("Max Speed for side force" and
    // "Speed Clamp") -- the asm has no SetStep call between their RegisterVariable and their
    // SetChangeCallback, and none is added here.
    void PropDebugComponent::OnActivate()
    {
        const char* KPC_ANTI_HERDING_GROUP = "Anti herding...";

        RegisterVariable(&mfAntiHerdUpwardScale, KPC_ANTI_HERDING_GROUP, "Upward Scale");
        SetStep(&mfAntiHerdUpwardScale, 0.01f);
        SetChangeCallback(&mfAntiHerdUpwardScale, OnChangeAntiHerdUpwardScale, this);
        mfAntiHerdUpwardScale = KVF_ANTI_HERD_UPWARD_SCALE.x;

        RegisterVariable(&mfAntiHerdSideScale, KPC_ANTI_HERDING_GROUP, "Side Scale");
        SetStep(&mfAntiHerdSideScale, 0.01f);
        SetChangeCallback(&mfAntiHerdSideScale, OnChangeAntiHerdSideScale, this);
        mfAntiHerdSideScale = KVF_ANTI_HERD_SIDE_SCALE.x;

        RegisterVariable(&mfAntiHerdHighSpeedSideScale, KPC_ANTI_HERDING_GROUP,
                         "High Speed Side Scale");
        SetStep(&mfAntiHerdHighSpeedSideScale, 0.01f);
        SetChangeCallback(&mfAntiHerdHighSpeedSideScale,
                          OnChangeAntiHerdHighSpeedSideScale, this);
        mfAntiHerdHighSpeedSideScale = KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE.x;

        RegisterVariable(&mfMaxSpeedForSideForce, KPC_ANTI_HERDING_GROUP,
                         "Max Speed for side force");
        SetChangeCallback(&mfMaxSpeedForSideForce, OnChangeMaxSpeedForSideForce, this);
        mfMaxSpeedForSideForce = KVF_MAX_SPEED_FOR_SIDE_FORCE.x;

        RegisterVariable(&mfAntiHerdSpeedClamp, KPC_ANTI_HERDING_GROUP, "Speed Clamp");
        SetChangeCallback(&mfAntiHerdSpeedClamp, OnChangeAntiHerdSpeedClamp, this);
        mfAntiHerdSpeedClamp = KVF_SPEED_CLAMP.x;

        RegisterVariable(&mfInertiaScale, "Inertia Scale");
        SetRange(&mfInertiaScale, 0.0f, 10.0f);
        SetStep(&mfInertiaScale, 0.1f);
        SetChangeCallback(&mfInertiaScale, OnChangeInertiaScale, this);
        mfInertiaScale = KVF_INERTIA_SCALE.x;

        RegisterVariable(&mfGravityScale, "Gravity Scale");
        SetRange(&mfGravityScale, 0.0f, 10.0f);
        SetStep(&mfGravityScale, 0.1f);
        SetChangeCallback(&mfGravityScale, OnChangeGravityScale, this);
        mfGravityScale = KVF_GRAVITY_SCALE.x;

        RegisterVariable(&KF_PROP_LINEAR_DRAG, "Linear Drag");
        SetRange(&KF_PROP_LINEAR_DRAG, 0.0f, 1.0f);
        SetStep(&KF_PROP_LINEAR_DRAG, 0.001f);

        RegisterVariable(&KF_PROP_ANGULAR_DRAG, "Angular Drag");
        SetRange(&KF_PROP_ANGULAR_DRAG, 0.0f, 1.0f);
        SetStep(&KF_PROP_ANGULAR_DRAG, 0.001f);

        RegisterVariable(&KF_PROP_MAX_LINEAR_VEL, "Max Linear Velocity");
        SetRange(&KF_PROP_MAX_LINEAR_VEL, 0.0f, 1000.0f);
        SetStep(&KF_PROP_MAX_LINEAR_VEL, 1.0f);

        RegisterVariable(&KF_PROP_MAX_ANGULAR_VEL, "Max Angular Velocity");
        SetRange(&KF_PROP_MAX_ANGULAR_VEL, 0.0f, 1000.0f);
        SetStep(&KF_PROP_MAX_ANGULAR_VEL, 1.0f);

        RegisterVariable(&KF_PROP_RESTITUTION, "Restitution");
        SetRange(&KF_PROP_RESTITUTION, 0.0f, 1.0f);
        SetStep(&KF_PROP_RESTITUTION, 0.01f);

        RegisterVariable(&mbRenderStats, "Render prop manager stats");
        RegisterVariable(&mpPropManager->mbRenderCOM, "Render prop centre of mass");
        RegisterVariable(&mbRenderWorldContacts, "Render prop contacts");
        RegisterVariable(&mpPropManager->mbDisableFreezing, "Disable prop freezing");
    }

    // X360 0x822A9750 -- a single `b` to the folded-empty base.
    void PropDebugComponent::OnRegister()
    {
        CgsDev::DebugComponent::OnRegister();
    }

    // X360 0x82628EB0.
    void PropDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (mbRenderStats)
            RenderStats(lpRender);
    }

    // X360 0x826131E8. Three stat lines down the right of the screen. Everything here is read
    // off the asm:
    //   * the stream is a CgsDev::StrStream over a 100-byte stack buffer (`li r10,0x64 ;
    //     stw r10, miBufferSize`), Reset() before each line (three calls, 0x82613274 /
    //     0x82613348 / 0x82613488);
    //   * the start position is (600.0f, 40.0f)  (flt_820879CC / flt_8208FBD0), the text scale
    //     is 15.0f (flt_820047C4), the colour is `li r25,-1` == 0xFFFFFFFF, and each line steps
    //     the Y down by 10.0f (flt_82004A20). The console keeps the position packed in a vector
    //     register and advances only lane 1 (`vaddfp128` then `vrlimi128 v0,v13,4,0`); the ABI
    //     hands DrawText the two lanes as separate f32s (f1/f2), which is the overload used here.
    //   * the first count is mpPropManager->mpPhysicsData->GetNumberOfPropTypes(): the asm takes
    //     `addi r3,r11,0x54` (== &mpPhysicsData) into ResourcePtr<T>::operator-> -- identified by
    //     its baked assert line 0x220 == 544, which is exactly the non-const operator->'s line in
    //     CgsResourcePtr.h -- and then reads word 0 of the header, i.e. muNumberOfPropTypes.
    //   * the limits 500 / 15 / 30 are `li r4,0x1F4` / `li r4,0xF` / `li r4,0x1E`. 500 is
    //     KU_MAX_PROP_TYPES; 15 and 30 are the capacities of the two BitArrays being counted and
    //     the DWARF gives them no named constant, so they stay literals.
    //   * both instance counts are the textbook SWAR popcount of one 64-bit field, inlined --
    //     BitArray<N>::CountSetBits() (the DWARF's name for it).
    void PropDebugComponent::RenderStats(CgsDev::Debug2DImmediateRender* lpRender)
    {
        const s32 KI_STAT_BUFFER_SIZE = 100;
        const f32 KF_STAT_TEXT_SCALE  = 15.0f;
        const f32 KF_STAT_LINE_STEP   = 10.0f;
        const CgsDev::RGBA KC_STAT_COLOUR = 0xFFFFFFFF;

        char lacBuffer[KI_STAT_BUFFER_SIZE];
        CgsDev::StrStream lStream(lacBuffer, KI_STAT_BUFFER_SIZE);

        f32 lfX = 600.0f;
        f32 lfY = 40.0f;

        lStream.Reset();
        lStream << "Number of physical prop types: "
                << mpPropManager->mpPhysicsData->GetNumberOfPropTypes()
                << " / " << KU_MAX_PROP_TYPES;
        lpRender->DrawText(lStream.GetBuffer(), lfX, lfY, KF_STAT_TEXT_SCALE, KC_STAT_COLOUR);
        lfY += KF_STAT_LINE_STEP;

        lStream.Reset();
        lStream << "Number of physical prop instances: "
                << mpPropManager->mUsedProps.CountSetBits()
                << " / " << 15;
        lpRender->DrawText(lStream.GetBuffer(), lfX, lfY, KF_STAT_TEXT_SCALE, KC_STAT_COLOUR);
        lfY += KF_STAT_LINE_STEP;

        lStream.Reset();
        lStream << "Number of physical part instances: "
                << mpPropManager->mUsedParts.CountSetBits()
                << " / " << 30;
        lpRender->DrawText(lStream.GetBuffer(), lfX, lfY, KF_STAT_TEXT_SCALE, KC_STAT_COLOUR);
    }

    // ------------------------------------------------------------------------------------
    // The seven change callbacks. Each is the same fourteen instructions: load the member the
    // debug UI just edited out of lpUserData (which SetChangeCallback was handed as `this`),
    // splat it into all four lanes, and store the whole vector over the tuning global.
    // lpValue -- the pointer the UI passes as the first argument -- is READ BY NONE OF THEM;
    // that is the console's, and no use is invented for it.
    // ------------------------------------------------------------------------------------
    void PropDebugComponent::OnChangeGravityScale(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfGravityScale;
        KVF_GRAVITY_SCALE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeInertiaScale(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfInertiaScale;
        KVF_INERTIA_SCALE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeAntiHerdUpwardScale(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfAntiHerdUpwardScale;
        KVF_ANTI_HERD_UPWARD_SCALE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeAntiHerdSideScale(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfAntiHerdSideScale;
        KVF_ANTI_HERD_SIDE_SCALE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeAntiHerdHighSpeedSideScale(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfAntiHerdHighSpeedSideScale;
        KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeMaxSpeedForSideForce(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfMaxSpeedForSideForce;
        KVF_MAX_SPEED_FOR_SIDE_FORCE = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }

    void PropDebugComponent::OnChangeAntiHerdSpeedClamp(void* /*lpValue*/, void* lpUserData)
    {
        const PropDebugComponent* lpThis = static_cast<const PropDebugComponent*>(lpUserData);
        const f32 lfValue = lpThis->mfAntiHerdSpeedClamp;
        KVF_SPEED_CLAMP = ::VecFloat{ lfValue, lfValue, lfValue, lfValue };
    }
}
}
