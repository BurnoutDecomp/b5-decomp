#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                    // CgsNumeric::Random
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::{...}

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
    static s32 siSortContactsPerfMon        = -1;   // dword_82F2A348
    static s32 siSolveContactsPerfMon       = -1;   // dword_82F2A34C
    static s32 siUpdateWheelsAndGlassPerfMon = -1;  // dword_82F2A350
    static s32 siUpdateSweptSpherePerfMon   = -1;   // dword_82F2A354
    static s32 siUpdateWorldSpheres         = -1;   // dword_82F2A358
    static s32 siCheckDetaching             = -1;   // dword_82F2A35C
    static s32 siUpdateSkinningOffsets      = -1;   // dword_82F2A360
    static s32 siUpdateIK                   = -1;   // dword_82F2A364
    static s32 siUpdateSuspensionIK         = -1;   // dword_82F2A368
    static s32 siUpdateLocators             = -1;   // dword_82F2A36C

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
        // Bind the scene/handling ids + the packed game-mode/state word from the event (asm: event[+1]
        // -> +26384 handling body, event[+4] -> +26392 game-mode word). The asm does NOT touch +26388
        // (mGlobalEntityId) in Prepare -- the entity id is only set/invalidated in Release -- so no
        // entity-id store is emitted here (a prior fabricated store was removed).
        mHandlingBodyID  = lrEvent.mHandlingBodyID;
        mu32GameModeState = 0u;   // FLAG: asm copies event[+4] into the +26392 packed word; the event's
                                  // game-mode word has no named field on AddDeformationModelEvent (the
                                  // bounce gate reads it off the OTHER car), so it is reconstructed here.

        // Resolve + cache the streamed spec from the model handle (asm: v12 = **event -> the resolved,
        // serialiser-fixed-up StreamedDeformationSpec pointer the resource handle points at).
        // FLAG: mModelHandle is a u32 ResourceHandle, not a raw pointer; the asm's double-dereference is
        // the resource-manager handle->spec resolve, which is not expressible from the frozen event
        // layout. Cached into mpDeformationSpec by name; pinned null until the resolve is re-derived
        // (NEVER fabricated). All spec-driven loops below correctly degenerate to empty under a null/zero
        // count, matching the asm's "no parts" path.
        mpDeformationSpec = nullptr;   // FLAG: = ResourceManager::Resolve(lrEvent.mModelHandle)

        // Pool index + the active/swept-sphere flags (asm: a3 -> +26290 index region; +26402 active=1;
        // +26410 = event[+156] swept flag; +26460/+26417/+26415 latches cleared).
        mu16DeformableObjectIndex = lu16Index;
        mbActive                  = true;
        mbDoSweptSphereTests      = lrEvent.mbDoSweptSphereTests;

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
        mHandlingBodyID.muValue   = 0xFFFFFFFFu;   // RigidBodyId::SetInvalid()
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

        // Spec bounding box + the initial damage compression scale/limits (asm: GetBoundingBox into a
        // scratch AABB, then a coin-flip-signed +/-0.30000001 damage-point offset built from a Random
        // draw, fed to GetInitialCompressionScalesAndLimits).
        // FLAG: StreamedDeformationSpec::GetBoundingBox + GetInitialCompressionScalesAndLimits use spec
        // internals not all exposed; the scratch values are pinned and the call kept by name.
        Vector3Plus lv3pCompressionScale_Scratch = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3     lvPosLimits = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3     lvNegLimits = { 0.0f, 0.0f, 0.0f, 0.0f };

        // The asm's per-driven-point damage-point sign is a Random parity draw (v49 & 1) selecting
        // +/-0.30000001 on the X lane (asm: 0.30000001 / -0.30000001 build). Faithful in structure.
        const bool lbDamageSignPositive = ((lrRandom.RandomUInt() & 1u) != 0u);
        Vector3 lDamagePoint = lbDamageSignPositive
                                 ? Vector3{  0.30000001f, 0.0f,  0.30000001f, 0.0f }
                                 : Vector3{ -0.30000001f, 0.0f,  0.30000001f, 0.0f };

        GetInitialCompressionScalesAndLimits(leResetType, lvfTime,
                                             lv3pCompressionScale_Scratch, lvPosLimits, lvNegLimits);

        // Re-seed every sensor's compression/displacement from the damage scale/limits/point (asm:
        // ResetSensors(this, scale, posLimits, negLimits, damagePoint)).
        ResetSensors(lv3pCompressionScale_Scratch, lvPosLimits, lvNegLimits, lDamagePoint);

        // --- rebuild the tag-point pool from the spec (asm: miNumTagPoints = spec->count; per-index
        //     bounds-assert; TagPoint::Construct(slot, spec->tagSpec[i], &mDrivenPoints[0])) -----------
        // FLAG: spec tag count + per-tag spec pointer accessors not exposed on frozen spec -> 0 here.
        const s32 liNumTagPoints = 0;   // FLAG: = mpDeformationSpec->miNumberOfTagPoints
        miNumTagPoints = liNumTagPoints;
        for (s32 liTag = 0; liTag < liNumTagPoints; ++liTag)
        {
            CGS_ASSERT(liTag < liNumTagPoints, "liIndex < miNumberOfTagPoints");
            CGS_ASSERT(liTag >= 0, "liIndex >= 0");
            // maTagPoints[liTag].Construct(spec->GetTagPointSpec(liTag), &maDrivenPoints[0]);  // FLAG
        }

        // --- rebuild the driven-point pool from the spec (asm mirror of the tag-point loop) ----------
        const s32 liNumDrivenPoints = 0;   // FLAG: = mpDeformationSpec->miNumberOfDrivenPoints
        miNumDrivenPoints = liNumDrivenPoints;
        for (s32 liDriven = 0; liDriven < liNumDrivenPoints; ++liDriven)
        {
            CGS_ASSERT(liDriven < liNumDrivenPoints, "liIndex < miNumberOfDrivenPoints");
            CGS_ASSERT(liDriven >= 0, "liIndex >= 0");
            // maDrivenPoints[liDriven].Construct(spec->GetDrivenPointSpec(liDriven), &maTagPoints[0]); // FLAG
        }

        // --- rebuild the IK parts (damage-state only) (asm: if (lbResetParts) { miNumIKBodyParts =
        //     spec->numIKParts; per-part bounds-assert; IKBodyPart::Construct + PrepareIKPart; seed
        //     mau8PhysicalBodyPartPoolIndex[i] = -1; then fill the tail [count..50) with state 0 /
        //     pool-index -1 }) ----------------------------------------------------------------------
        if (lbResetParts)
        {
            const s32 liNumIKParts = 0;   // FLAG: = mpDeformationSpec->miNumberOfIKParts
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
                // maIKParts[liPart].Construct(spec->GetDrivenPartSpec(liPart), &maDrivenPoints[0],
                //                             &maTagPoints[0]);                                   // FLAG
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

    // =================================================================================================
    // ConstructUpdatePerformanceMonitors @ 0x825B99A0
    //   Register the four per-frame Update-stage CPU perf monitors (Sort contacts, Solve contacts,
    //   Upd. Suspension IK, Upd. Locators), each asserting the returned handle id is >= 0. The
    //   AddMonitor args (group 15, parent 0, budget 10.0 ms, the per-call cookie, enabled 1) are
    //   verbatim from the asm.
    // =================================================================================================
    void DeformableObject::ConstructUpdatePerformanceMonitors()
    {
        siSortContactsPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Sort contacts", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siSortContactsPerfMon >= 0, "siSortContactsPerfMon >= 0");

        siSolveContactsPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Solve contacts", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siSolveContactsPerfMon >= 0, "siSolveContactsPerfMon >= 0");

        siUpdateSuspensionIK = CgsDev::PerfMonCpu::AddMonitor("          Upd. Suspension IK", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSuspensionIK >= 0, "siUpdateSuspensionIK >= 0");

        siUpdateLocators = CgsDev::PerfMonCpu::AddMonitor("          Upd. Locators", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateLocators >= 0, "siUpdateLocators >= 0");
    }

    // =================================================================================================
    // ConstructUpdateIKAndLocatorsPerformanceMonitors @ 0x825B9B00
    //   Register the IK/locators-stage perf monitors. NOTE the asm asserts siSolveContactsPerfMon >= 0
    //   FIRST (it was registered by ConstructUpdatePerformanceMonitors; this is a dependency tripwire,
    //   it does NOT register it), then adds Check Detaching, Upd. Skinning Offs, Upd. IK, and
    //   Upd. wheels & glass.
    // =================================================================================================
    void DeformableObject::ConstructUpdateIKAndLocatorsPerformanceMonitors()
    {
        // Dependency tripwire (asm: tests the already-registered solve-contacts handle, no AddMonitor).
        CGS_ASSERT(siSolveContactsPerfMon >= 0, "siSolveContactsPerfMon >= 0");

        siCheckDetaching = CgsDev::PerfMonCpu::AddMonitor("          Check Detaching", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siCheckDetaching >= 0, "siCheckDetaching >= 0");

        siUpdateSkinningOffsets = CgsDev::PerfMonCpu::AddMonitor("          Upd. Skinning Offs", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSkinningOffsets >= 0, "siUpdateSkinningOffsets >= 0");

        siUpdateIK = CgsDev::PerfMonCpu::AddMonitor("          Upd. IK", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateIK >= 0, "siUpdateIK >= 0");

        siUpdateWheelsAndGlassPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Upd.wheels & glass", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateWheelsAndGlassPerfMon >= 0, "siUpdateWheelsAndGlassPerfMon >= 0");
    }

    // =================================================================================================
    // ConstructPostPhysicsPerformanceMonitors @ 0x825B9C88
    //   Register the post-physics sphere-update perf monitors (Update World Spheres, Update Swept
    //   Spheres). NOTE the leading indentation in these two labels differs from the IK ones (8 spaces),
    //   verbatim from the asm.
    // =================================================================================================
    void DeformableObject::ConstructPostPhysicsPerformanceMonitors()
    {
        siUpdateWorldSpheres = CgsDev::PerfMonCpu::AddMonitor("        Update World Spheres", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateWorldSpheres >= 0, "siUpdateWorldSpheres >= 0");

        siUpdateSweptSpherePerfMon = CgsDev::PerfMonCpu::AddMonitor("        Update Swept Spheres", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSweptSpherePerfMon >= 0, "siUpdateSweptSpherePerfMon >= 0");
    }
}
}
