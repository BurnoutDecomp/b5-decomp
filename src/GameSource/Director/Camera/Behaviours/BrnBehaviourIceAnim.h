#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                               // rw::math::vpu::Matrix44Affine / Vector3 / Vector4
#include "GameSource/Director/Camera/Camera.h"               // BrnDirector::Camera::Camera (mLastCamera, by value)
#include "GameSource/Director/Utils/BrnVehicleRef.h"         // BrnDirector::VehicleRef (the VehicleRef members)
#include "SDKs/Packages/ICE/ICEData.hpp"                     // ICE::ICETake / ICE::ICETakeData (controller take + take data)
#include "SharedClasses/DataLists/ICEList.h"                 // BrnResource::ICEList::GetICETakeDataFromGuid
#include "GameSource/AttribSys/Generated/classes/iceanim.h"  // Attrib::Gen::iceanim (the real/canonical home)

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

// ----------------------------------------------------------------------------
// FLAG: minimal slice of ICE::ICEAuthor (the in-game editor's author/edit store). No
//   reconstructed home yet (ICEController.hpp notes the ICEAuthor base is not modelled).
//   Prepare / ChangeMovie call exactly the one named accessor below -- the editor-edited
//   take lookup by guid -- declaration-only (the per-TU `cl /c` gate does not link).
//   Replace with its real home when the ICEAuthor base TU lands; the NAME is stable.
// ----------------------------------------------------------------------------
namespace ICE
{
    class ICEAuthor
    {
    public:
        // The edited-take store the editor keeps; null when the guid is not being edited.
        ICETakeData* FindEditedTakeFromGuid(s32 liGuid);
    };
}

// The director "rank-up" shot-group lives in the AttribSys generated layer; the resource
// manager embeds one and hands it back by reference. Forward-declared so the minimal
// DirectorResourceManager slice can name it without pulling the generated header into this
// behaviour header (the using TU includes shotgroup.h to reach Num_ShotList()/GetShotListData).
namespace Attrib { namespace Gen { class shotgroup; } }

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// FLAG: minimal slice of the director resource manager Prepare / ChangeMovie resolve
//   takes through. The behaviour reaches exactly the two named providers below: the
//   in-game editor's author (edited-take lookup) and the runtime ICE list (on-disk take
//   lookup by guid). The behaviour reaches the author at the manager's +560 and the
//   ICE list at +544; modelled here as the two named accessors. Replace with the real
//   home when the DirectorResourceManager TU lands; the NAMES are stable.
// ----------------------------------------------------------------------------
class DirectorResourceManager
{
public:
    ICE::ICEAuthor&        GetICEAuthor() const;   // manager +560
    const BrnResource::ICEList& GetICEList() const;  // manager +544

    // The intro shot-group for an event mode. @0x821F6AB8: a switch on the event mode that
    // returns a pointer to one of the manager's embedded shot-group records (manager + a
    // mode-specific offset); lbCarInFront selects between the two shot variants per mode.
    // Returns the shot group as an opaque pointer (the Attrib::Gen::shotgroup the arbitrator
    // race-intro state drives). DECLARATION-ONLY (body in the resource-manager TU). FLAG:
    // modelled on this behaviour's minimal DirectorResourceManager slice (the in-scope model
    // for the director-camera cone); the real home is BrnDirectorResourceManager.h.
    const void* GetEventIntroShots(s32 liEventMode, bool lbCarInFront) const;

    // The event-COMPLETION shot-group for an event mode + finish line (the post-event
    // results / winner take). @0x????: BrnArbStatePostEvent::Prepare resolves it as
    // GetEventCompletionShots(mpGameState->meEventType, mpGameState->mFinishLineID) and
    // asserts Num_ShotList() > 0 before driving it. Returns the shot group BY REFERENCE
    // (the temporary the X360 builds on the caller stack; PickAppropriateShot takes it by
    // const ref and Num_ShotList() is called on it). DECLARATION-ONLY (body in the
    // resource-manager TU). FLAG: modelled on this minimal DirectorResourceManager slice;
    // the real home is BrnDirectorResourceManager.h. liFinishLineID is the GameState
    // mFinishLineID (an 8-byte CgsID); typed as s64 here to keep the camera-cone slice free
    // of the CgsID header (the VALUE is what selects the completion group).
    const Attrib::Gen::shotgroup& GetEventCompletionShots(s32 liEventMode,
                                                          s64 liFinishLineID) const;

    // The post-event completion MOVIE data block the ACTIVE state swaps to on a rank-up
    // reveal. X360 (BrnArbStatePostEvent::Update case ACTIVE): the manager embeds an
    // Attrib::Instance at +0x508 (manager + 1288); the state resolves its "ShotList"
    // attribute via Attrib::Instance::GetAttributePointer (falling back to
    // DefaultDataArea(0x18) when null) and hands the block to BehaviourIceAnim::ChangeMovie.
    // Returned as an opaque pointer (the iceanim ShotReference block); the caller casts.
    // DECLARATION-ONLY (body in the resource-manager TU). FLAG: modelled on this minimal
    // DirectorResourceManager slice; the real home is BrnDirectorResourceManager.h.
    void* GetPostEventMovieData() const;

    // The rank-up sequence's shot-group (the manager's embedded shotgroup @+0x3C8). The
    // rank-up arbitrator state (BrnArbStateRankUp) drives it: it asserts Num_ShotList() > 0
    // and indexes the shots by rival. DECLARATION-ONLY (body in the resource-manager TU).
    // FLAG: modelled on this minimal DirectorResourceManager slice; the real home is
    // BrnDirectorResourceManager.h. X360: ArbStateRankUp reads it at manager +968 (+0x3C8).
    const Attrib::Gen::shotgroup& GetRankUp() const;

    // The online car-select reveal shot-group (the manager's embedded shotgroup @+0x5C8). The
    // online-car-select arbitrator state (BrnArbStateOnlineCarSelect::Prepare) feeds its
    // ShotList data block to the ICE-anim reveal behaviour's SetParameters. DECLARATION-ONLY
    // (body in the resource-manager TU). FLAG: modelled on this minimal DirectorResourceManager
    // slice; the real home is BrnDirectorResourceManager.h. X360: ArbStateOnlineCarSelect reads
    // it at manager +1480 (+0x5C8) and resolves it through the same ShotList attribute key
    // (0x7533C0E2_15246B49) the shotgroup accessor uses.
    const Attrib::Gen::shotgroup& GetOnlineCarSelectShots() const;

    // The two crash-moment shotgroup banks (X360 manager +1448/+1464, the 16-byte
    // stride after the fast/normal/slow crash trio) and the guid->take resolver
    // (@0x821F69A8). GROWN for MomentStationaryCrash::Update @0x82272EA8.
    // DECLARATION-ONLY (bodies in the resource-manager TU). FLAG: modelled on this
    // minimal DirectorResourceManager slice; the real home is
    // BrnDirectorResourceManager.h (which carries the same decls).
    const Attrib::Gen::shotgroup& GetTumblingCrashShots() const;     // +1448
    const Attrib::Gen::shotgroup& GetStationaryCrashShots() const;   // +1464
    ICE::ICETakeData* GetKeyAnimFromGuid(s32 liGuid) const;          // @0x821F69A8
    ICE::ICETakeData* GetKeyAnim(s32 liHashedNameId) const;          // @0x821F6948 (the hashed-name lookup; MomentPlayerStunt's "World_Signature_%i" takes)

    // The online-race-start shot-group (the manager's embedded shotgroup @+616 / +0x268). The
    // online-race-intro arbitrator state (BrnArbStateOnlineRaceIntro::Update / ::SetupRivalMovie)
    // drives it: it asserts Num_ShotList() >= the ICE-movie minimum, indexes the per-rival shots
    // and feeds each ShotList data block to a BehaviourIceAnim's SetParameters. DECLARATION-ONLY
    // (body in the resource-manager TU). FLAG: modelled on this minimal DirectorResourceManager
    // slice; the real home is BrnDirectorResourceManager.h. X360: ArbStateOnlineRaceIntro reads
    // it at manager +616 (+0x268) and resolves it through the same ShotList attribute key
    // (0x7533C0E2_15246B49 / 354708297) the shotgroup accessor uses.
    const Attrib::Gen::shotgroup& GetOnlineRaceStartShots() const;

    // ADDITIVE GROW (BrnArbStateDriveThru::Prepare @0x8226E938): the five drive-thru shop
    // shot-groups (the manager embeds one shotgroup per shop kind, at per-type offsets, NOT a
    // contiguous array -- ArbStateDriveThru::Prepare itself switches on GameState::meDriveThruType
    // to pick which one to read, so that switch stays in Prepare; these are just the five named
    // per-type slots it reads). X360 offsets:
    //   GetDriveThruAutoPartsShots  -> manager +1032 (+0x408)  E_DRIVETHRU_AUTO_PARTS(1)
    //   GetDriveThruBodyShopShots   -> manager +1000 (+0x3E8)  E_DRIVETHRU_BODY_SHOP(2)
    //   GetDriveThruGasStationShots -> manager +984  (+0x3D8)  E_DRIVETHRU_GAS_STATION(3)
    //   GetDriveThruTuningShopShots -> manager +1048 (+0x418)  E_DRIVETHRU_TUNING_SHOP(4)
    //   GetDriveThruTireShopShots   -> manager +1016 (+0x3F8)  E_DRIVETHRU_TIRE_SHOP(5) (also
    //     the fallback Prepare's default case asserts "unhandled drivethru type" then reads)
    // DECLARATION-ONLY (body in the resource-manager TU). FLAG: modelled on this minimal
    // DirectorResourceManager slice; the real home is BrnDirectorResourceManager.h.
    const Attrib::Gen::shotgroup& GetDriveThruAutoPartsShots() const;
    const Attrib::Gen::shotgroup& GetDriveThruBodyShopShots() const;
    const Attrib::Gen::shotgroup& GetDriveThruGasStationShots() const;
    const Attrib::Gen::shotgroup& GetDriveThruTuningShopShots() const;
    const Attrib::Gen::shotgroup& GetDriveThruTireShopShots() const;
};

// FLAG: minimal slice of the per-frame timestep source the Update body samples. No
//   reconstructed home yet -- modelled with exactly the named accessor Update uses
//   (Get(eType) returns the scaled frame delta). Replace with its real home when the
//   Timestep TU lands; the accessor NAME is stable.
struct Timestep
{
    enum EType { E_TYPE_DEFAULT = 0 };
    f32 Get(EType leType) const;
};

namespace Camera
{

// ----------------------------------------------------------------------------
// FLAG: minimal slices of the Director camera-behaviour support layer reached by this
//   behaviour. None of these has a reconstructed home yet; each is modelled with only the
//   named members/methods this behaviour's bodies touch, accessed BY NAME. Replace each
//   with its real home when its own Camera TU is reconstructed -- the member/method NAMES
//   are stable. Pointer/handle-only types are forward-declared.
// ----------------------------------------------------------------------------

// The base of all camera behaviours: a vtable plus a shared flag/state block. The shared
// fields below are the ones this behaviour's bodies read or write through `this`; the rest
// of the base block is an opaque reserved span. Construct/Update/Prepare touch these by
// name. FLAG: minimal slice -- the real base layout lands with the Behaviour TU.
class Behaviour
{
public:
    // The reference to one anchor vehicle (eye / look / bystander) a behaviour keeps. The
    // bodies query validity and resolve it to a live vehicle each frame. FLAG: minimal
    // slice -- the real type lands with the Behaviour TU; offsets here are nominal.
    class VehicleRef
    {
    public:
        // True when this reference currently resolves against the given world.
        bool IsValid(const void* lpWorld) const;
        // Resolve to the live vehicle (Update reads its transform rows from the result).
        void* Get(const void* lpWorld) const;

        u8 maReserved[0x10];   // +0x00  nominal (the ref is 16 bytes wide)
    };

    // Mark "the director cannot switch away from me this frame" with the given reason id.
    // (Body homed in Behaviour.cpp.)
    void SetCantSwitchFromMeNow(void* lpInfo, s32 liReason);

    // Give up: record the failure reason in the info's validity account, raise the failed
    // flags on this behaviour and drop the "follow" request bit on the info. The Update of
    // every concrete behaviour calls it when its anchor stops resolving. (Body in
    // Behaviour.cpp.)
    void Fail(void* lpInfo, s32 liReason);

    // ADDITIVE GROW (BrnArbStateDriveThru::Update @0x82235DB0): a public read of the shared
    // "flag C" byte (+0x0C -- see mbBaseFlagC below / Behaviour.cpp's SetCantSwitchFromMeNow /
    // Fail, which both write it). X360: `lbz r11, 0xC(r3)` on the live Behaviour* an unnamed
    // handle-resolution helper (sub_821FCDA8, owning TU not yet identified) hands back; the
    // drive-thru state gates its hand-off-to-roaming transition on this byte. FLAG: the exact
    // semantic role of mbBaseFlagC (beyond "written true by Fail / cleared by
    // SetCantSwitchFromMeNow") is not fully recovered; exposed as a named read so the consumer
    // never pokes the base by offset.
    bool GetBaseFlagC() const { return mbBaseFlagC; }

protected:
    // The shared head of the base block. The named flags below land inside it.
    void* mpVTable;            // +0x00  behaviour vtable

    // +0x04 reset by Construct.
    s32  miBaseResetWord;      // +0x04

    // +0x08..+0x0C: the shared boolean state the bodies read/write by name.
    bool mbHasPreparedCamera;  // +0x08  set once the produced camera has been seeded
    bool mbFailed;             // +0x09  set when the behaviour gave up (HasFinishedOrFailed)
    bool mbBaseFlagA;          // +0x0A  set by Update's setup
    bool mbMotionBlurGate;     // +0x0B  motion-blur enable gate (Update toggles it)
    bool mbBaseFlagC;          // +0x0C

    // +0x0D..+0x0F: padding to the next named member.
    u8   maBasePad0[3];        // +0x0D

    // +0x10: the take-data pointer Prepare / ChangeMovie bind (lpTakeData + 0xC).
    void* mpCurrentTakeData;   // +0x10
};

// ----------------------------------------------------------------------------
// The two collision policies the behaviour exposes. GetCollisionPolicy hands back a
// pointer to one (or null). They are embedded by value at fixed offsets in the behaviour.
// FLAG: minimal slices -- the real policy layouts/method sets land with their own TUs.
// ----------------------------------------------------------------------------

// The base collision-policy interface GetCollisionPolicy returns through. Vtable only here.
class CollisionPolicy
{
public:
    virtual ~CollisionPolicy() {}
};

// The "free" visibility collision policy (embedded at +0x20). Sized so its vtable lands at
// the behaviour's +0x20 and the attached-to-car policy follows at +0x260.
//
// FLAG: minimal slice -- only the three see-through state bytes the Update gate reads are
// named here (policy-relative +0x1A0/+0x1A1/+0x1A2). The Update see-through gate consults
// these three bytes before raising the camera's see-through flag. The real policy layout /
// method set lands with the VisibilityCollisionPolicy TU; the byte NAMES are stable.
class VisibilityCollisionPolicy : public CollisionPolicy
{
public:
    // True when the see-through flag should be raised this frame:
    //   mbSeeThroughAlways || (mbSeeThroughEnabled && !mbSeeThroughSuppressed).
    bool ShouldRaiseSeeThrough() const
    {
        return mbSeeThroughAlways || (mbSeeThroughEnabled && !mbSeeThroughSuppressed);
    }

    u8   maReservedHead[0x1A0 - sizeof(void*)];  // span from the vtable to the state bytes
    bool mbSeeThroughEnabled;                    // +0x1A0  (default true)
    bool mbSeeThroughAlways;                     // +0x1A1  (default false)
    bool mbSeeThroughSuppressed;                 // +0x1A2  (default true)
    u8   maReservedTail[0x240 - 0x1A3];          // span to the next embedded policy (+0x260)
};

// The "attached to the car" collision policy (embedded at +0x260). Construct builds it.
class CollisionPolicyAttachedToVehicle : public CollisionPolicy
{
public:
    // Build the policy in place. Construct takes a trailing 0 selector.
    void Construct(s32 liSelector);

    u8 maReserved[0x250 - sizeof(void*)];   // span to the camera (+0x4B0)
};

// The camera-shake post-process the take's shake space drives (embedded at +0x650).
// Update samples it once per frame; Construct seeds its parameters by name. FLAG: minimal
// slice -- the real layout/method set lands with the CameraShake TU.
namespace Utils
{
    class CameraShake
    {
    public:
        u8 maReserved[0x10];   // +0x00  nominal (the shake is 16 bytes wide)
    };

    // The looker post-process (embedded at +0x660). Update runs it for the look-at space.
    // FLAG: minimal slice -- the real layout/method set lands with the Looker TU.
    class Looker
    {
    public:
        u8 maReserved[0x20];   // +0x00  nominal
    };

    // The dev-tools tweaker SetupTweaker hands the behaviour's editable parameters to.
    // FLAG: minimal slice -- the real home is the camera-utils dev-tools TU. SetupTweaker
    // only tail-calls Tweaker::Construct(tweaker), so just that static is modelled.
    class Tweaker
    {
    public:
        static void Construct(Tweaker& lrTweaker);
    };
}

// The key-anim controller: the live ICETake evaluator the behaviour advances. It embeds an
// ICE::ICETake at its +0x20 (the behaviour's ctor constructs that ICETake and GetTimeRemaining
// reads the take's data/length through it). Prepare binds it to the resource manager + guid,
// Update advances it and reads its eye/look spaces. FLAG: minimal slice -- the real layout
// and the rest of the method set land with the KeyAnimController TU; the offsets/NAMES below
// are pinned from this behaviour's accesses.
} // namespace Camera

// The key-anim controller lives directly under BrnDirector
// (BrnDirector::KeyAnimController), not under Camera.
class KeyAnimController
{
public:
    // Bind the controller to a take resolved from the resource manager by guid. Returns
    // true once the take data is bound. The behaviour also re-prepares it mid-Update.
    bool Prepare(const DirectorResourceManager& lrResourceManager, s32 liAnimGuid);

    // True once the controller's parameter has run off the end of the take.
    bool HasFinished() const;

    // The controller's normalised playback parameter [0..1].
    f32  GetParametricTime0To1() const;

    // Seek the controller's normalised playback parameter to lf01. The rank-up arbitrator
    // state rewinds a freshly-swapped take to its start (X360
    // KeyAnimController::SetParametricTime0To1(behaviour+0x680, 0.0)). DECLARATION-ONLY (the
    // body lands with the KeyAnimController TU; the per-TU cl /c gate does not link).
    void SetParametricTime0To1(f32 lf01);

    // The eye / look reference-space selectors of the currently-bound take (the Update
    // body switches on these to pick the heading/eye/look space rows).
    s32  GetEyeSpace() const;
    s32  GetLookSpace() const;

    // The current look-target position (Update feeds it into the looker parameters).
    const rw::math::vpu::Vector3& GetLookPos() const;

    // The controller's per-frame entry point: advance the take and write into the camera.
    // The behaviour calls it through the controller's vtable. Modelled as the named update.
    void Update(const void* lpSpaces, void* lpInfo);

    void* mpVTable;            // +0x00  controller vtable (Update is dispatched through it)
    u8    maReserved[0x20 - sizeof(void*)]; // +0x?? span to the embedded ICETake (+0x20)
    ICE::ICETake mTake;        // +0x20  the live evaluatable take
};

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
// FLAG: minimal slices of the two Behaviour information blocks this behaviour's virtuals
//   consume. No reconstructed home yet (the Behaviour shared-info TU owns them); each is
//   modelled with exactly the named accessors the bodies read. The Update body also OR-/
//   AND-sets two camera-request flag words on the info block (the +312 / +320 flag
//   words); those are expressed as the named request helpers below. Replace with the real
//   homes when the Behaviour shared-info TU lands; the accessor NAMES are stable.
// ----------------------------------------------------------------------------

// The prepare/release info Prepare consumes (carries the resource manager).
struct BehaviourSharedPrepareReleaseInfo
{
    const DirectorResourceManager* GetDirectorResourceManager() const;
};

// The per-frame info Update consumes (world, spaces, timestep, resource manager, the
// produced-camera request flags, the debug sink).
struct BehaviourSharedInfo
{
    const void*              GetWorld() const;             // +1468  the world the refs resolve against
    bool                     ShouldRePrepareController() const; // +1517 re-prepare gate
    const DirectorResourceManager& GetDirectorResourceManager() const; // +1448
    void*                    GetSpaceArgs() const;         // +1280..  the look-at build args
    const void*              GetSourceSpaces() const;      // +1508  source reference spaces
    void*                    GetInfoPointer() const;       // the info block itself (controller arg)
    const BrnDirector::Timestep& GetTimestep() const;      // +1360  per-frame timestep source
    void*                    GetDebugSink() const;         // +1488  debug-print sink
    const void*              GetEyeTarget() const;         // +592   eye target (visibility test)
    const void*              GetLookTarget() const;        // +1280  look target (visibility test)

    // The two produced-camera request flags the Update body raises:
    //   "give up following" (the early-out path) and "follow" (the normal path).
    void RequestNoFollow();   // sets +312 |= 0x800, +320 &= ~2
    void RequestFollow();     // sets +320 |= 2
};

// ============================================================================
// BehaviourIceAnim -- the ICE-anim camera behaviour itself.
// ============================================================================
class BehaviourIceAnim : public Behaviour
{
public:
    // The arbitrator-side parameter block SetParameters / ChangeMovie consume. It is the
    // generated iceanim attrib instance; the behaviour only reads its class key + guid.
    typedef Attrib::Gen::iceanim ShotReference;

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

    // GROWN for MomentStationaryCrash::Update @0x82272EA8 (the moment polls its
    // candidate through the Behaviour-base head bytes): the DWARF base names for
    // +0x09 / +0x0B. NOTE: the +0x0B byte the slice models as mbMotionBlurGate IS
    // the base's mbCanSwitchToMeNow (Behaviour.h DWARF); the behaviour raising it
    // from Update is it signalling readiness -- the member keeps the slice name
    // pending its own-TU reconcile.
    bool HasFailed() const          { return mbFailed; }
    bool CanSwitchToMeNow() const   { return mbMotionBlurGate; }
    bool CanSwitchFromMeNow() const { return mbBaseFlagC; }   // +0x0C == the base mbCanSwitchFromMeNow (DWARF base name; MomentPlayerStunt's release gate)

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

    // Clear the base-behaviour "first-frame" gate the intro state resets (X360 stb 0 at the
    // base behaviour's +0x28 -- a field beyond this header's modelled base slice). DECLARATION-
    // ONLY: the body lands with the Behaviour base TU that owns that offset; the per-TU cl /c
    // gate does not link.
    void ClearBaseFirstFrameGate();   // base +0x28 = 0

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
};

} } // namespace BrnDirector::Camera

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ICE_ANIM_H
