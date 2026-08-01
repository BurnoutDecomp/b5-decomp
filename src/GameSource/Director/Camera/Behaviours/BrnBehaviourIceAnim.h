#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                               // rw::math::vpu::Matrix44Affine / Vector3 / Vector4
#include "GameSource/Director/Camera/Camera.h"               // BrnDirector::Camera::Camera (mLastCamera, by value)
#include "GameSource/Director/Utils/BrnVehicleRef.h"         // BrnDirector::VehicleRef (the VehicleRef members)
#include "SDKs/Packages/ICE/ICEData.hpp"                     // ICE::ICETake / ICE::ICETakeData (controller take + take data)
#include "SharedClasses/DataLists/ICEList.h"                 // BrnResource::ICEList::GetICETakeDataFromGuid
#include "GameSource/AttribSys/Generated/classes/iceanim.h"  // Attrib::Gen::iceanim (the real/canonical home)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h" // THE canonical Behaviour base +
                                                             //   BehaviourSharedInfo /
                                                             //   BehaviourSharedPrepareReleaseInfo
                                                             //   (this header's forks retired)
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"   // BrnDirector::Timestep (real home)
// ---- the four canonical homes this header's own forks are retired in favour of -----------
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"     // CollisionPolicy /
                                                              //   VisibilityCollisionPolicy /
                                                              //   CollisionPolicyAttachedToVehicle
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"   // Utils::CameraShake  (struct)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"        // Utils::Looker       (struct)
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h" // Utils::Tweaker      (struct)
// ---- the fifth canonical home this header's own fork is retired in favour of --------------
#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h"
                                                              // BrnDirector::KeyAnimController
                                                              //   (THE home; the private slice
                                                              //    that used to sit below is gone)
#include "GameSource/Director/Shots/BrnShotController.h"       // BrnDirector::ShotContext
// ---- the SIXTH canonical home this header's own fork is retired in favour of ------------
#include "GameSource/Director/BrnDirectorResourceManager.h"
                                                              // BrnDirector::DirectorResourceManager
                                                              //   (THE home -- the minimal slice that
                                                              //    used to sit below is gone; it now
                                                              //    carries all 65 shot-group members)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h
//
// BrnDirector::Camera::BehaviourIceAnim -- the camera behaviour that plays a recorded
// ICE (In-game Camera Editor) "anim" take. It owns a key-anim controller (the live
// ICETake evaluator), the per-frame camera it produces, an attached-to-car collision
// policy, a camera shake and a looker post-process, and the three vehicle references
// the take's eye/look spaces anchor to. The arbitrator states drive it: SetParameters /
// ChangeMovie load a take from an attrib block, Prepare binds the controller to the
// take data, and Update advances the controller and writes the resulting camera.
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES from this subsystem's role; OFFSETS/ORDER are pinned from the
// reconstructed function bodies' member accesses -- the ctor, Construct, Prepare, Update,
// ChangeMovie, SetParameters, GetCollisionPolicy, GetTimeRemaining and HasFinishedOrFailed).
// Members are accessed BY NAME; the struct-relative offsets in comments are provenance
// only, never used as casts.
//
// The base Behaviour and the embedded policy / camera / controller / shake / looker are
// modelled by VALUE and the rebuilt sizes differ from this build, so absolute offsets are
// NOT reproduced with padding -- members are declared in their reconstructed ORDER and
// parity is BY NAME. The struct-relative offsets quoted below are the reconstructed ones:
//
//   +0x0000  Behaviour                       (base)  vtable + shared flag/state block
//   +0x0020  VisibilityCollisionPolicy       mCollisionPolicy            (GetCollisionPolicy -> &this)
//   +0x0260  CollisionPolicyAttachedToVehicle mAttachedToCarCollisionPolicy
//   +0x04B0  Camera                          mLastCamera                 (the produced camera)
//   +0x0610  Matrix44Affine                  mHeadingSpaceTransform      (see the note below)
//   +0x0650  CameraShake                     mShake
//   +0x0660  Looker                          mLooker
//   +0x0680  KeyAnimController               mKeyAnimController           (embeds ICETake @+0x6A0)
//   +0x0DE0  f32                             mfReset0DE0                  (per-take reset block; ChangeMovie/Construct zero it)
//   +0x0DE4  u8[4]                           maReset0DE4                  (per-take reset block tail)
//   +0x0DF0  Behaviour::VehicleRef           mPrimaryVehicleRef
//   +0x0E00  Behaviour::VehicleRef           mSecondaryVehicleRef
//   +0x0E10  Behaviour::VehicleRef           mBystanderRef
//   +0x0E20  s32                             miAnimGuid
//   +0x0E24  Camera::ShotReference*          mpSourceShot
//   +0x0E28  bool                            mbUseCollisionPolicy
//   +0x0E29  bool                            mbUseAttachedToCarCollisionPolicy
//   +0x0E2A  bool                            mbForceHeadingSpaceToBeLooseHeadingSpace
//   +0x0E2B  bool                            mbForceMotionBlurEverything
// ============================================================================

// ============================================================================
// RETIRED (2026-07-31, shot-group wave): the private `namespace ICE { class ICEAuthor
// { ICETakeData* FindEditedTakeFromGuid(s32); }; }` slice that used to sit here is GONE.
//
// WARNING -- IT WAS A CLASS-KEY FORK. The real home
// (SDKs/Packages/ICE/ICEAuthor.hpp:144) spells it `struct ICEAuthor`; this header said
// `class`. MSVC encodes the two differently inside a mangled type (`U` vs `V`), so the
// slice was not a duplicate declaration -- it was a DIFFERENT SYMBOL, and every
// signature mentioning it (DirectorResourceManager::GetICEAuthor, and this behaviour's
// two calls through it) would have failed to link against the real one with an
// LNK2019 that names an identical-looking function. The manager's own header now
// forward-declares it as the `struct` it is.
// ============================================================================

namespace BrnDirector
{

// ============================================================================
// RETIRED (2026-07-31, shot-group wave): the private `class DirectorResourceManager`
// slice that used to sit here -- ~32 declaration-only accessors and NO data members --
// is GONE. The real home is
//     GameSource/Director/BrnDirectorResourceManager.h   (included above)
// and it now declares all 65 shot-group slots plus the DWARF head members, so every
// accessor below is a real header inline over a real member instead of an unresolved
// external.
//
// WHY IT HAD TO GO: it was a SECOND definition of BrnDirector::DirectorResourceManager,
// mangling identically to the real one and disagreeing with it about the layout of
// every member. It survived only because every accessor on it was declaration-only, so
// the disagreement surfaced as LNK2019 rather than as silent corruption -- and because
// no TU ever saw both definitions. Declaring the real member bank turns that from
// latent into live, which is why the two land in the same commit.
//
// THE RENAMES the merge forces (the fork's spellings were invented from each
// consumer's role; the DWARF names are the originals, and the offsets agree):
//     GetCarSelectMotorCityShots()            -> GetCarSelect_MotorCity()             +1064
//     GetCarSelectMotorCityRivalUnlockShots() -> GetCarSelect_MotorCity_RivalUnlock() +1080
//     ... the same four pairs for WestAcres / SouthBay / Heartbreak / LowerPeaks ...
//     GetCarSelectIdleShots()                 -> GetCarSelect_Idle()                  +1224
//     GetCarSelectOutroShots()                -> GetCarSelect_Outro()                 +1240
//     GetCarUnlockShots()                     -> GetCarUnlock()                       +1256
//     GetGameIntroShots()                     -> GetGameIntro()                       +1272
//     GetDriveThruAutoPartsShots()            -> GetDriveThruAutoPartsGroup()         +1032
//     GetDriveThruBodyShopShots()             -> GetDriveThruBodyShopGroup()          +1000
//     GetDriveThruGasStationShots()           -> GetDriveThruGasStationsGroup()       +984
//     GetDriveThruTuningShopShots()           -> GetDriveThruTuningShopGroup()        +1048
//     GetDriveThruTireShopShots()             -> GetDriveThruTyreShopGroup()          +1016
//     GetOnlineCarSelectShots()               -> GetOnlineCarSelect()                 +1480
//     GetOnlineRaceStartShots()               -> GetOnlineRaceStart()                 +616
//     GetTumblingCrashShots()                 -> GetAfterCrash()                      +1448
//     GetStationaryCrashShots()               -> GetAfterCrashSafe()                  +1464
//     GetRankUp() / GetICEAuthor() / GetICEList() / GetKeyAnimFromGuid() / GetKeyAnim()
//     / GetEventIntroShots() / GetEventCompletionShots()  -- names unchanged.
//
// ONE FORK ACCESSOR HAS NO REAL COUNTERPART: `void* GetPostEventMovieData() const`.
// The fork documented it as "the manager's Attrib::Instance at +0x508 (== 1288),
// resolve its ShotList attribute, fall back to DefaultDataArea(0x18)". Byte 1288 is
// mBurnoutLicense, and the DWARF's accessor for it is GetBurnoutLicense(), so the
// console site is `GetBurnoutLicense().GetShotListData(...)` and the "accessor" was a
// whole inlined expression mistaken for one. ArbStatePostEvent is rewritten to that
// spelling; see its call site.
// ============================================================================

// RETIRED (2026-07-29): the minimal `struct Timestep` fork that used to sit here (whose only
// enumerator was E_TYPE_DEFAULT = 0) is gone -- the real home is
// GameSource/Director/Utils/BrnDirectorTimestep.h, included above. Its index 0 is E_WORLD,
// which is the flavour the console reads here (`lfs f0, mafTimestep[0]`).

namespace Camera
{

// ----------------------------------------------------------------------------
// FLAG: minimal slices of the Director camera-behaviour support layer reached by this
//   behaviour. None of these has a reconstructed home yet; each is modelled with only the
//   named members/methods this behaviour's bodies touch, accessed BY NAME. Replace each
//   with its real home when its own Camera TU is reconstructed -- the member/method NAMES
//   are stable. Pointer/handle-only types are forward-declared.
// ----------------------------------------------------------------------------

// RETIRED (2026-07-29): the `class Behaviour` fork that used to sit here is gone; the base
// now comes from GameSource/Director/Camera/Behaviours/Behaviour.h (included above). The
// fork's field names map onto the DWARF base names BY CONSOLE OFFSET, which both models
// agree on:
//     mbHasPreparedCamera (+0x08) -> mbIsPrepared
//     mbFailed            (+0x09) -> mbHasFailed
//     mbBaseFlagA         (+0x0A) -> mbTweakerAttached      <-- see the FLAG below
//     mbMotionBlurGate    (+0x0B) -> mbCanSwitchToMeNow
//     mbBaseFlagC         (+0x0C) -> mbCanSwitchFromMeNow
//     miBaseResetWord     (+0x04) -> meTimestepType
//     mpCurrentTakeData   (+0x10) -> (see mpCurrentTakeData below)
// Two of those renames turn previously-opaque writes into recognisable base operations:
// the fork's Update failure block (`RequestNoFollow(); mbMotionBlurGate = false;
// mbBaseFlagC = true; mbFailed = true;`) is EXACTLY the inlined Behaviour::Fail
// @0x822063E8, and `SetCantSwitchFromMeNow(..., 29)` lands inside the account's attested
// no-cut-from band [27,31) -- independent confirmation of both the base layout and the
// ValidityAccount bounds.
//
// FLAG (+0x0A): the DWARF names that byte `mbTweakerAttached`, yet this behaviour's Update
//   raises it every normal frame, which does not read like "a dev tweaker is attached". One
//   of the two is off: either the reconstructed IceAnim store targets the wrong byte, or the
//   base name is. The RENAME is by offset (which is not in doubt); the SEMANTIC question is
//   recorded here rather than silently resolved.
//   DELETE-WHEN: BehaviourIceAnim::Update's asm is re-walked against the base layout.
// ----------------------------------------------------------------------------

// ============================================================================
// RETIRED (2026-07-30 -- the de-fork wave this header's own FLAGs kept asking for). Six
// private forks used to sit here:
//
//   class CollisionPolicy                    -> ../BrnCollisionPolicy.h  (THE home; every one
//   class VisibilityCollisionPolicy          ->   of the family's tripwires names that file)
//   class CollisionPolicyAttachedToVehicle   ->
//   class Utils::CameraShake                 -> ../Utils/BrnCameraShake.h   (struct, not class)
//   class Utils::Looker                      -> ../Utils/BrnLooker.h        (struct, not class)
//   class Utils::Tweaker                     -> ../Utils/BrnCameraTweaker.h (struct, not class)
//
// All six homes are included at the top of this file. Nothing was lost in the merge -- the
// three members this header's slices uniquely named were CARRIED FORWARD into the canonical
// homes as additive grows:
//   * VisibilityCollisionPolicy's see-through bytes (+0x1A0/+0x1A1/+0x1A2) are now carved out
//     of BrnCollisionPolicy.h's [+0xE8, +0x210) reserved span, reached through the named
//     Set*/ShouldRaiseSeeThrough accessors instead of the fork's public bytes;
//   * CollisionPolicyAttachedToVehicle::Construct(s32) and the `: public CollisionPolicy` base
//     joined BrnCollisionPolicy.h's slice (the base is proved by GetCollisionPolicy returning
//     `&mAttachedToCarCollisionPolicy` as a `CollisionPolicy*`).
// The Tweaker's fork modelled Construct as a STATIC taking the tweaker by reference; the
// canonical home has it as the MEMBER `void Construct()` (X360 @0x821F8588 with this == a2),
// so SetupTweaker calls `lrTweaker.Construct()` now.
//
// ⚠️ THE CLASS KEYS DIFFERED: this header said `class` for all three Utils types where the
// canonical homes say `struct`. MSVC mangles the two differently, so the fork was not merely a
// duplicate definition -- it was a different SYMBOL. Deleting the forks (rather than "fixing"
// the canonical homes to match) is what makes the ICE-anim behaviour link against the real ones.
//
// WHY IT MATTERED: any TU including both the named-parameter bank
// (-> BehaviourPassengerCam.h -> BehaviourRig.h) and this header hit six C2011s, which is the
// whole family of retail-intro arbitrator states (ArbStateCarSelect / ArbStateOnlineCarSelect /
// ArbStateRaceIntro / ArbStateRankUp / ArbStatePostEvent / ArbStateDriveThru /
// ArbStateOnlineRaceIntro). None of them could be mounted.
// ============================================================================

// ============================================================================
// RETIRED (2026-07-31): the private `class BrnDirector::KeyAnimController` slice that used to
// sit here -- an opaque {vptr, pad, ICETake} with `s32 GetEyeSpace()`, an untyped
// `Update(const void*, void*)` and no other state -- is GONE. The real controller is
// reconstructed at
//     GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h  (included above)
// and it is a `ShotController` subclass whose virtual is `Update(const ShotContext&, Camera*)`.
//
// ⚠️ This was a SECOND definition of BrnDirector::KeyAnimController, mangling identically to
// the real one and disagreeing with it about the layout of every member. It survived only
// because the two never met in one TU (the real .cpp did not exist). Nothing was lost in the
// merge: every method the fork named is present on the real class, and the two type
// corrections the merge forces are both improvements --
//   * GetEyeSpace / GetLookSpace return `ICE::eICESpace`, not `s32` (which is why this
//     behaviour's own Update compares them against 0/2/4/6/8/9/10/11/12/13 -- those are
//     eICE_CAR_SPACE .. eICE_LOOSE_HEADING_SPACE);
//   * GetLookPos returns BY VALUE (the console copies the 16-byte lane into the caller's
//     sret slot), not by const reference.
// ============================================================================

} // namespace Camera
} // namespace BrnDirector

// ----------------------------------------------------------------------------
// The attrib-block "iceanim" parameters SetParameters reads (the take guid lives at the
// instance's +0xC). Attrib::Gen::iceanim's real (canonical) home is
// GameSource/AttribSys/Generated/classes/iceanim.h (included above) -- ClassKey()/
// GetClassKey() already live there; GetAnimGuid() is ADDITIVE GROW there (the accessor this
// behaviour's SetParameters/ChangeMovie need). This file previously carried its OWN local
// `struct iceanim` redefinition (a fork); removed here in favour of the real header, which a
// second local definition would collide with once a TU includes both together (e.g.
// BrnArbStateTakedown.cpp, via BrnSimpleIceTakedownPlayer.h).
// ----------------------------------------------------------------------------

namespace BrnDirector { namespace Camera {

// ----------------------------------------------------------------------------
// RETIRED (2026-07-29): the `BehaviourSharedPrepareReleaseInfo` / `BehaviourSharedInfo`
// forks that used to sit here now come from the canonical Behaviour.h (included above).
// Every accessor name this behaviour calls is carried forward there, each resolving to the
// DWARF member the fork's own offset comment pinned (e.g. GetWorld -> mpAllVehicleData
// @+1468, ShouldRePrepareController -> mbIceDataBeingEdited @+1517, GetSourceSpaces ->
// mpCameraSpaceHandler @+1508, GetDebugSink -> mpDebugPrinter @+1488).
// The fork's `RequestFollow` / `RequestNoFollow` did NOT move: their console reaches
// (`+312 |= 0x800` and `+320 &= ~2`) are camera-state writes at Camera +0x138 / +0x140, not
// shared-info writes -- the same pair Behaviour::Fail performs. The two call sites in the
// .cpp now go through the Camera, which is what the console does.
// ============================================================================

// ============================================================================
// BehaviourIceAnim -- the ICE-anim camera behaviour itself.
// ============================================================================
class BehaviourIceAnim : public Behaviour
{
public:
    // The arbitrator-side parameter block SetParameters / ChangeMovie consume: ONE ELEMENT
    // of a shotgroup's ShotList, i.e. a raw 24-byte Attrib::RefSpec.
    //
    // ⭐ CORRECTED 2026-08-01, and the old spelling was a real (latent) DEFECT, not a naming
    // preference. This used to read `typedef Attrib::Gen::iceanim ShotReference;`. Every
    // producer in the tree hands SetParameters a ShotList element -- shotgroup's
    // GetShotListElement / GetShotListData, or Attrib::DefaultDataArea(0x18) as the null
    // fallback -- and that element is an Attrib::RefSpec {mClassKey, mCollectionKey,
    // mpCollectionPtr}, NOT a constructed Attrib::Instance. Under the old typedef,
    // `lpParameters->GetAnimGuid()` read RefSpec+4 (the high half of mCollectionKey) as if
    // it were Instance::mpAttributeData and dereferenced it at +0xC: a garbage take guid at
    // best, a wild read at worst -- a camera that links, boots, and silently plays nothing.
    // The class-key assert did not catch it, because a RefSpec's class key IS its leading
    // qword and so the check passed for the wrong reason.
    //
    // The correct type was already in the tree: Camera::ShotReference (Camera.h:83-84,
    // DWARF Camera.h:43 `typedef const Attrib::RefSpec ShotReference;`), which is what
    // Camera::mpSourceShot @+0x54 uses for the SAME console field role as this behaviour's
    // mpSourceShot @+0xE24. This typedef now names that one rather than forking it, so the
    // two cannot drift apart again. SetParameters builds the temporary iceanim over the
    // RefSpec exactly as the console does (@0x8220F5C0); see its body.
    typedef ::BrnDirector::Camera::Camera::ShotReference ShotReference;

    // Build the behaviour object: set up the vtables and default-construct the embedded
    // ICETake. Body in the class TU.
    BehaviourIceAnim();

    // Reset every owned field to its empty/default state and construct the embedded camera
    // + the attached-to-car collision policy.
    virtual void Construct();

    // Bind the key-anim controller to the take named by miAnimGuid (resolved from the
    // shared-info resource manager). Always returns true.
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);

    // Advance the take this frame and write the produced camera (eye/look spaces, looker,
    // shake, collision gating, motion-blur gate). Always returns true.
    virtual bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo);

    // Read the take guid out of a parameter block and store the block + guid.
    void SetParameters(ShotReference* lpParameters);

    // Re-point the behaviour at a different take: re-take the parameters, reset the
    // per-take state, re-prepare the controller and re-resolve the take data.
    void ChangeMovie(ShotReference* lpParameters, const DirectorResourceManager& lrResourceManager);

    // Hand the dev-tools tweaker the behaviour's editable parameters.
    virtual void SetupTweaker(Utils::Tweaker& lrTweaker);

    // The behaviour's debug name.
    virtual const char* GetName() const;

    // The collision policy in force this frame: the attached-to-car policy when the eye
    // space is car-relative, otherwise the free visibility policy; null when collision is
    // off.
    virtual CollisionPolicy* GetCollisionPolicy();

    // True once the controller has finished playing or the behaviour has failed.
    bool HasFinishedOrFailed() const;

    // RETIRED (2026-07-29): these three re-declarations of HasFailed / CanSwitchToMeNow /
    // CanSwitchFromMeNow existed only because this header carried its own Behaviour fork with
    // its own names for +0x09 / +0x0B / +0x0C. The canonical base declares all three, so the
    // consumers (MomentStationaryCrash::Update @0x82272EA8, MomentPlayerStunt's release gate)
    // now reach the base's versions unchanged.

    // The camera this behaviour produced this frame (mLastCamera). The arbitrator states
    // copy it into their own mCamera while the behaviour is driving (the X360 reaches it
    // via the manager pool slot the BehaviourHandle resolves to). Inline accessor.
    const Camera& GetProducedCamera() const { return mLastCamera; }

    // ---- intro/transition configuration the arbitrator race-intro state pokes ----------
    // ArbStateRaceIntro::Prepare/Update set these behaviour-mode flags directly on the live
    // behaviour after SetParameters (X360 byte stores at the offsets noted). Exposed as named
    // setters so the arbitrator state never pokes the (private) flags by offset.
    void SetUseCollisionPolicy(bool lbUse)            { mbUseCollisionPolicy = lbUse; }                   // +0xE28
    void SetForceLooseHeadingSpace(bool lbForce)      { mbForceHeadingSpaceToBeLooseHeadingSpace = lbForce; } // +0xE2A

    // Force every motion-blur pass on for the take (X360 stb at +0xE2B). The online-race-intro
    // arbitrator state (BrnArbStateOnlineRaceIntro) sets this on its rival/player "show" takes.
    void SetForceMotionBlurEverything(bool lbForce)   { mbForceMotionBlurEverything = lbForce; }          // +0xE2B

    // The two per-take reset bytes the intro state seeds (maReset0DE4[0] / [2]; roles not
    // recovered -- the intro state sets them to gate the take's first-frame behaviour).
    void SetTakeResetByte0(u8 lu8Value)               { maReset0DE4[0] = lu8Value; }   // +0xDE4
    void SetTakeResetByte2(u8 lu8Value)               { maReset0DE4[2] = lu8Value; }   // +0xDE6

    // The trailing per-take reset byte the online-race-intro state seeds with a bool the X360
    // derives via cntlzw (maReset0DE4[3] @+0xDE7; role not recovered -- modelled as the raw
    // attested byte store).
    void SetTakeResetByte3(u8 lu8Value)               { maReset0DE4[3] = lu8Value; }   // +0xDE7

    // ---- online-race-intro arbitrator-state pokes (BrnArbStateOnlineRaceIntro) ----------
    // The online-race-intro "show" takes anchor the behaviour's primary (eye) and secondary
    // (look) vehicle references to one of the intro race cars. The X360 stores, across
    // mPrimaryVehicleRef @+0xDF0 and mSecondaryVehicleRef @+0xE00, the same 4-word ref layout:
    //   ref+0x00 (word)  = 1                 (the ref kind: a race-car reference)
    //   ref+0x04 (word)  = liRaceCarIndex    (asserted < BrnPhysics::Vehicle::ku8MaxNumRaceCars)
    //   ref+0x08 (word)  = 0
    //   ref+0x0C (byte)  = 1                 (the ref is set / valid)
    // and asserts liRaceCarIndex < 8 (ku8MaxNumRaceCars). Exposed as named setters so the
    // arbitrator state never reaches the (private) refs by offset. DECLARATION-ONLY (the
    // VehicleRef configuration body lands with the VehicleRef TU; the per-TU cl /c gate does not
    // link). FLAG: the ref-kind word's full role (==1) is the asm-attested store; the rest of the
    // VehicleRef semantics land with the VehicleRef TU.
    void SetPrimaryVehicleRefToRaceCarIndex(s32 liRaceCarIndex);     // +0xDF0 block
    void SetSecondaryVehicleRefToRaceCarIndex(s32 liRaceCarIndex);   // +0xE00 block

    // The online-race-intro PLAYER "show" take anchors its refs to the player rather than a
    // numbered race car. The X360 writes the same 4-word ref layout with a different kind/index:
    //   ref+0x00 (word)  = 0     (the ref kind: the player ref, not a numbered race car)
    //   ref+0x04 (word)  = -1    (no race-car index)
    //   ref+0x08 (word)  = 0
    //   ref+0x0C (byte)  = 1     (the ref is set / valid)
    // into mPrimaryVehicleRef @+0xDF0 / mSecondaryVehicleRef @+0xE00. Named setters; DECLARATION-
    // ONLY (the VehicleRef body lands with the VehicleRef TU). FLAG: the ref-kind word's full
    // role (==0 for the player ref) is the asm-attested store.
    void SetPrimaryVehicleRefToPlayer();     // +0xDF0 block = {0, -1, 0, 1}
    void SetSecondaryVehicleRefToPlayer();   // +0xE00 block = {0, -1, 0, 1}

    // Seek the embedded key-anim controller back to its take start (mKeyAnimController @+0x680;
    // the online-race-intro ACTIVE state rewinds the player "show" take with 0.0). Named alias of
    // SetControllerParametricTime0To1(0.0f).
    void RewindControllerToStart()                    { SetControllerParametricTime0To1(0.0f); }

    // ⭐ SetCollisionPolicyCanFail -- BODIED 2026-08-01. Every ICE-anim arbitrator state emits
    // `stb <0>, 0x28(behaviour)` immediately after NewBehaviour<BehaviourIceAnim>:
    //     ArbStateCarSelect::Prepare  @0x8226F0C4 and @0x8226F1A8   (r23 == 0, li r23,0 @0x8226EFD4)
    //     ArbStateRaceIntro::Update   @0x8226E730                   (r10 == 0)
    // Behaviour +0x28 is NOT a base-Behaviour field: the Behaviour base ends at +0x14 (vptr,
    // meTimestepType @+4, five flag bytes @+8..+0xC, mpcDebugParametersName @+0x10), and
    // mCollisionPolicy sits at +0x20 -- so +0x28 is mCollisionPolicy + 0x08, which the DecFIGS
    // DWARF (BrnCollisionPolicy.h:436) names VisibilityCollisionPolicy::mbCanFail and which
    // BehaviourIceAnim::Construct @0x8225624C seeds to 1 through this same pointer. So the
    // arbitrator states are saying "this take's camera may NOT fail out for occlusion /
    // collision / off-screen", not "clear a first-frame gate".
    void SetCollisionPolicyCanFail(bool lbCanFail) { mCollisionPolicy.SetCanFail(lbCanFail); }

    // ⛔ DEPRECATED ALIAS -- MIS-NAMED, KEPT ONLY SO THE 17 EXISTING CALL SITES KEEP LINKING.
    // It was a guess ("clear the base-behaviour first-frame gate") at the +0x28 store decoded
    // above, and it was declaration-only, which is why it was one of this wave's unresolved
    // externals. Callers today: BrnArbStateCarSelect.cpp (9), BrnArbStateOnlineRaceIntro.cpp
    // (3), BrnArbStateRaceIntro.cpp (2), BrnArbStatePostEvent.cpp, BrnArbStateRankUp.cpp,
    // BrnArbStateTakedown.cpp. DELETE-WHEN: all six files are re-pointed at
    // `SetCollisionPolicyCanFail(false)` (a straight textual substitution:
    //   ClearBaseFirstFrameGate()  ->  SetCollisionPolicyCanFail(false)).
    void ClearBaseFirstFrameGate() { SetCollisionPolicyCanFail(false); }   // policy +0x08 = 0

    // ---- rank-up arbitrator-state pokes (BrnArbStateRankUp::Prepare/Update) -------------
    // Bind the behaviour's primary anchor vehicle reference (mPrimaryVehicleRef @+0xDF0) to a
    // race car, so the rank-up take frames that rival's car (X360:
    // VehicleRef::SetToRaceCar(this+0xDF0, raceCarIndex)). Exposed as a named setter so the
    // arbitrator state never reaches the (private) ref by offset. DECLARATION-ONLY (the
    // VehicleRef::SetToRaceCar body lands with the VehicleRef TU; the per-TU cl /c gate does
    // not link).
    void SetPrimaryVehicleRefToRaceCar(EActiveRaceCarIndex leRaceCar);

    // ---- post-event arbitrator-state poke (BrnArbStatePostEvent::Prepare) ---------------
    // Seed the behaviour's BYSTANDER anchor vehicle reference (mBystanderRef @+0xE10) to the
    // fixed value the post-event take frames the winner against. X360 (Prepare @0x8226E228)
    // stores into mBystanderRef directly: word +0x00 = 0, word +0x04 = -1, word +0x08 = 0,
    // byte +0x0C = 1. Exposed as a named setter so the arbitrator state never pokes the
    // (private) ref by offset. DECLARATION-ONLY (the body lands with this behaviour's TU,
    // which owns mBystanderRef). FLAG: the four bystander-ref fields' individual roles are
    // not recovered (modelled as a fixed post-event seed; the field WRITES are asm-attested).
    void SetBystanderRefForPostEvent();

    // ADDITIVE GROW (BrnArbStateTakedown.cpp -- SimpleIceTakedownPlayer::Prepare @0x8226CF38).
    // Bind the bystander anchor vehicle reference (mBystanderRef @+0xE10) to a specific race car,
    // the same 4-word {kind=1, index, 0, valid=1} pattern as SetSecondaryVehicleRefToRaceCarIndex
    // (X360 asserts liRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars, BrnVehicleRef.h:222,
    // same as that sibling setter). DECLARATION-ONLY (the body lands with this behaviour's TU,
    // which owns mBystanderRef); exposed as a named setter so the takedown player never pokes
    // the (private) ref by offset.
    void SetBystanderRefToRaceCarIndex(s32 liRaceCarIndex);

    // Seek the embedded key-anim controller's normalised playback parameter (mKeyAnimController
    // @+0x680) to lf01. The rank-up state rewinds a freshly-changed take to its start with 0.0
    // (X360: KeyAnimController::SetParametricTime0To1(this+0x680, 0.0)). Exposed as a named
    // method so the arbitrator state never reaches the (private) controller by offset.
    void SetControllerParametricTime0To1(f32 lf01) { mKeyAnimController.SetParametricTime0To1(lf01); }

    // Seconds of take left to play (the un-played fraction times the take length).
    f32  GetTimeRemaining();

private:
    // +0x0020  the free visibility collision policy.
    VisibilityCollisionPolicy        mCollisionPolicy;

    // +0x0260  the attached-to-car collision policy.
    CollisionPolicyAttachedToVehicle mAttachedToCarCollisionPolicy;

    // +0x04B0  the camera this behaviour produces each frame.
    Camera                           mLastCamera;

    // +0x0610  the behaviour's own HEADING-SPACE frame: the look-at built from the secondary
    // (look-at) vehicle, slerped forward each frame and handed to the take evaluator as the
    // heading reference space. CARVED 2026-07-31: the Update asm reaches it with
    // `addi r11, this, 0x610` / `addi r29, this, 0x610` / `stvx r31, 0x640` -- and
    // mLastCamera starts at +0x4B0 with sizeof(Camera) == 0x160, i.e. it ENDS at exactly
    // +0x610, so this is a separate 64-byte matrix member, not part of the camera. (The
    // previous reconstruction wrote these stores into `mLastCamera.mTransform`, i.e. 0x160
    // bytes too low.)
    rw::math::vpu::Matrix44Affine    mHeadingSpaceTransform;

    // +0x0650  the camera shake the take's shake space drives.
    Utils::CameraShake               mShake;

    // +0x0660  the looker post-process the take's look space drives.
    Utils::Looker                    mLooker;

    // +0x0680  the live key-anim (ICETake) controller; embeds the ICETake at its +0x20.
    KeyAnimController                 mKeyAnimController;

    // +0x0DE0  the per-take reset sub-block (sits just before the VehicleRefs). Construct
    // and ChangeMovie both clear it: the f32 to 0.0f and the four trailing bytes to 0.
    f32                              mfReset0DE0;
    u8                               maReset0DE4[4];

    // +0x0DF0 / +0x0E00 / +0x0E10  the eye / look / bystander anchor vehicle references.
    Behaviour::VehicleRef            mPrimaryVehicleRef;
    Behaviour::VehicleRef            mSecondaryVehicleRef;
    Behaviour::VehicleRef            mBystanderRef;

    // +0x0E20  the take guid SetParameters reads from the parameter block.
    s32                              miAnimGuid;

    // +0x0E24  the parameter block last passed to SetParameters.
    ShotReference*                   mpSourceShot;

    // +0x0E28..+0x0E2B  the four behaviour-mode flags.
    bool                             mbUseCollisionPolicy;
    bool                             mbUseAttachedToCarCollisionPolicy;
    bool                             mbForceHeadingSpaceToBeLooseHeadingSpace;
    bool                             mbForceMotionBlurEverything;

    // The live take-data pointer Prepare / ChangeMovie bind (`lpTakeData + 0xC`).
    // FLAG (home): the console keeps this in the BASE's +0x10 slot, which the DWARF names
    //   `Behaviour::mpcDebugParametersName` (a `const char*`). The two readings are
    //   reconcilable -- `lpTakeData + 0xC` is plausibly the take's NAME string, i.e. the
    //   console line is `SetDebugParametersName(takeData->mpcName)` -- but that is NOT
    //   proven, and aliasing a `const char*` base field with a take-data pointer would be a
    //   guess with teeth. It is therefore given a NAMED member of its own here (the x64 gate
    //   is semantic parity by named member, so the extra word costs nothing), and the base
    //   field is left alone.
    //   DELETE-WHEN: the ICE take record's +0xC field is typed (if it is the name string,
    //   fold this into SetDebugParametersName / GetDebugParametersName and drop the member).
    void*                            mpCurrentTakeData;
};

} } // namespace BrnDirector::Camera

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H
