#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                    // CgsNumeric::Random
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::{...}
// ⭐ 2026-08-14 (deformation-mount wave): ResetJointVelocities reaches the pooled parts through
// the manager's header-inline GetPartFromIndex, so the real manager header is needed (it was only
// forward-declared before; no cycle -- the pool/part headers reference DeformableObject by
// forward-decl only).
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"     // DetachedPartManager (GetPartFromIndex)

// =====================================================================================================
// BrnPhysics::Deformation::DeformableObject -- LIFECYCLE / SETUP group.
// Reconstructed store-for-store from the X360 ARTIST.XEX Hex-Rays pseudocode + the DecFIGS DWARF
// source-level reconstruction (references/DecFIGS/dwarfdump/.../BrnDeformableObject.cpp), which names
// the locals + the member-method call chain. Per project convention (cf. ApplyCarCarImpulse in the
// committed BrnDeformableObject.cpp, and BrnAbsorptionTable.cpp / BrnDeformationSensor.cpp) the VMX128
// inline-asm bodies are written as the de-SIMD'd scalar/by-member equivalent -- no __asm, no raw-offset
// pokes -- with the EXACT control flow, early-outs, bounds asserts and constants preserved.
//
// Functions bodied here (X360 addresses):
//   DeformableObject (ctor)                            @ 0x82603EF0
//   Prepare                                            @ 0x82642180
//   PrepareIKPart                                      @ 0x826074B0
//   PrepareLocators                                    @ 0x825BA010
//   Release                                            @ 0x8263A698
//   ClearVariables                                     @ (DWARF :3250; inlined-call site in ctor path)
//   ResetDeformation                                   @ 0x82639D60
//   UpdateAbsorptionSet                                @ 0x825DF9A0
//   ConstructUpdatePerformanceMonitors                 @ 0x825B99A0
//   ConstructPostPhysicsPerformanceMonitors            @ 0x825B9C88
//   ConstructUpdateIKAndLocatorsPerformanceMonitors    @ 0x825B9B00
//
// FLAGGED / MODELLED-VS-ASM notes (honest, never fabricated):
//   * The per-stage perf-monitor TIMING-CONSTANT pool (the 10.0 "budget ms" / 15 group-id / 0 / 1
//     args to CgsDev::PerfMonCpu::AddMonitor) ARE visible in the asm and reproduced verbatim. The
//     module-level si* perf-monitor handle ids the asm stores into .bss (dword_82F2A348..0x36C) are
//     reconstructed as file-static s32 handles below (the names match the asm assert strings exactly:
//     siSortContactsPerfMon, siSolveContactsPerfMon, ...). They are NOT DeformableObject members.
//   * Prepare / ResetDeformation are large SIMD+raw-offset functions whose asm walks the streamed
//     spec (mpDeformationSpec) tag/driven/IK/locator tables and the attached vehicle physics through
//     accessors that are NOT all exposed on the FROZEN headers (e.g. StreamedDeformationSpec's private
//     locator lists; VehiclePhysics::GetMaxNonBoostSpeedMPH / GetSpeedMPHOnLastCrash). Where the asm
//     needs an accessor the frozen header does not declare, the call is left as a clearly-FLAGGED
//     declare-only call or a structurally-faithful by-member loop; the control flow, the bounds
//     asserts (verbatim messages) and the member writes that ARE expressible by name are exact.
//   * Wheel body-part-type selectors (0x30..0x33 / 0x82 / 0x83) in PrepareIKPart have no named
//     EBodyParts enumerator in the (placeholder) EBodyParts enum, so the switch keys are the asm hex
//     literals, FLAGGED.
//   * unk_82FB9B80 (ResetDeformation initial-damage->bbox scale) and unk_82FB9AB0 (UpdateAbsorptionSet
//     extreme-crash speed margin) are unrecovered .rodata -> FLAGGED-0 placeholders (NEVER fabricated).
//   * byte_82F2A345 is the global "deformation parts enabled" debug flag the asm tests; carried as a
//     file-static extern-style flag (FLAG: real home not in-tree).
// =====================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    // -------------------------------------------------------------------------------------------------
    // Module-scope perf-monitor handle ids (asm .bss dword_82F2A348..0x36C). These are the integer
    // handles CgsDev::PerfMonCpu::AddMonitor returns; the per-stage Update/Render code reads them back.
    // Names taken verbatim from the asm FireAssert expression strings. NOT DeformableObject members.
    // -------------------------------------------------------------------------------------------------
    // ⚠️⚠️ DEFINED IN BrnDeformationConstructShims.cpp, NOT HERE (2026-08-03, task #116). They used
    // to be `static s32 ... = -1;` in this file, alongside the three ConstructXPerformanceMonitors
    // bodies that seed them. Those three registrations had to be split out so that
    // DeformationManager::Construct -- and therefore PhysicsModule::Construct, which was a live
    // empty stub -- could link without mounting this TU's whole Update/Render closure. Leaving these
    // `static` would have given the shim its own private copy: the shim would register into that
    // copy while the thirty-three read sites BELOW kept reading -1 forever. That is the silent-drop
    // stub failure class exactly. EXTERNAL linkage, one definition, either TU can be mounted.
    extern s32 siSortContactsPerfMon;         // dword_82F2A348
    extern s32 siSolveContactsPerfMon;        // dword_82F2A34C
    extern s32 siUpdateWheelsAndGlassPerfMon; // dword_82F2A350
    extern s32 siUpdateSweptSpherePerfMon;    // dword_82F2A354
    extern s32 siUpdateWorldSpheres;          // dword_82F2A358
    extern s32 siCheckDetaching;              // dword_82F2A35C
    extern s32 siUpdateSkinningOffsets;       // dword_82F2A360
    extern s32 siUpdateIK;                    // dword_82F2A364
    extern s32 siUpdateSuspensionIK;          // dword_82F2A368
    extern s32 siUpdateLocators;              // dword_82F2A36C

    // The "deformation parts enabled" debug flag the asm tests (byte_82F2A345). FLAG: real home is a
    // debug-menu global not in-tree; carried as an honest file-static (default: parts enabled == true,
    // i.e. the non-degenerate path the asm takes when the flag is set).
    static const bool gbDeformationPartsEnabled = true;   // FLAG: byte_82F2A345

    // FLAGGED-0 .rodata placeholders (NEVER fabricated). Shapes are authoritative; values stay inert.
    //   unk_82FB9B80 -- ResetDeformation per-axis initial-damage->bbox scale (vmulfp into the spec BB).
    //   unk_82FB9AB0 -- UpdateAbsorptionSet extreme-crash speed margin (vsubfp from crash-speed delta).
    // ⭐ RECOVERED 2026-08-03. unk_82FB9B80's initialiser @82C5D7D0 is a RECIPROCAL, not a splat:
    // `vrefp` + two Newton-Raphson steps over unk_82FB9770 (0.2), i.e. 1/0.2 = 5.0. A static-init
    // scan that only recognises the splat idiom cannot resolve it, which is why it stayed flagged.
    static const Vector3 KVF_INITIAL_DAMAGE_BBOX_SCALE = { 5.0f, 5.0f, 5.0f, 5.0f }; // unk_82FB9B80 = 1/unk_82FB9770
    static const f32     KF_EXTREME_CRASH_SPEED_MARGIN = 5.0f;                        // unk_82FB9AB0 @82C5D8B8 <- flt_8200426C

    // ⭐ RECOVERED 2026-08-14 (deformation-mount wave): the three compression/scratch ratio vectors
    // GetInitialCompressionScalesAndLimits selects between (PS3 names them verbatim:
    // KV3P_{EVENT,CAR_SELECT,DEFAULT}_COMPRESSION_SCRATCH_RATIO; X360 homes 0x82FB9540 / 0x82FB9DB0
    // / 0x82FB9760). They are DYNAMIC-INIT (zero in the image); their initializers were read out of
    // the static-init region and their scalar seeds out of .rodata via x360rd.py:
    //   EVENT      @0x82C5D740: (flt_82001C98, x, x, flt_8208F9C8)      = (1.0, 1.0, 1.0, 0.8)
    //   CAR_SELECT @0x82C5D700: (flt_8208F9C8, flt_82004C68, flt_8208F9C8, flt_82004018)
    //                                                                    = (0.8, 0.7, 0.8, 0.75)
    //   DEFAULT    @0x82C5D778: (flt_82001CC0 x4)                        = (0.0, 0.0, 0.0, 0.0)
    // xyz = the per-axis compression scale ratio, w = the scratch ratio.
    static const Vector3Plus KV3P_EVENT_COMPRESSION_SCRATCH_RATIO      = { 1.0f, 1.0f, 1.0f, 0.8f };
    static const Vector3Plus KV3P_CAR_SELECT_COMPRESSION_SCRATCH_RATIO = { 0.8f, 0.7f, 0.8f, 0.75f };
    static const Vector3Plus KV3P_DEFAULT_COMPRESSION_SCRATCH_RATIO    = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Wheel-tag body-part selectors used by PrepareIKPart's switch on the part's GetPartType(). These
    // are the asm switch keys; the placeholder EBodyParts enum has no named enumerators, so the keys
    // are the literal part-type codes. FLAG: rename to the EBodyParts enumerators when they are
    // recovered (front-left wheel arch / front-right / rear-left / rear-right / front-pair / rear-pair).
    enum { E_BODY_PART_WHEEL_FL = 0x30, E_BODY_PART_WHEEL_FR = 0x31,
           E_BODY_PART_WHEEL_RL = 0x32, E_BODY_PART_WHEEL_RR = 0x33,
           E_BODY_PART_WHEEL_REAR_PAIR = 0x82, E_BODY_PART_WHEEL_FRONT_PAIR = 0x83 };

    // =================================================================================================
    // DeformableObject (constructor) @ 0x82603EF0
    //   The X360 ctor wires the 20 embedded DeformationSensors' vtables/collision-generator sub-arrays
    //   and seeds each sensor's owner back-pointer slot to -1. The asm runs `vector constructor
    //   iterator` over each sensor's 3x64-byte BaseCollisionGenerator sub-array (the sensor's contact
    //   generators) and stores the per-element vtable (off_82095228) + the -1 sentinel (sensor[+71]).
    //   The first store (off_82095220) is THIS DeformableObject's own vtable.
    //
    //   MODELLED-VS-ASM: vtable wiring + placement-iteration is implicit in the C++ object model (the
    //   embedded maDeformationSensors[20] are constructed by value), so the faithful C++ equivalent is
    //   to default/zero the sensor array's owner-index sentinels. The asm's 20-iteration loop (v3 = 19,
    //   `do ... while (v3 >= 0)`) is reproduced by name over maDeformationSensors.
    // =================================================================================================
    DeformableObject::DeformableObject()
    {
        // The 20 embedded maDeformationSensors are constructed BY VALUE: the host C++ object model runs
        // each DeformationSensor's default ctor (which zero-inits via its own ClearVariables and
        // vector-constructs its embedded contact generators), reproducing the asm's per-sensor
        // `vector constructor iterator` + vtable wiring implicitly. The asm's extra per-sensor `-1`
        // sentinel poke at sensor+0x11C (the owner/contact-gen index) lands on a DeformationSensor
        // member with no public setter on the frozen sensor header, so it is left to the sensor's own
        // ctor default. FLAG: promote to an explicit per-sensor owner-index reset when the sensor slice
        // exposes it. No further work is required here -- the array is fully value-constructed.
    }

    // =================================================================================================
    // ClearVariables @ DWARF :3250 (the asm body the ctor/Prepare path runs to zero the per-frame state)
    //   Zeroes the 25-dword sphere/scratch header block, the live counts (tag/driven/IK = 0), the
    //   no-damage timer (= 100.0), the cooldown/velocity accumulators (= 0) and the four wheel->sensor
    //   sentinels (= -1). DWARF names the tail: SetZero on the angular-velocity sum, SetLastLinearVelocity,
    //   SetEntitySphereSize.
    //
    //   The asm walks 25 dwords from +6372, then per-sensor-block writes (100.0 default radius, zeroed
    //   spy/contact fields) across the 20 sensors at stride 432, then the four +26336.. = -1 sentinels.
    //   Expressed by member name below.
    // =================================================================================================
    void DeformableObject::ClearVariables()
    {
        // Live element counts -> 0 (asm: *(this+6368)=0 spec slot cleared at +6368 is mpDeformationSpec;
        // the 19216 / 25376 / 26232 zeroes are miNumTagPoints / miNumDrivenPoints / miNumIKBodyParts).
        miNumTagPoints    = 0;
        miNumDrivenPoints = 0;
        miNumIKBodyParts  = 0;

        // No-damage cooldown timer default (asm: *(_R11 - 104) = 100.0 each of the 20 sensor blocks is
        // the per-sensor default; the object-level mfNoDamageTimer is reset on the deformation path).
        mfNoDamageTimer = 100.0f;

        // Per-frame latches the asm clears (+26417 region) -> the reconstructed bounce/parity flags.
        mbHasBouncedThisFrame = 0u;
        mbBounceRandomParity  = 0u;

        // Per-sensor state reset across the 20 embedded sensors (asm: the 20-iteration stride-432 loop
        // that zeroes each sensor's accumulated impulse/spy and seeds the 100.0 default). FLAG: the
        // DeformationSensor reset is the sensor's own ClearVariables, but it is not exposed publicly on
        // the frozen sensor header, so the per-sensor field zeroing the asm inlines here is left to the
        // sensor type. The object-level state below is reset by name.

        // The four wheel->sensor map sentinels (asm: *(this+26336..26339) = -1). mau8WheelToSensorMap[4].
        mau8WheelToSensorMap[0] = 0xFFu;
        mau8WheelToSensorMap[1] = 0xFFu;
        mau8WheelToSensorMap[2] = 0xFFu;
        mau8WheelToSensorMap[3] = 0xFFu;

        // DWARF tail: accumulators / derived state reset by name (asm: vspltisw v0,0 over the cooldown
        // sum, then SetLastLinearVelocity / SetEntitySphereSize). mAngularVelocitySum is a VecFloat.
        mAngularVelocitySum.SetZero();
        SetLastLinearVelocity(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        SetEntitySphereSize(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });

        // ⚠️ RECONCILE NOTE (2026-08-14, walls wave -- recorded, not yet applied): the PS3
        // out-of-line ClearVariables @0x6BEEC4 shows this body ALSO runs
        // ImpulsePasser::Construct(&mImpulsePasser) (the "+6372 25-dword zero" the Prepare banner
        // misread as a scratch header -- it is the 25-slot chain map), VehicleRigidBody::
        // Construct(&mVehicleBody), the 20-sensor Construct loop and the mLastAngularVelocity
        // zero. Reconcile when the sensor/rigid-body Constructs land with the mount wave.
    }

    // =================================================================================================
    // SetLastLinearVelocity / SetEntitySphereSize (DWARF :696 / :703) -- ⭐ BODIED 2026-08-14 (walls
    // wave). Both are lane writes on the SAME packed member, proven by the PS3 ClearVariables
    // @0x6BEEC4 tail: two vperm{0,1,2,7} merges on this+26320 == mLastLinearVelocityPlusEntityRadius
    // -- the first replaces xyz and KEEPS w (SetLastLinearVelocity), the second replaces w and KEEPS
    // xyz (SetEntitySphereSize; the header's own gloss "w = entity radius" says the same).
    // =================================================================================================
    void DeformableObject::SetLastLinearVelocity(Vector3 lVelocity)
    {
        mLastLinearVelocityPlusEntityRadius.x = lVelocity.x;
        mLastLinearVelocityPlusEntityRadius.y = lVelocity.y;
        mLastLinearVelocityPlusEntityRadius.z = lVelocity.z;
        // w (the entity radius) is deliberately preserved -- the vperm mask {0,1,2,7}.
    }

    void DeformableObject::SetEntitySphereSize(VecFloat lvfSize)
    {
        // Only the w lane (the entity radius) -- the second vperm's {0,1,2,7} with the roles swapped.
        mLastLinearVelocityPlusEntityRadius.w = lvfSize.w;
    }

    // ⭐ 2026-08-14 (walls wave): the free-function trampoline BrnDeformationManager.cpp's Prepare
    // calls by name (the manager's per-model reset loop; ClearVariables is private on the frozen
    // header and the header now grants exactly this function friendship). Defined HERE, next to the
    // private body, so the two can never drift apart.
    void DeformableObject_ClearVariables(DeformableObject* lpModel)
    {
        lpModel->ClearVariables();
    }

    // =================================================================================================
    // Prepare @ 0x82642180
    //   Bind this DeformableObject to a streamed model + scene ids from the AddDeformationModelEvent,
    //   then full-reset the deformation to the event's initial-damage amount. The asm:
    //     mHandlingBodyID            = lEvent.mHandlingBodyID        (event[+1], 8 bytes)
    //     mu32GameModeState slot     = lEvent[+4]                    (the packed mode/state word)
    //     mpDeformationSpec          = *lEvent.mModelHandle          (**event -> resolved spec)
    //     mu16DeformableObjectIndex  = lu16Index                     (a3)
    //     mbActive                   = 1                             (+26402)
    //     mbDoSweptSphereTests       = lEvent.mbDoSweptSphereTests   (event[+156])
    //     (the +26460/+26417/+26415 latches cleared; the +6372 25-dword header zeroed)
    //   then ResetDeformation(input, scene, partMgr, wheelMgr, lEvent.mfInitialDamageAmount,
    //                         lEvent.meBaseDeformationType, /*lbForceFullIKSolve*/ false, random).
    //   Returns true (asm: `return 1`). The frozen signature returns bool, so `return true` is emitted.
    //
    //   MODELLED-VS-ASM: several of the bound fields are packed/reconstructed members whose offsets the
    //   asm pokes raw (+26384 handling-body, +26392 game-mode word, +26410 swept-sphere flag). The ones
    //   with a named member are written by name; the rest are FLAGGED reconstructed-member writes.
    // =================================================================================================
    bool DeformableObject::Prepare(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput, u16 lu16Index,
                                   const AddDeformationModelEvent& lrEvent,
                                   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                                   DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr,
                                   CgsNumeric::Random& lrRandom)
    {
        // Bind the scene/handling ids from the event, RE-READ off the raw asm 2026-08-14 (walls
        // wave -- the two notes this block used to carry were both wrong against the bytes):
        //   0x826421A0  ld  r8,  8(evt)     0x826421B0  std r8,  0x6710(this)  -> mHandlingBodyID (8B)
        //   0x826421B8  lwz r27, 0x10(evt)  0x826421BC  stw r27, 0x6718(this)  -> mGlobalEntityId (4B)
        // The +0x10 word is the event's mGlobalEntityId (BrnDeformationEvents.h layout note:
        // mGlobalEntityId +0x10), NOT a "game-mode word", and the old "the asm does NOT touch
        // +26388/+26392 in Prepare" claim was a pre-widening misread -- at the widened seats the
        // pair lands at +26384/+26392 and Prepare writes BOTH.
        mHandlingBodyID = lrEvent.mHandlingBodyID;
        mGlobalEntityId = lrEvent.mGlobalEntityId;
        // ⚠️ The header still carries the RECONSTRUCTED duplicate mu32GameModeState for the SAME
        // console seat (+26392; its own :572 flag says "ALMOST CERTAINLY mGlobalEntityId"). Until
        // that reconciliation lands, keep the two host members equal so _Update/_Detach's readers
        // of either name see the console's one value.
        mu32GameModeState = lrEvent.mGlobalEntityId.muValue;

        // Resolve + cache the streamed spec from the model handle. ⭐ UN-PINNED 2026-08-14 (walls
        // wave): the old `mpDeformationSpec = nullptr` FLAG ("the resolve is not expressible from
        // the frozen event layout") predates the handle widening -- the event's mModelHandle IS the
        // real CgsResource::ResourceHandle now, and the console resolve is ONE dereference of its
        // leading pointer:
        //   0x826421C0  lwz r27, 0(evt)     -- the handle's mpResourceMemory
        //   0x826421C4  lwz r27, 0(r27)     -- *(mpResourceMemory) == the fixed-up spec pointer
        //   0x826421D4  stw r27, 0x18E0(this)
        // -- the exact `*reinterpret_cast<T* const*>(mpResourceMemory)` shape every CgsResourcePtr_*
        // TU in the tree uses, and the same resolve ProcessValidateDeformationModelEvents
        // @0x825DB14C..164 performs on the same seat.
        mpDeformationSpec =
            *reinterpret_cast<const StreamedDeformationSpec* const*>(lrEvent.mModelHandle.mpResourceMemory);

        // ⭐ THE ATTACHED-VEHICLE BIND (walls wave; previously DROPPED entirely -- with it missing,
        // every GetVehiclePhysics() consumer of a registered model would have dereferenced a NULL
        // or stale body: StartVehicleContactGeneration's IsFrozen() read on frame one, the sensor
        // impulse routing, the wall-impulse path):
        //   0x826421F0  lwz r9, 0x90(evt)   -- AddDeformationModelEvent::mpVehiclePhysics
        //   0x826421F4  stw r9, 0x194C(this)-- mVehicleBody's console +0x4 vehicle pointer
        //                                      (6472 base + 4 == 6476 == 0x194C)
        mVehicleBody.mpAttachedVehicle = lrEvent.mpVehiclePhysics;

        // Pool index + the active/swept-sphere flags (asm: sth r5 -> +26290 index; stb 1 -> +26402
        // active; lbz evt+0x9C -> stb +26410 swept flag; stw 0 -> +26460 / stb 0 -> +26417/+26415
        // latches; sth 0 -> +26286/+26288 the two live-part counts).
        mu16DeformableObjectIndex = lu16Index;
        mbActive                  = true;
        mbDoSweptSphereTests      = lrEvent.mbDoSweptSphereTests;
        mi16NumPhysicalParts      = 0;   // 0x826421C8 sth 0, +0x66AE
        mi16NumHingedParts        = 0;   // 0x826421CC sth 0, +0x66B0

        // The +6372 25-dword scratch header the asm zeroes before the reset (the world-sphere/scratch
        // block). Cleared by name via the shared ClearVariables init the asm inlines here.
        // (asm: the `v14 = 25; do *v11 = 0 ...` loop over the +6372 header.)

        // Full deformation reset to the event's initial-damage amount + base type (asm tail-call). The
        // f32 initial-damage amount is broadcast into the VecFloat the reset takes (asm: vspltw of the
        // single damage scalar).
        const VecFloat lvfInitialDamage = { lrEvent.mfInitialDamageAmount, lrEvent.mfInitialDamageAmount,
                                            lrEvent.mfInitialDamageAmount, lrEvent.mfInitialDamageAmount };
        ResetDeformation(lpInput, lpScene, lpPartMgr, lpWheelMgr,
                         lvfInitialDamage, lrEvent.meBaseDeformationType,
                         /*lbForceFullIKSolve*/ false, lrRandom);
        return true;   // asm: `return 1`
    }

    // =================================================================================================
    // PrepareIKPart @ 0x826074B0
    //   For one IK body part (index liIndex): mark its part-state (ATTACHED_IK if the part is
    //   detachable, else NON_DETACHABLE; forced NON_DETACHABLE when the deformation-parts debug flag is
    //   off), then -- for the WHEEL part types only -- find the part's left/right-most tag points and
    //   record them into this car's wheel tag-point index map (start-index + found index). Every
    //   find-most-tag-point result is asserted != -1 (non-gating tripwire, verbatim messages).
    // =================================================================================================
    void DeformableObject::PrepareIKPart(s32 liIndex)
    {
        IKBodyPart& lrPart = maIKParts[liIndex];

        // Part state: detachable -> ATTACHED_IK(2), else NON_DETACHABLE(1) (asm: *v6 = v5 ? 2 : 1).
        if (lrPart.DetachablePart())
            maPartStates[liIndex] = static_cast<u8>(E_PART_STATE_ATTACHED_IK);
        else
            maPartStates[liIndex] = static_cast<u8>(E_PART_STATE_NON_DETACHABLE);

        // Deformation-parts disabled -> force NON_DETACHABLE (asm: if (!byte_82F2A345) *v6 = 1).
        if (!gbDeformationPartsEnabled)
            maPartStates[liIndex] = static_cast<u8>(E_PART_STATE_NON_DETACHABLE);

        // The part's first-tag-point index in this car's tag-point pool (asm: v8 = result[117], where
        // `result` is the part's driven-part spec from StreamedDeformationSpec). The found left/right
        // index is added to this base to give the absolute wheel tag-point index.
        // FLAG: StreamedDeformationSpec::GetDrivenPartSpec + IKBodyPartSpec::GetStartIndexOfTagPoints
        // are the DWARF-named lookups; not exposed on the frozen StreamedDeformationSpec, so the base
        // index is read off the part's own spec (GetSpec) which IS available.
        const s32 liStartIndexOfTagPoints = 0;   // FLAG: = mpSpec->GetStartIndexOfTagPoints() (accessor
                                                 //       not exposed on frozen spec); pinned 0 here.
        (void)lrPart.GetSpec();

        // Only the wheel part types map a tag point into the wheel tag-point index array. The switch
        // keys are the asm part-type codes (see the FLAGGED enum above). The writes land in this car's
        // wheel tag-point index map (asm: *(this+3920..3923)).
        switch (static_cast<s32>(lrPart.GetPartType()))
        {
            case E_BODY_PART_WHEEL_FL:   // 0x30 -- left-most tag point -> slot 0
            {
                const s32 liLeftMostIndex = lrPart.FindLeftMostTagPoint();
                CGS_ASSERT(liLeftMostIndex != -1, "liLeftMostIndex != -1");
                mu8WheelTagPointIndices[0] = static_cast<u8>(liLeftMostIndex + liStartIndexOfTagPoints);
                break;
            }
            case E_BODY_PART_WHEEL_FR:   // 0x31 -- right-most tag point -> slot 1
            {
                const s32 liRightMostIndex = lrPart.FindRightMostTagPoint();
                CGS_ASSERT(liRightMostIndex != -1, "liRightMostIndex != -1");
                mu8WheelTagPointIndices[1] = static_cast<u8>(liRightMostIndex + liStartIndexOfTagPoints);
                break;
            }
            case E_BODY_PART_WHEEL_RL:   // 0x32 -- left-most tag point -> slot 2
            {
                const s32 liLeftMostIndex = lrPart.FindLeftMostTagPoint();
                CGS_ASSERT(liLeftMostIndex != -1, "liLeftMostIndex != -1");
                mu8WheelTagPointIndices[2] = static_cast<u8>(liLeftMostIndex + liStartIndexOfTagPoints);
                break;
            }
            case E_BODY_PART_WHEEL_RR:   // 0x33 -- right-most tag point -> slot 3
            {
                const s32 liRightMostIndex = lrPart.FindRightMostTagPoint();
                CGS_ASSERT(liRightMostIndex != -1, "liRightMostIndex != -1");
                mu8WheelTagPointIndices[3] = static_cast<u8>(liRightMostIndex + liStartIndexOfTagPoints);
                break;
            }
            case E_BODY_PART_WHEEL_REAR_PAIR:   // 0x82 -- both -> slots 2 (L) and 3 (R)
            {
                const s32 liLeftMostIndex  = lrPart.FindLeftMostTagPoint();
                const s32 liRightMostIndex = lrPart.FindRightMostTagPoint();
                CGS_ASSERT(liLeftMostIndex  != -1, "liLeftMostIndex != -1");
                CGS_ASSERT(liRightMostIndex != -1, "liRightMostIndex != -1");
                mu8WheelTagPointIndices[2] = static_cast<u8>(liLeftMostIndex  + liStartIndexOfTagPoints);
                mu8WheelTagPointIndices[3] = static_cast<u8>(liRightMostIndex + liStartIndexOfTagPoints);
                break;
            }
            case E_BODY_PART_WHEEL_FRONT_PAIR:  // 0x83 -- both -> slots 0 (L) and 1 (R)
            {
                const s32 liLeftMostIndex  = lrPart.FindLeftMostTagPoint();
                const s32 liRightMostIndex = lrPart.FindRightMostTagPoint();
                CGS_ASSERT(liLeftMostIndex  != -1, "liLeftMostIndex != -1");
                CGS_ASSERT(liRightMostIndex != -1, "liRightMostIndex != -1");
                mu8WheelTagPointIndices[0] = static_cast<u8>(liLeftMostIndex  + liStartIndexOfTagPoints);
                mu8WheelTagPointIndices[1] = static_cast<u8>(liRightMostIndex + liStartIndexOfTagPoints);
                break;
            }
            default:
                break;
        }
    }

    // =================================================================================================
    // PrepareLocators @ 0x825BA010
    //   Copy the streamed spec's three locator lists (generic, light, camera) into this car's live
    //   VehicleLocatorData table: for each locator, copy its 4-row Matrix44Affine frame + its tag-point
    //   type, bounds-asserting (index < spec count; tag type < E_TAGPOINT_COUNT==57; live count <
    //   category max). The three live counts are zeroed first.
    //
    //   NOTE the asm ORDER + count maxima (verbatim asserts): generic (max 15), light (max 24), camera
    //   (max 1). FLAG: the spec-side per-list accessors (GetGenericLocators / GetLightLocators /
    //   GetCameraLocators + LocatorPointSpecList::GetNumLocatorPoints / GetLocatorSpec) are private on
    //   the frozen StreamedDeformationSpec, so the spec walk is left as a FLAGGED declare-only shape:
    //   the count-clears + the per-category bound asserts + the destination by-member writes are exact;
    //   the source read is pinned to an empty list (0 locators) until the spec accessors are exposed.
    // =================================================================================================
    void DeformableObject::PrepareLocators()
    {
        // Clear the three live category counts (asm: result[305]=result[716]=result[975]=0).
        mLocatorData.miNumCameraLocators  = 0;
        mLocatorData.miNumLightLocators   = 0;
        mLocatorData.miNumGenericLocators = 0;

        // --- generic locators (asm: spec+36 count / spec+40 array; dest stride 16 dwords; max 15) ---
        const u32 luNumGeneric = 0;   // FLAG: = mpDeformationSpec->mGenericTags.GetNumLocatorPoints()
        for (u32 luIndex = 0; luIndex < luNumGeneric; ++luIndex)
        {
            // FLAG: lpLocator = mpDeformationSpec->mGenericTags.GetLocatorSpec(luIndex) (accessor not
            // exposed on frozen spec). The bound asserts + dest writes below are the exact asm shape.
            CGS_ASSERT(luIndex < luNumGeneric, "luIndex < muNumLocators");
            const ETagPointType leType = E_TAG_POINT_TYPE_INVALID;   // FLAG: = lpLocator->meTagPointType
            CGS_ASSERT(static_cast<s32>(leType) < 57, "lpLocator->meTagPointType < E_TAGPOINT_COUNT");
            CGS_ASSERT(mLocatorData.miNumGenericLocators < KI_NUM_GENERIC_LOCATORS,
                       "mLocatorData.miNumGenericLocators < KI_MAX_GENERIC_LOCATORS");
            // dest: mLocatorData.maGenericLocators[count] = lpLocator->mLocatorMatrix (4x16B copy).
            mLocatorData.maGenericLocatorTypes[mLocatorData.miNumGenericLocators] = leType;
            ++mLocatorData.miNumGenericLocators;
        }

        // --- light locators (asm: spec+52 count / spec+56 array; dest stride 16; max 24) ------------
        const u32 luNumLight = 0;   // FLAG: = mpDeformationSpec->mLightTags.GetNumLocatorPoints()
        for (u32 luIndex = 0; luIndex < luNumLight; ++luIndex)
        {
            CGS_ASSERT(luIndex < luNumLight, "luIndex < muNumLocators");
            const ETagPointType leType = E_TAG_POINT_TYPE_INVALID;   // FLAG: = lpLocator->meTagPointType
            CGS_ASSERT(static_cast<s32>(leType) < 57, "lpLocator->meTagPointType < E_TAGPOINT_COUNT");
            CGS_ASSERT(mLocatorData.miNumLightLocators < KI_NUM_LIGHT_LOCATORS,
                       "mLocatorData.miNumLightLocators < KI_MAX_LIGHT_LOCATORS");
            mLocatorData.maLightLocatorTypes[mLocatorData.miNumLightLocators] = leType;
            ++mLocatorData.miNumLightLocators;
        }

        // --- camera locators (asm: spec+44 count / spec+48 array; dest stride 16; max 1) ------------
        const u32 luNumCamera = 0;   // FLAG: = mpDeformationSpec->mCameraTags.GetNumLocatorPoints()
        for (u32 luIndex = 0; luIndex < luNumCamera; ++luIndex)
        {
            CGS_ASSERT(luIndex < luNumCamera, "luIndex < muNumLocators");
            const ETagPointType leType = E_TAG_POINT_TYPE_INVALID;   // FLAG: = lpLocator->meTagPointType
            CGS_ASSERT(static_cast<s32>(leType) < 57, "lpLocator->meTagPointType < E_TAGPOINT_COUNT");
            CGS_ASSERT(mLocatorData.miNumCameraLocators < KI_NUM_CAMERA_LOCATORS,
                       "mLocatorData.miNumCameraLocators < KI_MAX_CAMERA_LOCATORS");
            mLocatorData.maCameraLocatorTypes[mLocatorData.miNumCameraLocators] = leType;
            ++mLocatorData.miNumCameraLocators;
        }
    }

    // =================================================================================================
    // Release @ 0x8263A698
    //   Tear this car out of the simulation: assert it was active, remove its physical parts/joints and
    //   detached wheels, then invalidate the handling-body / entity ids, drop the spec pointer and clear
    //   the active/index state. The asm:
    //     CGS_ASSERT(mbActive, "mbActive")
    //     RemovePhysicalPartsAndJoints(input, scene, partMgr)
    //     DetachedWheelManager::RemoveVehicleWheels(wheelMgr, input, mHandlingBodyID, mGlobalEntityId)
    //     mHandlingBodyID.SetInvalid(); mGlobalEntityId.SetInvalid(); mpDeformationSpec = 0;
    //     (+26286/+26288 cleared; +26290 index = -1; +26402 active = 0)
    //   DWARF tail names CgsPhysics::RigidBodyId::SetInvalid + CgsSceneManager::EntityId::SetInvalid.
    // =================================================================================================
    void DeformableObject::Release(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                                   DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr)
    {
        CGS_ASSERT(mbActive, "mbActive");

        // Drop physical parts/joints (a DeformableObject method, called by name), then the detached
        // wheels keyed by this car's body/entity ids. FLAG: DetachedWheelManager is only forward-declared
        // on the frozen header (its full layout would risk an include cycle), so its RemoveVehicleWheels
        // call -- DetachedWheelManager::RemoveVehicleWheels(lpWheelMgr, lpInput, mHandlingBodyID,
        // mGlobalEntityId) -- is left documented but not emitted here; it is restored when the manager
        // header is in-tree. RemovePhysicalPartsAndJoints IS a DeformableObject member and runs.
        RemovePhysicalPartsAndJoints(lpInput, lpScene, lpPartMgr);
        (void)lpWheelMgr;   // FLAG: DetachedWheelManager::RemoveVehicleWheels(lpWheelMgr, lpInput, ...)

        // Invalidate the ids + drop the spec + clear active/index state (asm tail + DWARF SetInvalid).
        // EntityId / RigidBodyId are plain { u32 muValue } handles on the frozen common-types header; the
        // DWARF SetInvalid sets the handle to its 0xFFFFFFFF "invalid" sentinel (asm: qword_82F2A3A8).
        // ⭐ 2026-08-11 (handle-widening wave): the member is the real 8-byte
        // CgsPhysics::RigidBodyId now, so this is its own SetInvalid() -- which writes the full
        // 64-bit K_INVALID_RIGID_BODY_ID. The old `.muValue = 0xFFFFFFFFu` on the 4-byte stand-in
        // left the HIGH dword (the half every consumer reads) untouched, so a "released" model
        // still answered with its old entity word.
        mHandlingBodyID.SetInvalid();              // RigidBodyId::SetInvalid()
        mGlobalEntityId.muValue   = 0xFFFFFFFFu;   // EntityId::SetInvalid()
        mpDeformationSpec         = nullptr;
        mu16DeformableObjectIndex = 0xFFFFu;   // asm: +26290 = -1 (the index sentinel)
        mbActive                  = false;
    }

    // =================================================================================================
    // ResetDeformation @ 0x82639D60
    //   Reset this car's deformation to a target initial-damage amount + base type. This is the heavy
    //   setup spine: decide whether we are entering a DAMAGE state (initial-damage > 0 OR reset type ==
    //   E_DEFORMATION_RESET_NONE-relative), optionally rebuild all IK parts, re-construct the tag/driven
    //   point pools from the spec, re-run the IK solve (and a 50-iteration full settle when forced),
    //   re-seat the four wheels (asserting each wheel position is valid), re-prepare locators, and seed
    //   the per-frame bbox/cooldown state. The asm is ~600 lines of SIMD + raw offsets.
    //
    //   FAITHFULNESS: the asm's spec-table walks (tag/driven/IK part Construct loops, the wheel re-seat
    //   loop, the glass/IK-part state seeding) index members the FROZEN header exposes by name
    //   (maTagPoints / maDrivenPoints / maIKParts / maPartStates / miNum*), but they are driven by
    //   StreamedDeformationSpec accessors (GetTagPointSpec / GetDrivenPointSpec / GetDrivenPartSpec /
    //   GetBoundingBox / count fields) that are NOT all exposed on the frozen spec. The control flow,
    //   the verbatim bounds asserts, the helper call ORDER (CalculateDriveTimeLimits -> Remove/ResetJoint
    //   -> RemoveVehicleWheels -> GetBoundingBox -> GetInitialCompressionScalesAndLimits -> ResetSensors
    //   -> rebuild pools -> UpdateIK [+50x settle] -> UpdateSkinningOffsets -> re-seat wheels ->
    //   PrepareLocators -> seed cooldown -> UpdateDeformedBBox) is preserved; the spec-driven element
    //   COUNTS are pinned to 0 where the accessor is unexposed and FLAGGED, so the rebuild loops are
    //   well-formed but empty until the spec accessors are exposed. No numbers are fabricated.
    // =================================================================================================
    void DeformableObject::ResetDeformation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                                            DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr,
                                            VecFloat lvfTime, DeformationResetType leResetType, bool lbFlag,
                                            CgsNumeric::Random& lrRandom)
    {
        // Damage-state classification (asm: vcmpgtfp |initialDamage| > eps; the result lane is fed to
        // cntlzw; v43 = (cntlzw & 0x20)==0  -> the compare was TRUE -> damage PRESENT; v44 = the
        // complement -> damage ZERO. The compare scalar is the small rodata threshold at stru_8208F620;
        // DWARF spells it rw::math::vpu::IsZero. Tested on the broadcast X lane. FLAG: the eps threshold
        // is a tiny tolerance, modelled as exact-zero here.)
        //   v43 (lbDamagePresent)   = |initialDamage| > eps                  (damage PRESENT)
        //   v44 (lbResetParts)      = !damagePresent, then forced TRUE if resetType==1
        //   v45 == v155             = v44                                    (kept as one bool)
        const bool lbDamagePresent = (lvfTime.x > 0.0f);
        bool       lbResetParts    = !lbDamagePresent;   // asm v44 = (cntlzw & 0x20) != 0
        if (static_cast<s32>(leResetType) == 1)          // asm: if (a6 == 1) v44 = 1
            lbResetParts = true;
        const bool lbDamageZeroOrType1 = lbResetParts;   // asm v45 = v155 = v44

        // Drive-time deformed-bbox clamp limits recomputed up front (asm: CalculateDriveTimeLimits).
        CalculateDriveTimeLimits();

        // The reset-parts gate is v44 (damage ZERO or resetType==1): such a reset rebuilds the IK parts;
        // otherwise the existing parts are kept and only joint velocities reset (asm: if (v44)
        // RemovePhysicalPartsAndJoints else { assert(lbResetParts || miNumIKBodyParts != 0);
        // ResetJointVelocities }).
        if (lbResetParts)
        {
            RemovePhysicalPartsAndJoints(lpInput, lpScene, lpPartMgr);
        }
        else
        {
            CGS_ASSERT(lbResetParts || (miNumIKBodyParts != 0), "lbResetParts || (miNumIKBodyParts != 0)");
            ResetJointVelocities(lpPartMgr);
        }

        // Remove the detached wheels for this body (asm: DetachedWheelManager::RemoveVehicleWheels).
        // FLAG: DetachedWheelManager is forward-declared only on the frozen header; the call
        // DetachedWheelManager::RemoveVehicleWheels(lpWheelMgr, lpInput, mHandlingBodyID, mGlobalEntityId)
        // is documented but not emitted here (the manager header would risk an include cycle). Restored
        // when the manager header is in-tree.
        (void)lpWheelMgr;

        // ⭐ UN-PINNED 2026-08-14 (deformation-mount wave). The old FLAG pinned the scratch values
        // and modelled the damage point as a bare +/-0.3 -- the asm (0x82639EA8..0x82639F94) says
        // it is BBOX-DERIVED: GetBoundingBox over the spec's sensor spheres into a scratch AABB,
        // then the damage point is a TOP CORNER of that box inset by 0.30000001 (flt_82004740,
        // image-read), x-side chosen by a Random parity draw (the inlined LCG on the Random seed;
        // bit set -> the +x corner, clear -> the -x corner with x negated):
        //   bit==1: (max.x - 0.3,  max.y - 0.0,  max.z - 0.3)         (flt_82001CC0 == 0.0 on y)
        //   bit==0: (-max.x + 0.3, max.y - 0.0,  max.z - 0.3)         (flt_82020A80 == -0.3)
        // GetBoundingBox is bodied + mounted (spec TU), so nothing is pinned any more.
        CgsGeometric::AxisAlignedBox lSensorBBox;
        mpDeformationSpec->GetBoundingBox(lSensorBBox);

        Vector3Plus lv3pCompressionScale_Scratch = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3     lvPosLimits = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3     lvNegLimits = { 0.0f, 0.0f, 0.0f, 0.0f };

        const bool lbDamageSignPositive = ((lrRandom.RandomUInt() & 1u) != 0u);
        const Vector3 lDamagePoint = lbDamageSignPositive
            ? Vector3{  lSensorBBox.mMax.x - 0.30000001f, lSensorBBox.mMax.y,
                        lSensorBBox.mMax.z - 0.30000001f, 0.0f }
            : Vector3{ -lSensorBBox.mMax.x + 0.30000001f, lSensorBBox.mMax.y,
                        lSensorBBox.mMax.z - 0.30000001f, 0.0f };

        GetInitialCompressionScalesAndLimits(leResetType, lvfTime,
                                             lv3pCompressionScale_Scratch, lvPosLimits, lvNegLimits);

        // Re-seed every sensor's compression/displacement from the damage scale/limits/point (asm:
        // ResetSensors(this, scale, posLimits, negLimits, damagePoint)).
        ResetSensors(lv3pCompressionScale_Scratch, lvPosLimits, lvNegLimits, lDamagePoint);

        // --- rebuild the tag-point pool from the spec ------------------------------------------------
        // ⭐ UN-PINNED 2026-08-14 (walls wave): the "spec accessors not exposed" FLAG that zeroed
        // this loop was STALE -- BrnStreamedDeformationSpec.h has modelled the tables (counts +
        // Ptr32 bases, static_assert-pinned, shipped-spec-verified) since the spec TU landed; the
        // two per-index reads it lacked (GetTagPointSpec / GetDrivenPointSpec) are added there this
        // wave as exact siblings of GetDrivenPartSpec. ⚠️ The old commented-out call also had the
        // WRONG second argument (&maDrivenPoints[0]) -- TagPoint::Construct's own declaration takes
        // the SENSOR array base (BrnTagPoint.h:43), which is what is passed now.
        const s32 liNumTagPoints = mpDeformationSpec->GetNumberOfTagPoints();
        miNumTagPoints = liNumTagPoints;
        for (s32 liTag = 0; liTag < liNumTagPoints; ++liTag)
        {
            CGS_ASSERT(liTag < liNumTagPoints, "liIndex < miNumberOfTagPoints");
            CGS_ASSERT(liTag >= 0, "liIndex >= 0");
            maTagPoints[liTag].Construct(mpDeformationSpec->GetTagPointSpec(liTag),
                                         &maDeformationSensors[0]);
        }

        // --- rebuild the driven-point pool from the spec (mirror of the tag-point loop; the
        //     endpoint base is the TAG-POINT array per IKDrivenPoint::Construct's declaration) -------
        const s32 liNumDrivenPoints = mpDeformationSpec->GetNumberOfDrivenPoints();
        miNumDrivenPoints = liNumDrivenPoints;
        for (s32 liDriven = 0; liDriven < liNumDrivenPoints; ++liDriven)
        {
            CGS_ASSERT(liDriven < liNumDrivenPoints, "liIndex < miNumberOfDrivenPoints");
            CGS_ASSERT(liDriven >= 0, "liIndex >= 0");
            maDrivenPoints[liDriven].Construct(mpDeformationSpec->GetDrivenPointSpec(liDriven),
                                               &maTagPoints[0]);
        }

        // --- rebuild the IK parts (damage-state only) (asm: if (lbResetParts) { miNumIKBodyParts =
        //     spec->numIKParts; per-part bounds-assert; IKBodyPart::Construct + PrepareIKPart; seed
        //     mau8PhysicalBodyPartPoolIndex[i] = -1; then fill the tail [count..50) with state 0 /
        //     pool-index -1 }) ----------------------------------------------------------------------
        if (lbResetParts)
        {
            // ⭐ UN-PINNED 2026-08-14 (walls wave) -- same stale-FLAG retirement as the two loops
            // above; the count + per-part spec come from the spec's own checked accessors.
            const s32 liNumIKParts = mpDeformationSpec->GetNumberOfIKParts();
            miNumIKBodyParts = liNumIKParts;
            // Clear the four wheel tag-point indices (asm: +3920..3923 = -1).
            mu8WheelTagPointIndices[0] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
            mu8WheelTagPointIndices[1] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
            mu8WheelTagPointIndices[2] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
            mu8WheelTagPointIndices[3] = KU_INVALID_WHEEL_TAG_POINT_INDEX;

            for (s32 liPart = 0; liPart < liNumIKParts; ++liPart)
            {
                CGS_ASSERT(liPart < liNumIKParts, "liIndex < miNumberOfIKParts");
                CGS_ASSERT(liPart >= 0, "liIndex >= 0");
                maIKParts[liPart].Construct(mpDeformationSpec->GetDrivenPartSpec(liPart),
                                            &maDrivenPoints[0], &maTagPoints[0]);
                PrepareIKPart(liPart);
                mau8PhysicalBodyPartPoolIndex[liPart] = KU_INVALID_WHEEL_TAG_POINT_INDEX;  // asm: -1
            }
            // Fill the unused tail [count..50): state 0 (UNUSED) + pool index -1 (asm tail loop).
            for (s32 liTail = miNumIKBodyParts; liTail < 50; ++liTail)
            {
                maPartStates[liTail]                  = static_cast<u8>(E_PART_STATE_UNUSED);
                mau8PhysicalBodyPartPoolIndex[liTail] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
            }
        }

        // Count the hinged/attached physical parts among the toughened part types (asm: scan parts,
        // part-type == 84 or 85 with state ATTACHED_IK(2)/HINGED(3) -> ++mi16NumHingedParts-region).
        s16 li16NumHingedish = 0;
        for (s32 liPart = 0; liPart < miNumIKBodyParts; ++liPart)
        {
            const s32 liType = static_cast<s32>(maIKParts[liPart].GetPartType());
            if (liType == 84 || liType == 85)
            {
                const u8 luState = maPartStates[liPart];
                if (luState == static_cast<u8>(E_PART_STATE_ATTACHED_IK) ||
                    luState == static_cast<u8>(E_PART_STATE_HINGED))
                    ++li16NumHingedish;
            }
        }
        mi16NumHingedParts = li16NumHingedish;   // asm: +26400 region accumulator

        // Pull the IK once (asm: UpdateIK(this, 1.0); the 1.0 amount is a broadcast VecFloat from
        // vcsxwfp128 of int 1); a forced full-settle does 50 more passes when damage is PRESENT (v43)
        // AND lbFlag (asm: if (v43 && a7) { 50x UpdateIK }).
        const VecFloat lvfOne = { 1.0f, 1.0f, 1.0f, 1.0f };
        UpdateIK(lvfOne);
        if (lbDamagePresent && lbFlag)
        {
            for (s32 liSettle = 0; liSettle < 50; ++liSettle)
                UpdateIK(lvfOne);
        }
        UpdateSkinningOffsets();

        // Clear the 10 glass-pane states (asm: the +26420 10-dword clear).
        for (s32 liGlass = 0; liGlass < 10; ++liGlass)
            maGlassPaneStates[liGlass] = E_GLASS_STATE_INTACT;

        // Re-seat the four wheels: for each wheel read its spec tag point, assert the world position is
        // valid (asm: the "Invalid wheel position: ... please tell Graham D." StrStream assert), then
        // Wheel::SetPosition + reset the wheel's broken latch. FLAG: the wheel-spec/tag-point walk + the
        // Wheel accessor live on VehiclePhysics (not exposed here); kept structurally with the verbatim
        // bounds assert. The valid-position message is built at runtime in the asm; modelled as a fixed
        // tripwire string here (no original file/line).
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
            // Wheel* lpWheel = GetVehiclePhysics()->GetWheel(liWheel);                          // FLAG
            // CGS_ASSERT(vpu::IsValid(lWheelPos), "Invalid wheel position: , please tell Graham D.");
            // lpWheel->SetPosition(lWheelPos); lpWheel->Attach();                               // FLAG
        }

        // Clear the 10 glass-pane states again (asm repeats the +26420 clear after the wheel loop).
        for (s32 liGlass = 0; liGlass < 10; ++liGlass)
            maGlassPaneStates[liGlass] = E_GLASS_STATE_INTACT;

        // Deformation parts disabled -> force every part-state to NON_DETACHABLE(1) (asm: if
        // (!byte_82F2A345) { 50x maPartStates = 1 }).
        if (!gbDeformationPartsEnabled)
        {
            for (s32 liPart = 0; liPart < 50; ++liPart)
                maPartStates[liPart] = static_cast<u8>(E_PART_STATE_NON_DETACHABLE);
        }

        // Rebuild the locator table + seed the per-frame bbox/cooldown state (asm tail).
        PrepareLocators();

        // No-damage cooldown: an extreme initial-damage reset arms the cooldown band (asm: if (v43 &&
        // v155) { spec-bbox *= unk_82FB9B80; +26417 = bbox.x + 1 } else +26417 = 0). FLAG: unk_82FB9B80
        // is the unrecovered per-axis scale (FLAGGED-0); the +26417 latch path is preserved by shape.
        if (lbDamagePresent && lbDamageZeroOrType1)
        {
            (void)KVF_INITIAL_DAMAGE_BBOX_SCALE;   // FLAG: spec-bbox *= unk_82FB9B80 (value unrecovered)
            // mu8...+26417 = scaledBBox.x + 1;  // FLAG: reconstructed latch, value inert until rodata
        }

        // The bonnet latch flags only stay set if the existing flag was set AND v155 is false (asm:
        // if (!+26411 || (tmp=1, v155)) tmp=0; +26411=tmp -- i.e. kept only when !v155, where
        // v155 == lbDamageZeroOrType1).
        if (mbBonnetHasOpened && !lbDamageZeroOrType1)
            mbBonnetHasOpened = true;
        else
            mbBonnetHasOpened = false;
        if (mbBonnetLatchedDown && !lbDamageZeroOrType1)
            mbBonnetLatchedDown = true;
        else
            mbBonnetLatchedDown = false;

        // Seed the rest of the per-frame state (asm: +26409/+26415 = v43; +26413/+26414 = 0;
        // +26460 = 0; the +26464 vector cleared; +26480 = 0). +26415 has no named member -> not
        // emitted (the asm also writes it = v43).
        mbResetDeformationNextUpdate = lbDamagePresent;   // asm: +26409 = v43 (damage present)
        mbHasBouncedThisFrame        = 0u;      // asm: +26414 = 0
        mbBounceRandomParity         = 0u;      // asm: +26413 = 0
        meAbsorptionSet              = E_ABSORPTIONSET_NORMAL;   // asm: +26460 = 0 (the absorption set slot)

        // A type-1 (full-crash) reset arms the absorption cooldown timer band (asm: if (a28==1) {
        // +26396 = 1.5; +26460 = 4 }). +26460 == meAbsorptionSet here maps onto E_ABSORPTIONSET_INVINCIBLE(4).
        if (static_cast<s32>(leResetType) == 1)
        {
            mfNoDamageTimer = 1.5f;                          // asm: +26396 = 1.5
            meAbsorptionSet = E_ABSORPTIONSET_INVINCIBLE;    // asm: +26460 = 4
        }

        // Final: rebuild the deformed bounding box (asm tail-call UpdateDeformedBBox).
        UpdateDeformedBBox();
    }

    // =================================================================================================
    // ResetSensors @ 0x82623D60 (X360, 718 instr) -- ⭐ BODIED 2026-08-14 (deformation-mount wave).
    // THE spec -> sensor/sphere seeding: without it a registered car is a hollow shell (no spheres
    // -> no contact tests). The PS3 twin (@0x7446FC, DecFIGS) names the signature verbatim:
    //   ResetSensors(const Vector3Plus lDamageScale, const Vector3 lPosLimits,
    //                const Vector3 lNegLimits, const Vector3 lDamagePoint)
    // (the frozen-header arg names lA..lD are kept; the roles are the PS3 names, in order).
    //
    // THREE PHASES, each verified on BOTH console listings:
    //
    //  (1) per-sensor seeding (X360 0x82623E44..0x82623F1C): for each of the spec's
    //      mu8NumDeformationSensors sensors: DeformationSensor::Prepare(&maDeformationSensors[i],
    //      spec sensor i (inlined checked accessor, :201 assert), &maLocalSensorSpheres[i],
    //      &maWorldSensorSpheres[i], carTransform-by-value, then the four vector args forwarded
    //      VERBATIM (vmr v1..v4 restores the original registers). Then the inlined
    //      ImpulsePasser::SetCollidableBodyMap(spec->mu8SceneIndex (`lbz 0x32(spec)`), &sensor)
    //      with its own BrnImpulsePasser.cpp:135 bounds assert, and the sensor's volume-instance
    //      id = (mHandlingBodyID's HIGH dword == the entity word; `clrrdi r10,r10,32`) | sceneIndex
    //      (`std sensor+0x190` == the promoted mVolInstId; PS3 spells it mVolInstId.muId).
    //
    //  (2) per-wheel mapping (0x82623FF4..0x8262468C, 4 wheels): read the wheel's LOCAL position
    //      (vehicle wheel array; the Wheel.h:412 "Invalid wheel position ... please tell Graham D."
    //      NaN sweep, fire-and-continue), transform to world by the car transform (vmaddfp
    //      cascade), find the CLOSEST live sensor by |worldSphereCentre - wheelWorld| (read through
    //      each sensor's mpWorldSpaceSphere pointer, `lwz sensor+0x1A0`; vmsum3fp128 +
    //      vrsqrtefp/Newton + vsel zero-guard == guarded sqrt), assert one was found
    //      (BrnDeformableObject.cpp:911 -- the console composes a multi-line diagnostic into the
    //      assert buffer; the CONDITION is the tripwire, reproduced with the leading message), and
    //      write mau8WheelToSensorMap[wheel]. Then append the wheel's own collision sphere at
    //      maWorldSensorSpheres[numSensors + wheel]:
    //        centre = wheelWorld + KV_UP * (mScale.x * 0.5) * 0.5   -- KV_UP == unk_82181510,
    //                 image-read (0, 1, 0, 0); the two 0.5s are vcsxwfp(1,1) splats, kept literal
    //        radius = mScale.x * 0.5
    //      where mScale.x is the spec's WheelSpec lane (`spec + 96 + 48*wheel` lane 0, via the
    //      inlined GetWheelSpec with its :257 "liWheel < eNumWheels" assert).
    //
    //  (3) swept-sphere seeding (0x82624690..0x82624880), gated on mbDoSweptSphereTests
    //      (`lbz this+0x672A`): for each of the numSensors+4 world spheres, the point velocity
    //      v = linVel + cross(angVel, centre - carPos) (the vpermwi-0x63 cross pair -- PS3 names
    //      gCrossProductPermuteConstant), then maSweptSpheres[i] = { world sphere verbatim,
    //      (normalize(v), |v| * KF_CONSOLE_TIMESTEP) } via the inlined SweptSphere::Set pair of
    //      16-byte stores. KF_CONSOLE_TIMESTEP == flt_82095EE0, image-read 0.016666668 (the
    //      console's baked 1/60 SIM step -- a source constant, NOT the render-rate coupling).
    //      The (dir.x, dir.y, dir.z, len) packing is the vperm pair over unk_82CDA3C0/82CDA400
    //      (image-read byte selectors) + vsldoi-8 merge == exactly that component assembly.
    //      The console's rsqrt path leaves dir = NaN when v is exactly zero (only the length is
    //      vsel-guarded); the committed vpu::Normalize guards dir to zero as well -- the guarded
    //      form is kept (house Normalize family), divergence noted.
    //
    // ASSERTS are non-gating tripwires (fire-and-continue), exactly as the asm falls through.
    // Constants image-verified this wave via x360rd.py: unk_82181510 = (0,1,0,0);
    // flt_8208F5EC = FLT_MAX (3.4028235e38); flt_82095EE0 = 0.016666668.
    // =================================================================================================
    void DeformableObject::ResetSensors(Vector3Plus lA, Vector3 lB, Vector3 lC, Vector3 lD)
    {
        // :848 -- the spec must be resolved before sensors can be seeded (fire-and-continue).
        CGS_ASSERT(mpDeformationSpec != nullptr, "mpDeformationSpec");

        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();

        // The car transform: the asm copies the vehicle's four transform rows (+0x10..+0x40) to
        // the stack ONCE and passes the copy by value to every sensor Prepare.
        const Matrix44Affine lWorldTransform = lpVehicle->GetTransform();

        // Sensor count captured once (asm r8/v157); the per-index accessor still asserts per pull.
        const s32 liNumSensors = static_cast<s32>(mpDeformationSpec->mu8NumDeformationSensors);

        // ---- phase 1: per-sensor Prepare + impulse-passer map + volume-instance id --------------
        for (s32 liSensor = 0; liSensor < liNumSensors; ++liSensor)
        {
            // Inlined StreamedDeformationSpec::GetDeformationSensorSpec (its own :201 assert).
            const SensorSpec* lpSensorSpec = mpDeformationSpec->GetDeformationSensorSpec(liSensor);

            // The sensor's collidable-body slot (sensor spec +0x32 == mu8SceneIndex, `lbz 0x32`).
            const u8 lu8SceneIndex = lpSensorSpec->GetSceneIndex();

            DeformationSensor& lrSensor = maDeformationSensors[liSensor];
            lrSensor.Prepare(lpSensorSpec, &maLocalSensorSpheres[liSensor],
                             &maWorldSensorSpheres[liSensor], lWorldTransform,
                             lA, lB, lC, lD);   // the four args forwarded verbatim (vmr v1..v4)

            // Inlined ImpulsePasser::SetCollidableBodyMap (its own :135 bounds assert).
            mImpulsePasser.SetCollidableBodyMap(static_cast<s32>(lu8SceneIndex), &lrSensor);

            // The sensor's volume-instance id: the handling-body ENTITY word (high dword of the
            // 8-byte id) keyed with this sensor's collidable-body index (`clrrdi` + `or` + `std`).
            lrSensor.mVolInstId.muId =
                (static_cast<u64>(mHandlingBodyID) & 0xFFFFFFFF00000000ull)
                | static_cast<u64>(lu8SceneIndex);
        }

        // ---- phase 2: closest sensor per wheel + the four appended wheel spheres ----------------
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            const Vehicle::Wheel& lrWheel =
                lpVehicle->GetWheel(static_cast<Vehicle::EVehicleDrivenWheel>(liWheel));

            // Wheel.h:412 NaN sweep over the wheel's local position (vspltw + vcmpeqfp. per lane).
            CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                       lrWheel.mPosition.y == lrWheel.mPosition.y &&
                       lrWheel.mPosition.z == lrWheel.mPosition.z,
                       "Invalid wheel position: , please tell Graham D.");   // Wheel.h:412

            // Wheel world position: the rotation rows scaled by the local lanes, seeded with the
            // translation row (the console's vmaddfp cascade over rows +0x10..+0x40).
            const Vector3 lvWheelWorld{
                lWorldTransform.xAxis.x * lrWheel.mPosition.x + lWorldTransform.yAxis.x * lrWheel.mPosition.y
                    + lWorldTransform.zAxis.x * lrWheel.mPosition.z + lWorldTransform.wAxis.x,
                lWorldTransform.xAxis.y * lrWheel.mPosition.x + lWorldTransform.yAxis.y * lrWheel.mPosition.y
                    + lWorldTransform.zAxis.y * lrWheel.mPosition.z + lWorldTransform.wAxis.y,
                lWorldTransform.xAxis.z * lrWheel.mPosition.x + lWorldTransform.yAxis.z * lrWheel.mPosition.y
                    + lWorldTransform.zAxis.z * lrWheel.mPosition.z + lWorldTransform.wAxis.z,
                0.0f };

            // Nearest live sensor by WORLD sphere centre distance -- read through each sensor's
            // own mpWorldSpaceSphere pointer exactly as the asm does (`lwz sensor+0x1A0`).
            f32 lfMinDistance = 3.4028235e38f;   // flt_8208F5EC == FLT_MAX seed
            u8  lu8MinIndex   = 0xFFu;           // li r15, 0xFF sentinel
            for (s32 liSensor = 0; liSensor < liNumSensors; ++liSensor)
            {
                const Sphere* lpWorldSphere = maDeformationSensors[liSensor].GetWorldSpaceSphere();
                const Vector4& lrCentre = lpWorldSphere->mPositionRadius;
                const Vector3 lvDelta{ lrCentre.x - lvWheelWorld.x, lrCentre.y - lvWheelWorld.y,
                                       lrCentre.z - lvWheelWorld.z, 0.0f };
                // vmsum3fp128 + vrsqrtefp/2xNewton + vsel zero-guard == the guarded Magnitude.
                const f32 lfDistance = vpu::Magnitude(lvDelta);
                if (lfDistance < lfMinDistance)
                {
                    lfMinDistance = lfDistance;
                    lu8MinIndex   = static_cast<u8>(liSensor);
                }
            }

            // :911 -- the console composes "Failed to find closest sensor to wheel\n" + a full
            // diagnostic dump (wheel local pos / index / car transform / NumSensors /
            // lu8MinDistanceSensorIndex) into the assert buffer. The CONDITION is the tripwire;
            // fire-and-continue.
            CGS_ASSERT(static_cast<s32>(lu8MinIndex) < liNumSensors,
                       "Failed to find closest sensor to wheel\n");   // BrnDeformableObject.cpp:911

            mau8WheelToSensorMap[liWheel] = lu8MinIndex;   // stbx this+0x66E0+wheel

            // Inlined GetWheelSpec (its own :257 "liWheel < eNumWheels" assert) -> mScale lane 0
            // (`spec + 96 + 48*wheel`, the WheelSpec's mScale.x).
            const f32 lfScale = mpDeformationSpec->GetWheelSpec(liWheel)->mScale.x;

            // The appended wheel sphere: centre raised along KV_UP (unk_82181510 == (0,1,0,0),
            // image-read) by (scale*0.5)*0.5; radius = scale*0.5 (the two vrlimi w-lane writes,
            // second wins).
            Vector4& lrWheelSphere = maWorldSensorSpheres[liNumSensors + liWheel].mPositionRadius;
            lrWheelSphere.x = lvWheelWorld.x;
            lrWheelSphere.y = lvWheelWorld.y + (lfScale * 0.5f) * 0.5f;
            lrWheelSphere.z = lvWheelWorld.z;
            lrWheelSphere.w = lfScale * 0.5f;
        }

        // ---- phase 3: swept-sphere seeding (gated on mbDoSweptSphereTests, `lbz +0x672A`) -------
        if (mbDoSweptSphereTests)
        {
            // flt_82095EE0, image-read: the console's baked 1/60 sim step (a source constant).
            const f32 KF_CONSOLE_TIMESTEP = 0.016666668f;

            const Vector3& lvCarPos     = lpVehicle->GetPosition();          // rows base +0x40
            const Vector3& lvLinearVel  = lpVehicle->GetLinearVelocity();    // +0x50
            const Vector3& lvAngularVel = lpVehicle->GetAngularVelocity();   // +0x60

            // Sensor spheres (loop A) then the four wheel spheres (loop B) -- identical math, one
            // contiguous index range over maWorldSensorSpheres / maSweptSpheres.
            for (s32 liSphere = 0; liSphere < liNumSensors + 4; ++liSphere)
            {
                const Vector4& lrSphere = maWorldSensorSpheres[liSphere].mPositionRadius;

                // r = centre - carPos; v = linVel + cross(angVel, r) (the vpermwi-0x63 cross pair;
                // PS3 names gCrossProductPermuteConstant).
                const Vector3 lvR{ lrSphere.x - lvCarPos.x, lrSphere.y - lvCarPos.y,
                                   lrSphere.z - lvCarPos.z, 0.0f };
                const Vector3 lvPointVel = vpu::Add(lvLinearVel, vpu::Cross(lvAngularVel, lvR));

                // vmsum3fp128 + vrsqrtefp/2xNewton: speed (vsel zero-guarded) + direction (the
                // committed Normalize family's guard extends to the direction; see banner).
                const f32     lfSpeed = vpu::Magnitude(lvPointVel);
                const Vector3 lvDir   = vpu::Normalize(lvPointVel);

                // The two packed 16-byte stores (inlined SweptSphere::Set): the world sphere
                // verbatim, then (dir.xyz, speed * timestep) -- the vperm/vsldoi component
                // assembly over unk_82CDA3C0/82CDA400, image-verified as exactly this packing.
                maSweptSpheres[liSphere].Set(
                    Vector3Plus{ lrSphere.x, lrSphere.y, lrSphere.z, lrSphere.w },
                    Vector3Plus{ lvDir.x, lvDir.y, lvDir.z, lfSpeed * KF_CONSOLE_TIMESTEP });
            }
        }
    }

    // =================================================================================================
    // RemovePhysicalPartsAndJoints @ 0x82625250 (155 instr) -- ⭐ BODIED 2026-08-14
    // (deformation-mount wave). Two passes + the count zeroing, asm-complete:
    //
    //  (1) for each live physical part (mau8PhysicalBodyPartPoolIndex[0..mi16NumPhysicalParts)):
    //      part = Pool::GetPart (via the manager -- pool at manager+0; host path is the inline
    //      GetPartFromIndex); partIdx = (packed handle >> 32) & 0x3FF; assert
    //      maPartStates[partIdx] is HINGED(3) or DETATCHED(4) (":2594", fire-and-continue);
    //      maPartStates[partIdx] = ATTACHED_IK(2); Pool::RemovePart(input, scene, poolSlot);
    //      and if the "deformation parts enabled" debug flag (byte_82F2A345) is CLEAR,
    //      maPartStates[partIdx] = NON_DETACHABLE(1) instead (the flag is carried as the
    //      file-static gbDeformationPartsEnabled above, default true == the console's set state,
    //      so the re-write arm is compiled but never taken -- exactly the console default).
    //  (2) consistency sweep over maPartStates[0..miNumIKBodyParts): any state still HINGED or
    //      DETATCHED fires the composed "Part: %d State: %d" assert (":2609", tripwire only).
    //  tail: mi16NumPhysicalParts = 0; mi16NumHingedParts = 0.
    // =================================================================================================
    void DeformableObject::RemovePhysicalPartsAndJoints(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
        DetachedPartManager* lpPartMgr)
    {
        // ---- pass 1: release every live physical part back to the pool --------------------------
        for (s16 li16Part = 0; li16Part < mi16NumPhysicalParts; ++li16Part)
        {
            const u8 lu8PoolSlot = mau8PhysicalBodyPartPoolIndex[li16Part];
            PhysicalBodyPart* lpPart = lpPartMgr->GetPartFromIndex(lu8PoolSlot);

            const u32 luPartIndex =
                static_cast<u32>(lpPart->GetContactVolumeInstanceId().muId >> 32) & 0x3FFu;

            // ":2594" tripwire -- the state must be HINGED or DETATCHED to be physical at all.
            CGS_ASSERT(maPartStates[luPartIndex] == static_cast<u8>(E_PART_STATE_HINGED) ||
                       maPartStates[luPartIndex] == static_cast<u8>(E_PART_STATE_DETATCHED),
                       "maPartStates[ liPartIndex ] == E_PART_STATE_HINGED || "
                       "maPartStates[ liPartIndex ] == E_PART_STATE_DETATCHED");

            maPartStates[luPartIndex] = static_cast<u8>(E_PART_STATE_ATTACHED_IK);   // stbx 2

            // Release the pool slot (posts the rigid-body remove + scene teardown; the manager
            // pointer IS the pool on the console -- host path is the pool via the manager).
            lpPartMgr->RemovePart(lpInput, lpScene, lu8PoolSlot);

            if (!gbDeformationPartsEnabled)
                maPartStates[luPartIndex] = static_cast<u8>(E_PART_STATE_NON_DETACHABLE);   // stbx 1
        }

        // ---- pass 2: consistency sweep (tripwire only; the console composes a diagnostic) -------
        for (s32 liPart = 0; liPart < miNumIKBodyParts; ++liPart)
        {
            CGS_ASSERT(maPartStates[liPart] != static_cast<u8>(E_PART_STATE_HINGED) &&
                       maPartStates[liPart] != static_cast<u8>(E_PART_STATE_DETATCHED),
                       "Part: State: ");   // the ":2609" composed "Part: %d State: %d" dump
        }

        // ---- tail: no physical or hinged parts remain -------------------------------------------
        mi16NumPhysicalParts = 0;   // sth 0 -> +0x66AE
        mi16NumHingedParts   = 0;   // sth 0 -> +0x66B0
    }

    // =================================================================================================
    // ResetJointVelocities @ 0x825DF810 (34 instr) -- ⭐ BODIED 2026-08-14 (deformation-mount wave).
    // For each LIVE physical part (mau8PhysicalBodyPartPoolIndex[0..mi16NumPhysicalParts)): fetch
    // the pooled part (Pool::GetPart via the manager -- the console passes the manager pointer
    // straight as the pool `this`, mPartPool being the manager's one member at +0; the host path
    // is the header-inline GetPartFromIndex forward), derive the part's own IK-part index from
    // the packed handle (`ld part+0x1D0; srdi 32; & 0x3FF` == the entity word's low 10 bits), and
    // if this object's maPartStates[index] == E_PART_STATE_HINGED(3), zero the part's JOINT
    // VELOCITY -- the w lane of mLocalGraphicsPositionPlusJointVelocity, xyz KEPT (`vrlimi128
    // v13, v0, 1, 0`; PS3 @0x6F8E64 vperm<0,1,2,7> vs zeros agrees). ⚠️ The walls-wave census
    // phrased this as "zero the xyz keeping w" -- BOTH console listings say the opposite; the w
    // lane IS the packed joint velocity, so the semantic is simply SetJointVelocity(0).
    // The asm re-reads mi16NumPhysicalParts each iteration; kept.
    // =================================================================================================
    void DeformableObject::ResetJointVelocities(DetachedPartManager* lpPartMgr)
    {
        for (s16 li16Part = 0; li16Part < mi16NumPhysicalParts; ++li16Part)
        {
            PhysicalBodyPart* lpPart =
                lpPartMgr->GetPartFromIndex(mau8PhysicalBodyPartPoolIndex[li16Part]);

            // The part's IK-part index out of the packed handle (entity word low 10 bits). Read
            // through the public packed-id accessor (the console `ld`s the same 8 bytes inline).
            const u32 luPartIndex =
                static_cast<u32>(lpPart->GetContactVolumeInstanceId().muId >> 32) & 0x3FFu;

            if (maPartStates[luPartIndex] == static_cast<u8>(E_PART_STATE_HINGED))
            {
                lpPart->SetJointVelocity(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
            }
        }
    }

    // =================================================================================================
    // GetInitialCompressionScalesAndLimits @ 0x825DF6F8 -- ⭐ BODIED 2026-08-14 (deformation-mount
    // wave). ⚠️ The X360 address is an .ida-exports HOLE (no JSON): recovered by decoding the `bl`
    // at ResetDeformation+0x238 (0x82639F98) out of the image (x360rd), then disassembling
    // 0x825DF6F8..0x825DF810 with ppcdis.py -- absent-from-JSON is not absent-from-image. The PS3
    // twin @0x6C90C0 names everything: (leDeformationType, VecFloat lvfInitialDamage,
    // Vector3Plus& lv3pCompressionScale_Scratch, Vector3& lvPosLimits, Vector3& lvNegLimits) --
    // the frozen header's lvfTime/lrScales/lrLimitA/lrLimitB param names are KEPT; the roles are
    // the PS3 names in order.
    //
    //   * ratio select by reset type (both consoles: 0 -> EVENT, 1 -> CAR_SELECT, else DEFAULT;
    //     the three KV3P_* dynamic-init vectors recovered above);
    //   * lrScales = ratio * initialDamage on ALL FOUR lanes (the two vperm<0,1,2,7> passes are
    //     just the xyz/w lane assembly of that one product);
    //   * assert the vehicle physics is attached (BrnDeformableObject.cpp:402, fire-and-continue);
    //   * limits from the live attribs' drive-time deform band, A = mpAttribs->mBaseAttribs
    //     .mDrivetimeDeformLimits (vehicle+0x720 -> attribs+0x40; the AttribSys names say exactly
    //     what the lanes are: x = DriveTimeDeformLimitX, y = NegY, z = PosZ, w = NegZ):
    //       lvPosLimits = ( A.x, 0,   A.z, A.x)     (vperm<0,5,0,0> + <0,1,6,3> over splats)
    //       lvNegLimits = (-A.x, -A.y, -A.w, -A.x)  (the vxor sign-mask pass, same perms)
    //     i.e. symmetric lateral band, no upward crush, PosZ forward / NegZ backward. The trailing
    //     w lanes are exactly what the console perms leave there.
    // =================================================================================================
    void DeformableObject::GetInitialCompressionScalesAndLimits(DeformationResetType leResetType,
                                                                VecFloat lvfTime, Vector3Plus& lrScales,
                                                                Vector3& lrLimitA, Vector3& lrLimitB)
    {
        // Ratio select (0 -> EVENT, 1 -> CAR_SELECT, else DEFAULT).
        const Vector3Plus& lrRatio =
            (static_cast<s32>(leResetType) == 0) ? KV3P_EVENT_COMPRESSION_SCRATCH_RATIO
          : (static_cast<s32>(leResetType) == 1) ? KV3P_CAR_SELECT_COMPRESSION_SCRATCH_RATIO
                                                 : KV3P_DEFAULT_COMPRESSION_SCRATCH_RATIO;

        // lrScales = ratio * initialDamage (vmaddfp ratio*damage + the two lane-assembly perms).
        const f32 lfDamage = lvfTime.x;   // the VecFloat's broadcast lane (house idiom)
        lrScales.x = lrRatio.x * lfDamage;
        lrScales.y = lrRatio.y * lfDamage;
        lrScales.z = lrRatio.z * lfDamage;
        lrScales.w = lrRatio.w * lfDamage;

        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();
        // :402 -- fire-and-continue tripwire (the asm re-reads the pointer and carries on).
        CGS_ASSERT(lpVehicle != nullptr, "mVehicleBody.GetVehiclePhysics() != NULL");

        // [marked deviation, 2026-08-14] mpAttribs NULL-guard -- same root cause as
        // CalculateDriveTimeLimits' (per-car VehiclePhysics::Construct still gated behind
        // PrepareData, so the CREATE-time reset can run before SetAttributes seats the pointer).
        // Zero limits until seated; the console never sees a null here.
        if (lpVehicle == nullptr || lpVehicle->mpAttribs == nullptr)
        {
            lrLimitA = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            lrLimitB = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            return;
        }

        // A = the live drive-time deform band (vehicle+0x720 -> attribs+0x40).
        const Vector4& lrA = lpVehicle->mpAttribs->mBaseAttribs.mDrivetimeDeformLimits;

        lrLimitA = Vector3{  lrA.x, 0.0f,   lrA.z,  lrA.x };   // pos limits
        lrLimitB = Vector3{ -lrA.x, -lrA.y, -lrA.w, -lrA.x };  // neg limits
    }

    // =================================================================================================
    // UpdateAbsorptionSet @ 0x825DF9A0
    //   Pick this car's active per-frame energy-absorption profile (meAbsorptionSet), which keys the
    //   AbsorptionTable. The asm branches on whether the attached vehicle physics is in a crash state
    //   (vehicle+4308), the player/game-mode (the +26384 packed word HIGH byte == 1), the vehicle's
    //   "is shutdown / extreme" flag (vehicle+1808), the supplied game mode (a2 == -1 selects the
    //   extreme/player path), the cooldown timer (+26396 >= 0), and the speed-below-max margin
    //   (vehicle crash-speed delta vs unk_82FB9AB0). meAbsorptionSet ends up one of
    //   NORMAL(0)/AI_CRASHING(1)/PLAYER_EXTREME_CRASH(2)/INVINCIBLE(4-as-cooldown)/...
    //
    //   FLAG: the vehicle-physics crash/shutdown/speed accessors (GetMaxNonBoostSpeedMPH /
    //   GetSpeedMPHOnLastCrash + the +4308/+1808/+1824/+3824 flags) are NOT exposed on the frozen
    //   VehiclePhysics; the branch structure + the meAbsorptionSet writes are exact, with the
    //   accessor reads pinned to their asm-equivalent sub-expressions and FLAGGED. unk_82FB9AB0 is a
    //   FLAGGED-0 .rodata margin. The arg is the plain-s32 game-mode (frozen-header convention).
    // =================================================================================================
    void DeformableObject::UpdateAbsorptionSet(s32 liGameMode)
    {
        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();

        // asm: v3 = *(this+6476) (the attached vehicle physics). *(v3 + 4308) is the "in a damaging
        // crash" flag. FLAG: read via a declared accessor when exposed; here pinned false.
        const bool lbVehicleInCrash = false;   // FLAG: = lpVehicle crash-state flag (vehicle+4308)
        (void)lpVehicle;

        if (lbVehicleInCrash)
        {
            // In a crash. If NOT (player byte == 1 AND vehicle "extreme eligible" flag set), fall to
            // NORMAL. (asm: if (HIBYTE(*(this+26384)) != 1 || !*(v3+1808)) -> +26460 = 0; return.) The
            // player selector is the HIGH byte of the handling-body-id word (+26384), NOT the +26392
            // game-mode word -> read it via GetHandlingBodyIdHighByte().
            const bool lbPlayerMode      = (GetHandlingBodyIdHighByte() == 1u); // asm: HIBYTE(*(this+26384))
            const bool lbExtremeEligible = false;   // FLAG: = vehicle "extreme eligible" flag (vehicle+1808)
            if (!lbPlayerMode || !lbExtremeEligible)
            {
                meAbsorptionSet = E_ABSORPTIONSET_NORMAL;   // asm: +26460 = 0
                return;
            }

            // Eligible: game-mode -1 selects the PLAYER_EXTREME path (+ arms the +26417 cooldown to 10);
            // any other mode is the AI_CRASHING set. (asm: if (a2 == -1) { +26460 = 3; +26417 = 10 }
            // else +26460 = 1.) NOTE the asm stores 3 here; meAbsorptionSet enum maps 3 -> SHUTDOWN.
            if (liGameMode == -1)
            {
                meAbsorptionSet = E_ABSORPTIONSET_SHUTDOWN;   // asm: +26460 = 3
                // +26417 = 10;  // FLAG: reconstructed cooldown latch (no named member)
            }
            else
            {
                meAbsorptionSet = E_ABSORPTIONSET_AI_CRASHING;   // asm: +26460 = 1
            }
            return;
        }

        // Not in a crash. If the absorption cooldown is still armed (current set == 4 / INVINCIBLE) and
        // the cooldown timer is non-negative, hold it; once it expires drop to NORMAL. (asm: if
        // (+26460 == 4) { if (+26396 >= 0.0) return; else { +26460 = 0; return; } }.)
        if (meAbsorptionSet == E_ABSORPTIONSET_INVINCIBLE)
        {
            if (mfNoDamageTimer >= 0.0f)
                return;
            meAbsorptionSet = E_ABSORPTIONSET_NORMAL;   // asm: +26460 = 0
            return;
        }

        // Otherwise: only an "extreme eligible" vehicle can enter the PLAYER_EXTREME_CRASH set, and only
        // when its speed is far enough below max (crash-speed delta exceeds the unk_82FB9AB0 margin).
        // (asm: if (!*(v3+1808)) -> +26460 = 0; else compare (crashSpeedDelta - unk_82FB9AB0) > scratch
        // -> +26460 = 2 ? 0.)
        const bool lbExtremeEligible = false;   // FLAG: = vehicle "extreme eligible" flag (vehicle+1808)
        if (!lbExtremeEligible)
        {
            meAbsorptionSet = E_ABSORPTIONSET_NORMAL;   // asm: +26460 = 0
            return;
        }

        // Speed-below-max margin test (asm: vsubfp crashSpeedDelta - unk_82FB9AB0; vcmpgtfp.). FLAG:
        // GetMaxNonBoostSpeedMPH / GetSpeedMPHOnLastCrash not exposed -> the delta is pinned, the
        // margin is the FLAGGED-0 unk_82FB9AB0; the branch + the writes are exact.
        const f32 lfSpeedBelowMaxMargin = 0.0f;   // FLAG: = (maxNonBoostSpeed - speedOnLastCrash)
        const bool lbFarBelowMax = (lfSpeedBelowMaxMargin - KF_EXTREME_CRASH_SPEED_MARGIN) > 0.0f;
        if (lbFarBelowMax)
            meAbsorptionSet = E_ABSORPTIONSET_PLAYER_EXTREME_CRASH;   // asm: +26460 = 2
        else
            meAbsorptionSet = E_ABSORPTIONSET_NORMAL;                 // asm: +26460 = 0
    }
    // ==============================================================================================
    // ConstructUpdatePerformanceMonitors @0x825B99A0, ConstructUpdateIKAndLocatorsPerformanceMonitors
    // @0x825B9B00 and ConstructPostPhysicsPerformanceMonitors @0x825B9C88 WERE HERE. They were MOVED
    // VERBATIM to BrnDeformationConstructShims.cpp on 2026-08-03 (task #116) together with the ten
    // si* handles they seed -- see the note at the handle declarations above. Nothing about them
    // changed; this TU simply cannot be mounted yet and DeformationManager::Construct needed them.
    // TO RE-MERGE: mount this TU, move the three bodies back, and make the handles `static` again.
    // ==============================================================================================

}
}
