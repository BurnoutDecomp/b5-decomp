#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                 // ::VecFloat, Vector3, Matrix44Affine
#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"           // PropDebugComponent (by value, +0x00)
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"     // PropInstance, PropEntityID
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h" // PropPartInstance
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // UpdatePropEvent
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                  // CgsContainers::BitArray
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                    // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                      // CgsModule::IOBuffer (PropRaceCarContactBuffer base, 2026-08-09)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // CgsResource::ResourcePtr<T>

namespace CgsPhysics { namespace PhysicsSimulationIO { struct InAddPotentialContact; struct OutContactSpy; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; } }
namespace BrnPhysics { namespace ContactSpy { struct PropContact; } }
namespace CgsMemory { struct SimpleDataStreamProducer; }   // pointer-only member (mpPrimitiveWithTriangleStream)
// ⭐ ADDED 2026-08-06 (big-five #2): SetupAndValidatePropContact collaborators, pointer use only.
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InputBuffer; } }  // class key struct, matching CgsPhysicsSimulationModuleIO.h:43
namespace BrnPhysics { namespace Vehicle { class VehicleManager; } }
// (PropRaceCarContactBuffer is DEFINED below as of 2026-08-09 -- DWARF home
//  BrnPropManager.h:47; the old fwd-decl line stood here.)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                        // CgsPhysics::RigidBodyId (by value)
// Class key `struct`, matching rw/rwcore_structs.h -- a `class` here mangles differently.
namespace rw { struct IResourceAllocator; }

// ⭐ ADDED 2026-08-09 (conductor wave) -- collaborators of the four per-frame prop legs,
// pointer/element use only. Class keys match each committed home.
namespace CgsSceneManager { namespace SceneManagerIO { struct TriangleCacheInterface;
                                                       struct InSceneUpdateInterface; } }
namespace CgsSceneManager { namespace CgsCollision { struct CollisionGenerator; } }
// ⭐ ADDED 2026-08-11 (lifetime wave): UpdateTriangleCache's parameter, pointer only.
// Class key `struct`, matching the single home CgsSceneManagerIO.h:31.
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Update; } }
// ⭐ ADDED 2026-08-10 (create-path wave): ProcessInputsPreScene's first parameter, pointer only.
// Class key `struct`, matching the single home SharedIO/BrnPropInputInterface.h:54.
namespace BrnPhysics { namespace Props { struct PropInputInterface; } }
// (CgsSceneManager::EntityId arrives complete through CgsRigidBody.h -> CgsEntityId.h.)
namespace CgsMemory { class LinearMalloc; }
namespace CgsPhysics { namespace PhysicsSimulationIO { struct OutUpdateRigidBody; } }
namespace BrnPhysics { namespace PhysicsModuleIO { class OutputBuffer; struct PotentialContactInterface; } }
// ⭐ ADDED 2026-08-18 (breakable-props keystone). Pointer/reference use only; class keys match
// each committed home:
//   PropTypeData / PropPartTypeData -- SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h
//                                      (both `class` there; NOT owned by this TU)
//   RaceCarPhysics                  -- GameSource/Physics/VehicleManager/VehiclePhysics/
//                                      RaceCarPhysics.h:38 (`class`, derives VehiclePhysics)
namespace BrnPhysics { namespace Props   { class PropTypeData; class PropPartTypeData; } }
namespace BrnPhysics { namespace Vehicle { class RaceCarPhysics; } }

namespace BrnPhysics
{
namespace Props
{
    class PropPhysicsDataHeader;      // ResourcePtr referent only (SharedClasses/Physics/Props)

    // ======================================================================================
    // PropRaceCarContactBuffer -- DWARF home BrnPropManager.h:47.       NEW 2026-08-09
    // (conductor wave). The per-frame prop-vs-racecar contact IO buffer PhysicsModule::
    // Update @0x825B0640 pushes on the output stack ("PropRaceCarContacts"). Console
    // attestation: CreateIOBuffer<PropRaceCarContactBuffer> @0x825AC4A0 allocates 992
    // bytes (`li r4,0x3E0`) and runs Construct, which constructs ONE
    // EventQueue<PropRaceCarContact,30> at +16 (`addi r3,r11,0x10 ; bl
    // EventQueue<PropRaceCarContact,30>::Construct @0x825A81B8`) -- 16 + (16 + 30*32)
    // == 992 with zero slack, and both spans are pointer-free-identical on the host
    // (the queue header pads to 16 on both targets).
    // ======================================================================================
    struct PropRaceCarContactBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<PropRaceCarContact, 30> PropRaceCarContactQueue;  // :62

        // :78 -- raise the buffer status, construct the queue (the X360 stack template's
        // inline; the PC stack template placement-news only, so Update calls this).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mPropRaceCarContactQueue.Construct();
        }

        // :81.
        void Destruct()
        {
            CgsModule::IOBuffer::Destruct();
        }

    private:
        u8                      maStatusPad[15];           // +1..+15 (force +16)
        PropRaceCarContactQueue mPropRaceCarContactQueue;  // +16  (:85)
    };

    // ---- The prop-physics tuning globals ------------------------------------------------
    // Namespace-scope, defined in BrnPropManager.cpp -- which is exactly where the DecFIGS
    // dwarfdump puts them (BrnPropManager.cpp:45..51). They are the twelve values the prop
    // debug component registers with the debug UI, and the seven VecFloats are what its
    // OnChange* callbacks splat-store into (X360 0x825BAEF0..0x825BB040, PS3 0x6B5AA4..0x6B5C7C
    // where the relocations carry the names).
    //
    // =====================================================================================
    // ⭐⭐ THE VALUES ARE RECOVERED. 2026-08-18, round 3b. READ THIS BEFORE TRUSTING ANY
    //     "UNRECOVERED" NOTE ANYWHERE IN THIS SUBSYSTEM -- there are none left that are true.
    //
    // Every one of the 0x82FB9xxx tuning globals below (and in the two blocks further down)
    // really does read 16 zero bytes in the static image -- that half of the old note was
    // right. What was WRONG, in every file that repeated it, is "the export set contains no
    // function that initialises them -- only readers". They are initialised, by an MSVC
    // DYNAMIC-INITIALISER THUNK that lives OUTSIDE every IDA function, which is exactly why
    // an xrefs-over-exports scan reports "readers only".
    //
    // THE RECIPE (walked end to end for every constant in this file; re-walk it before you
    // ever add a new one, and NEVER read a value off a sibling's pattern):
    //   1. Take the global's address and list its .text data xrefs. One of them is not inside
    //      any function; walk back from it to the instruction after the previous `blr` --
    //      that is the thunk's entry.
    //   2. Confirm the entry address is an element of the MSVC dynamic-initialiser pointer
    //      table. MEASURED extent 0x82CD0014..0x82CD3170 (3,159 dword slots; raw dword scan
    //      requiring the target to land in .text -- an earlier probe using a bare address-range
    //      filter reported the narrower 0x82CD0908..0x82CD3170 / 2,586, same upper bound, and
    //      every thunk named here is well inside BOTH). The per-constant slot is on its line.
    //   3. Decode the thunk. Scalar shape (10 instructions):
    //          lis r11,flt_82xxxxxx@ha ; addi r10,r1,-0x10 ; lfs f0,flt_82xxxxxx@l(r11)
    //          lis r11,<global>@ha ; stfs f0,-0x10(r1) ; lvlx v0,r0,r10
    //          addi r11,r11,<global>@l ; vspltw v0,v0,0 ; stvx128 v0,r0,r11 ; blr
    //      i.e. `<global> = Splat(flt_82xxxxxx)` -- all four lanes.
    //      Vector3 shape (16 instructions): three `stfs` into -0x10/-0xC/-8(r1) (x/y/z, which
    //      may come from two different rodata words), `stw r9(=0)` into -4 (w), then
    //      `lvx128`/`stvx128`. Lanes are decoded per-lane, never assumed to be a splat.
    //      Self-product shape (8 instructions): `lvx128` of a SIBLING global, `vmulfp128 v0,v0,v0`,
    //      `stvx128` into this one -- the square of the sibling, see the _SQ pair below.
    //   4. Read the rodata word as a big-endian u32 and print BOTH the hex and the float.
    // Every value below carries its own thunk address, table slot, rodata address and hex word.
    // MEASURED this round with headless IDA 9.3 against a PRIVATE COPY of
    // IDA Files/BURNOUT_X360_ARTIST.XEX.i64; the sibling PropManager_wQ2_03.cpp landed
    // the first instance of this recipe (0x82FB94F0) and this block is the rest of it.
    //
    // ⚠️ The five f32s below have real STATIC initialisers (non-zero bytes on disk), read
    //    straight out of the shipped image. The seven VecFloats are the DYNAMIC kind above.
    // =====================================================================================
    extern ::VecFloat KVF_GRAVITY_SCALE;                   // X360 0x82FB94E0 = Splat(3.0f)
                                                           //   thunk 0x82C5E6E8 (tbl slot 0x82CD19A4)
                                                           //   rodata flt_82004270 = 0x40400000
    extern ::VecFloat KVF_INERTIA_SCALE;                   // X360 0x82FB9400 = Splat(3.0f)
                                                           //   thunk 0x82C5E6C0 (tbl slot 0x82CD19A0)
                                                           //   rodata flt_82004270 = 0x40400000
    extern ::VecFloat KVF_ANTI_HERD_UPWARD_SCALE;          // X360 0x82FB93E0 = Splat(2.0f)
                                                           //   thunk 0x82C5EA00 (tbl slot 0x82CD19F0)
                                                           //   rodata flt_82001D9C = 0x40000000
    extern ::VecFloat KVF_ANTI_HERD_SIDE_SCALE;            // X360 0x82FB9450 = Splat(0.05f)
                                                           //   thunk 0x82C5EA28 (tbl slot 0x82CD19F4)
                                                           //   rodata flt_820047C8 = 0x3D4CCCCD
    extern ::VecFloat KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE; // X360 0x82FB9D70 = Splat(1.5f)
                                                           //   thunk 0x82C5EA50 (tbl slot 0x82CD19F8)
                                                           //   rodata flt_820945DC = 0x3FC00000
    extern ::VecFloat KVF_MAX_SPEED_FOR_SIDE_FORCE;        // X360 0x82FB93A0 = Splat(60.0f)
                                                           //   thunk 0x82C5EA78 (tbl slot 0x82CD19FC)
                                                           //   rodata flt_82092BC4 = 0x42700000
    extern ::VecFloat KVF_SPEED_CLAMP;                     // X360 0x82FB94A0 = Splat(120.0f)
                                                           //   thunk 0x82C5EAA0 (tbl slot 0x82CD1A00)
                                                           //   rodata flt_82092BC8 = 0x42F00000

    extern f32 KF_PROP_ANGULAR_DRAG;                       // X360 0x82F2A388
    extern f32 KF_PROP_LINEAR_DRAG;                        // X360 0x82F2A38C
    extern f32 KF_PROP_MAX_ANGULAR_VEL;                    // X360 0x82F2A390
    extern f32 KF_PROP_MAX_LINEAR_VEL;                     // X360 0x82F2A394
    extern f32 KF_PROP_RESTITUTION;                        // X360 0x82F2A398

    // =====================================================================================
    // ⭐ ADDED 2026-08-18 (breakable-props keystone wave) -- THE OTHER SEVEN ZERO-PAGE
    // CONSTANTS THIS TU'S UN-BODIED FUNCTIONS READ. The seven above are the ones
    // PropDebugComponent::OnActivate @0x825E3628 registers with the debug UI, which is
    // where their address<->name mapping comes from; these seven are NOT registered, so
    // their names come from the DecFIGS global list for this exact .cpp
    // (references/DecFIGS/dwarfdump/.../BrnPropManager.cpp, the `BrnPhysics::Props`
    // namespace block) matched against the ROLE each address plays in the asm. Each
    // mapping's evidence is on its line.
    //
    // ⭐ VALUES RECOVERED 2026-08-18 (round 3b) by the dynamic-initialiser-thunk recipe at the
    // top of this block. The old "reads all-zero on disk, no initialiser, only readers, values
    // UNRECOVERED" caveat that used to sit here was measured FALSE and is retired: the zero
    // bytes on disk are real, the "no initialiser" claim was not. Each constant's thunk, table
    // slot, rodata address and hex word are on its own declaration below.
    // =====================================================================================

    // X360 0x82FB93F0. DWARF BrnPropManager.cpp:53 (`const rw::math::vpu::Vector3`).
    // EVIDENCE: ReadUpdatedBodies @0x82632D14..0x82632D4C computes
    // `force = <this> * (KVF_GRAVITY_SCALE - 1.0f)` (v127 is a vcsxwfp128 splat of 1.0f) and
    // posts it as one InApplyForce event per non-frozen moving prop -- i.e. the extra gravity
    // needed on top of the simulation's own 1g so the prop falls at KVF_GRAVITY_SCALE x g.
    // A gravity DIRECTION times a (scale - 1) is what K_DEFAULT_GRAVITY is for, and it is the
    // only Vector3-typed (not VecFloat) gravity global the DWARF puts in this file.
    // ⭐ VALUE = (0.0f, -9.8f, 0.0f), w == 0. Vector3-shaped thunk 0x82C5E710 (tbl slot
    //    0x82CD19A8), decoded PER LANE, not assumed to be a splat:
    //      -0x10(r1) x <- flt_82001CC0 = 0x00000000 =  0.0f
    //      -0x0C(r1) y <- flt_82013FC8 = 0xC11CCCCD = -9.800000190734863f
    //      -0x08(r1) z <- flt_82001CC0 = 0x00000000 =  0.0f
    //      -0x04(r1) w <- `stw r9` with r9 == 0
    //    and the confirmation that the ROLE note above is right: a straight-down unit-ish
    //    gravity direction, scaled by (KVF_GRAVITY_SCALE - 1) == 2 for a 3g prop fall.
    extern const Vector3 K_DEFAULT_GRAVITY;

    // X360 0x82FB93C0. DWARF BrnPropManager.cpp:557 (`const rw::math::vpu::Vector3`).
    // EVIDENCE: AddPropToSim @0x82627714 loads it under `if (lbAddExtraComOffset)` -- the
    // second bool parameter -- splats x/y/z and rotates it by the prop's transform rows
    // before adding it to the body position; ReadUpdatedBodies @0x82632A7C reads the same
    // address on the mu8Flags & KU_HAS_EXTRA_COM_OFFSET_FLAG path to undo it. The flag's own
    // name and the global's name are the same fact.
    // ⭐ VALUE = (0.0f, 0.0f, -0.2f), w == 0. Vector3-shaped thunk 0x82C5E7F0 (tbl slot
    //    0x82CD19BC), decoded per lane:
    //      -0x10(r1) x <- flt_82001CC0 = 0x00000000 =  0.0f
    //      -0x0C(r1) y <- flt_82001CC0 = 0x00000000 =  0.0f
    //      -0x08(r1) z <- flt_82020A84 = 0xBE4CCCCD = -0.20000000298023224f
    //      -0x04(r1) w <- `stw r9` with r9 == 0
    //    i.e. a 20 cm shift of the centre of mass along the prop's own -Z, which is what makes
    //    a lamppost/sign topple away from its base rather than pivot about it.
    extern const Vector3 K_PROP_EXTRA_COM_OFFSET;

    // X360 0x82FB9420. DWARF BrnPropManager.cpp:2675 (`const rw::math::vpu::Vector3`).
    // EVIDENCE: GetPropInertia @0x826129BC substitutes it for the accumulated volume AABB
    // half-extents when the type's graphics id is 428364 or 428388 (the two lamppost scene-uri
    // ids), immediately before the `mass * (1/3) * (d_j^2 + d_k^2)` box-inertia fold.
    // ⚠️ CORRECTED 2026-08-18 (round 2): this line used to say `mass * (1/12) * (d^2)`. That was
    //    WRONG and it was wrong by a factor of four -- see the GetPropInertia/GetPartInertia
    //    block below for the measurement (flt_820065E0 == 1/3, and the "dimensions" are
    //    half-extents about the ORIGIN, not full box dimensions).
    // ⭐ VALUE = (2.0f, 1.0f, 2.0f), w == 0. Vector3-shaped thunk 0x82C5EAC8 (tbl slot
    //    0x82CD1A04), decoded per lane -- note x and z share ONE rodata word and y a different
    //    one, so this is emphatically not a splat:
    //      -0x10(r1) x <- flt_82001D9C = 0x40000000 = 2.0f
    //      -0x0C(r1) y <- flt_82001C98 = 0x3F800000 = 1.0f
    //      -0x08(r1) z <- flt_82001D9C = 0x40000000 = 2.0f
    //      -0x04(r1) w <- `stw r9` with r9 == 0
    //    A 2 x 1 x 2 metre half-extent box about the origin: deliberately FATTER than a real
    //    lamppost so the substituted inertia resists spin, which is the point of the special case.
    extern const Vector3 K_LAMPOST_INERTIA_BOX;

    // ClampAcceleration @0x82627F00's four thresholds. DWARF BrnPropManager.cpp:1174..1177.
    // EVIDENCE (roles are unambiguous from the two symmetric blocks; the ADDRESS ORDER is
    // NOT the declaration order -- the linker sorted them, so do not re-derive from address):
    //   linear  block: `vmsum3fp128` of (dv * invDt) compared against 0x82FB9F40, then the
    //                  clamped magnitude taken from 0x82FB94B0
    //   angular block: same shape against 0x82FB94D0, magnitude from 0x82FB9490
    //
    // ⭐ VALUES RECOVERED. The two non-_SQ ones are ordinary scalar thunks; the two _SQ ones are
    //    the SELF-PRODUCT shape -- their thunk loads the SIBLING global, `vmulfp128 v0,v0,v0`,
    //    and stores the square. That makes them ORDER-DEPENDENT on the console (the _SQ table
    //    slots 0x82CD19C8 / 0x82CD19CC sit AFTER their bases' 0x82CD19C0 / 0x82CD19C4, which is
    //    what makes the console's own result well-defined). The host tree does NOT reproduce
    //    that dependency: they are seated as LITERALS, and both squares are exact in f32.
    extern const VecFloat KVF_MAX_LINEAR_ACCELERATION;     // X360 0x82FB94B0 = Splat(30.0f)
                                                           //   thunk 0x82C5E830 (tbl slot 0x82CD19C0)
                                                           //   rodata flt_82004F5C = 0x41F00000
    extern const VecFloat KVF_MAX_ANGULAR_ACCELERATION;    // X360 0x82FB9490 = Splat(80.0f)
                                                           //   thunk 0x82C5E858 (tbl slot 0x82CD19C4)
                                                           //   rodata flt_82004A18 = 0x42A00000
    extern const VecFloat KVF_MAX_LINEAR_ACCELERATION_SQ;  // X360 0x82FB9F40 = Splat(900.0f)
                                                           //   thunk 0x82C5E880 (tbl slot 0x82CD19C8):
                                                           //   lvx128 unk_82FB94B0 / vmulfp128 v0,v0,v0
                                                           //   -- 30^2, NO rodata word of its own
    extern const VecFloat KVF_MAX_ANGULAR_ACCELERATION_SQ; // X360 0x82FB94D0 = Splat(6400.0f)
                                                           //   thunk 0x82C5E8A0 (tbl slot 0x82CD19CC):
                                                           //   lvx128 unk_82FB9490 / vmulfp128 v0,v0,v0
                                                           //   -- 80^2, NO rodata word of its own

    // ⚠️⚠️ X360 0x82FB94C0 -- NAME NOT RECOVERED. THIS IDENTIFIER IS AUTHORED, NOT RECOVERED.
    // The DecFIGS global list for BrnPropManager.cpp does not contain any constant that fits,
    // and the address is registered with no debug variable, so there is nothing to match it
    // against. What IS measured is its ROLE: ReadUpdatedBodies @0x82632B04 loads it and does
    // `vcmpgtfp <this>, transform.Pos().y` -- when it is GREATER than the body's world Y the
    // function logs "\t Warning!! prop fell out of the world: " and forces the prop's removal
    // (the same latch the frozen/removal path sets). So it is a world FLOOR height, splatted.
    // Named descriptively so ReadUpdatedBodies can compile and read honestly; RENAME IT the
    // moment a real name turns up (a DWARF gap, a PS3 relocation, or another TU's reader).
    //
    // ⭐ ITS VALUE, HOWEVER, IS RECOVERED -- name and value are separate facts and only the name
    //    is still missing. Scalar thunk 0x82C5B570 (tbl slot 0x82CD15E8), rodata flt_8200D4F8 =
    //    0xC47A0000 = -1000.0f, splatted. A world floor a kilometre below the origin, which is
    //    the sane reading of the ROLE above and is nothing like the Y == 0 the placeholder zero
    //    implied.
    // ⚠️ NEW EVIDENCE ON THE MISSING NAME, recorded rather than acted on: every OTHER constant in
    //    this file initialises from the CONTIGUOUS table run 0x82CD19A0..0x82CD1A04 -- one
    //    translation unit's initialisers, emitted in declaration order. This one's slot,
    //    0x82CD15E8, is ~950 slots earlier, i.e. a DIFFERENT TU emitted it. That is the most
    //    likely reason the DecFIGS global list for BrnPropManager.cpp has no candidate to match:
    //    the constant is probably not declared in this .cpp at all. FOLLOW-UP for whoever owns
    //    the neighbouring subsystem: walk the other constants initialised from the
    //    0x82CD15xx run, identify their TU, and the name should fall out. Until then it stays
    //    defined here because ReadUpdatedBodies is this file's and needs exactly one definition.
    extern const VecFloat KVF_PROP_OUT_OF_WORLD_HEIGHT;    // X360 0x82FB94C0  (AUTHORED NAME,
                                                           //   RECOVERED VALUE = Splat(-1000.0f))

    // ⭐⭐ NAME RECOVERED 2026-08-18 (wave Q4, physics-contact seam). This declaration read
    // `KVF_MAX_CONTACT_GEN_PADDING`, flagged "NAME AUTHORED, not recovered ... no DWARF
    // candidate". BOTH halves of that were wrong, and the DWARF candidate was in the file the
    // note said had none: `const VecFloat KVF_MAX_PROP_PADDING;` at
    // references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:54.
    // WHAT SETTLES IT is not the name's plausibility but the INITIALISER-TABLE ORDER, measured
    // this wave with headless IDA 9.3 over a private copy of the .i64: the MSVC dynamic-
    // initialiser pointer table runs in SOURCE-DECLARATION order for this file, and the slots
    // line up one-for-one with the DWARF's source lines --
    //     0x82CD19A0 :50 KVF_INERTIA_SCALE        0x82CD19A4 :51 KVF_GRAVITY_SCALE
    //     0x82CD19A8 :53 K_DEFAULT_GRAVITY        0x82CD19AC :54 <-- THIS ONE
    //     0x82CD19B0 :338 LEAN_PROP_LERP_SPEED    0x82CD19B4 :339 LEAN_PROP_MIN_LERP  ...
    // -- i.e. 0x82FB94F0's initialiser sits in the slot the DWARF's :54 must occupy, between
    // K_DEFAULT_GRAVITY (:53, independently pinned) and the three :338-:340 lean constants
    // (independently pinned). The name is therefore MEASURED, not authored, and is corrected
    // here. (No code referenced the old spelling -- only this declaration, its definition, and
    // two comments -- so this is a rename with no call-site cost. ⚠️ The two PARKED
    // contact-generation bodies at scratchpad/waveQ2/parked/PropManager_03_Do*.cpp each carry a
    // file-local `static const VecFloat KVF_MAX_CONTACT_GEN_PADDING`; their lander should delete
    // that local and reach this one under its recovered name.)
    // ROLE is measured, unchanged: the two world contact-generation legs --
    // DoPartWorldContactGeneration @0x82611D8C and DoPropInstanceWorldContactGeneration
    // @0x82612280 -- clamp their per-frame swept expansion with `Min(padding, <this>)`.
    // ⭐ VALUE = Splat(0.3f); thunk 0x82C5E750 (tbl slot 0x82CD19AC), rodata flt_82004740 =
    //    0x3E99999A. This is the constant PropManager_wQ2_03.cpp's banner walked end to end first --
    //    the measurement that showed the whole "no initialiser / values UNRECOVERED" story in this
    //    file was wrong -- and it is HEADER REQUEST D from that file's banner, landed here so the
    //    parked contact-generation bodies have one home to reach for instead of a file-local copy.
    //    Behaviourally: a 30 cm cap on the swept expansion. At the placeholder zero, every prop and
    //    part collision primitive was posted with NO swept padding at all.
    // ⚠️ HEADER REQUEST D's OTHER HALF IS NOT LANDED: `KB_USE_CONTACT_GEN_STREAM == true` has no
    //    address and no thunk cited anywhere, so there is nothing for this round to re-measure and
    //    nothing is invented for it. It stays an open request against this header.
    extern const VecFloat KVF_MAX_PROP_PADDING;            // X360 0x82FB94F0  (DWARF NAME :54,
                                                           //   RECOVERED VALUE = Splat(0.3f))

    // ⭐ ADDED 2026-08-18 (wave Q4, physics-contact seam). X360 0x82FB93B0.
    // DWARF `const VecFloat KVF_MAX_LEAN_PROP_WORLD_PENETRATION;` at BrnPropManager.cpp:1288.
    // SOLE READER, and the whole reason it exists: SetupAndValidatePropContact @0x826285C8
    // (`vcmpgtfp. v0, v13, v0` against the dot of the Y-flattened contact normal with
    // mPointOnB - mPointOnA). When a JOINTED prop is pushed into the WORLD by more than this,
    // its joint index is set in mBreakPropJoints -- i.e. this is the threshold at which a smash
    // gate's joint gives way.
    // ADDRESS<->NAME: measured three ways, no inference. (a) A whole-image xref scan returns
    // exactly TWO references to 0x82FB93B0 -- the read above and the write in its own
    // initialiser thunk -- so this function is its only consumer. (b) The DWARF puts
    // KVF_MAX_LEAN_PROP_WORLD_PENETRATION at source :1288, immediately before this function's
    // own first local at :1300, with no other file-scope VecFloat between them. (c) Its
    // initialiser-table slot is 0x82CD19D0, which is exactly the slot between :1177
    // (KVF_MAX_ANGULAR_ACCELERATION_SQ, 0x82CD19CC) and :1678 (KVF_MAX_PROP_SPEED_MPS,
    // 0x82CD19D4) -- the same source-order property that settles KVF_MAX_PROP_PADDING above.
    // ⭐ VALUE = Splat(0.01f). Thunk 0x82C5E8C0 (tbl slot 0x82CD19D0), rodata flt_82002138 =
    //    0x3C23D70A. A ONE-CENTIMETRE penetration budget -- and the same rodata word
    //    KVF_LEAN_PROP_MIN_LERP uses (ordinary literal pooling; separate thunks, separate
    //    addresses).
    // ⚠️ THE PLACEHOLDER ZERO WOULD NOT HAVE BEEN NEUTRAL, which is why the value is seated
    //    rather than left at the on-disk zeroes: with the threshold at 0 every jointed prop
    //    would break its joint on the first world contact with any positive penetration at all.
    extern const VecFloat KVF_MAX_LEAN_PROP_WORLD_PENETRATION;   // X360 0x82FB93B0

    // =====================================================================================
    // ⭐ ADDED 2026-08-18 (round 3, fix round) -- THE SIX REMAINING ZERO-PAGE TUNABLES THIS
    // TU'S NEWLY-BODIED FUNCTIONS READ. Requested by the UpdateJointedProps/BreakJoint lander
    // (five) and by the ApplyPropRaceCarCollisionImpulse lander (one); until now they were
    // spelled as file-local `extern`s in PropManager_wQ2_05.cpp with no definition anywhere,
    // i.e. a latent LNK2001. Homed here so there is ONE declaration and ONE definition.
    //
    // NAMES are DWARF-attested for THIS .cpp (references/DecFIGS/dwarfdump/GameSource/Physics/
    // PropManager/BrnPropManager.cpp, the `BrnPhysics::Props` namespace block, source lines
    // :338/:339/:340/:1678/:2046/:2047); the const-ness on each line below is the DWARF's own.
    // ADDRESS<->NAME is the same class of mapping as the seven above: the DWARF orders the file
    // scope by source line, and each address's SOLE reader is the function whose own source
    // scope immediately follows that line. Every reader is named on its line.
    //
    // ⭐⭐ VALUES RECOVERED 2026-08-18 (round 3b). THE ROUND-3 NOTE THAT USED TO SIT HERE WAS
    // WRONG IN ITS CENTRAL CLAIM AND IS RETIRED -- recording the error deliberately, because it
    // is the trap: round 3 correctly dumped all six as 16 zero bytes, correctly found "exactly
    // two data xrefs -- its one PropManager reader, and one unnamed store stub in the 0x82C5Exxx
    // run", and then MIS-CLASSIFIED that stub as "the debug-UI OnChange shape ... a runtime
    // writer is not a static initialiser". It is not an OnChange handler. It is the MSVC
    // DYNAMIC INITIALISER, an element of the 0x82CD00xx..0x82CD31xx pointer table, and the
    // pattern-match went wrong precisely because it compared shapes instead of checking table
    // membership. The check that settles it is one line of script -- see the recipe at the top
    // of this tuning-globals block, step 2 -- and it was run for all six below.
    //   The old "CONSEQUENCE with the zeroes" paragraph (a leaning prop never lerps upright, a
    //   broken joint released with zero throw and zero spin) described a bug this tree WOULD
    //   have shipped, not the console. It is retired with the claim it rested on.
    // X360 0x82FB9500. DWARF :338. Reader: UpdateJointedProps @0x82631954.
    // ROLE (measured @0x82631A14..0x82631A38, moved here from PropManager_wQ2_05.cpp where it was
    // attached to a now-deleted file-local re-declaration): loaded once per jointed prop and
    // multiplied into all FOUR rows of (desired - current) before the sum is added back onto
    // `current` -- i.e. the per-frame blend fraction of the lean lerp. A LERP SPEED, and the only
    // one of the three that is used as a multiplier rather than a threshold.
    // ⭐ VALUE = Splat(0.1f). Thunk 0x82C5E778 (tbl slot 0x82CD19B0), rodata flt_82004014 =
    //    0x3DCCCCCD. A 10%-per-frame blend, which is the ROLE's own sanity check.
    extern const VecFloat KVF_LEAN_PROP_LERP_SPEED;

    // X360 0x82FB9F30. DWARF :339. Reader: UpdateJointedProps @0x82631960.
    // ROLE (measured @0x82631978 + 0x82631A00, moved here from PropManager_wQ2_05.cpp): the
    // threshold the summed per-row |desired - current| is compared against (`vcmpgtfp.`) to decide
    // whether the lerp runs at all -- i.e. the SMALLEST difference still worth lerping. The DWARF
    // name that fits a "don't bother below this" gate on a lerp is MIN_LERP.
    // ⭐ VALUE = Splat(0.01f). Thunk 0x82C5E7A0 (tbl slot 0x82CD19B4), rodata flt_82002138 =
    //    0x3C23D70A. One tenth of the lerp speed above, i.e. a dead-band an order of magnitude
    //    below the step size -- the ordering a "min lerp" gate has to have to make sense.
    extern const VecFloat KVF_LEAN_PROP_MIN_LERP;

    // X360 0x82FB9390. DWARF :340. Reader: UpdateJointedProps @0x82631938.
    // ROLE (measured @0x82631D9C + 0x82631DC4, moved here from PropManager_wQ2_05.cpp): the
    // tolerance the residual of (M * transpose(M) - I) is compared against, immediately before the
    // BrnPropManager.cpp:393 assert. That is an ORTHOGONALITY tolerance and nothing else.
    // ⭐ VALUE = Splat(0.1f). Thunk 0x82C5E7C8 (tbl slot 0x82CD19B8), rodata flt_82004014 =
    //    0x3DCCCCCD -- the SAME rodata word KVF_LEAN_PROP_LERP_SPEED uses. Two constants sharing
    //    one rodata slot is ordinary compiler pooling of a repeated literal, not a sign that the
    //    address<->name mapping collided: they have separate thunks writing separate addresses.
    extern const VecFloat KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE;

    // X360 0x82FB9440. DWARF :2046. Reader: BreakJoint @0x82628BD8.
    // ROLE (measured @0x82628C28, moved here from PropManager_wQ2_05.cpp): multiplies the CROSS
    // PRODUCT of the released spin with the joint->prop lever arm, i.e. it scales a LINEAR
    // (tangential) velocity.
    // ⭐ VALUE = Splat(1.0f). Thunk 0x82C5E9B0 (tbl slot 0x82CD19E8), rodata flt_82001C98 =
    //    0x3F800000. Unity -- so the release is the raw cross product, unscaled.
    extern VecFloat       KVF_BREAK_JOINT_LINEAR_VEL;

    // X360 0x82FB9460. DWARF :2047. Reader: BreakJoint @0x82628BE8.
    // ROLE (measured @0x82628C08, moved here from PropManager_wQ2_05.cpp): multiplies
    // maLastJointRotation[joint], the stored per-joint ANGULAR rate, and the product is handed to
    // AddPropToSim in v2 == lAngularVelocity.
    // ⭐ VALUE = Splat(1.0f). Thunk 0x82C5E9D8 (tbl slot 0x82CD19EC), rodata flt_82001C98 =
    //    0x3F800000. Unity, same as the linear one -- both are pass-through gains the console kept
    //    as tunables and never tuned away from 1.
    extern VecFloat       KVF_BREAK_JOINT_ANGULAR_VEL;

    // X360 0x82FB9470. DWARF BrnPropManager.cpp:63 -> source :1678.
    // ⚠️ ADDRESS<->NAME IS AN INFERENCE, flagged as one: the address carries no symbol, and the
    // mapping rests on (a) it being the ONLY rodata/bss datum ApplyPropRaceCarCollisionImpulse
    // @0x825E3560 touches, (b) the DWARF putting `VecFloat KVF_MAX_PROP_SPEED_MPS` at source
    // :1678, immediately before that function's own scope at :1685, with no other file-scope
    // VecFloat between them, and (c) the ROLE matching the name: the body computes
    // `saturate( <this> - |relative velocity| )` and scales the impulse by it
    // (0x825E35E4 `lvx128 v12,r0,r10` -> 0x825E3600 `vsubfp v0,v12,v0` -> vmaxfp 0 / vminfp 1.0
    // -> 0x825E3618 `vmulfp128 v0,v13,v0`), i.e. a speed above which a prop stops kicking back.
    // ⭐ VALUE RECOVERED = Splat(10.0f); thunk 0x82C5E8E8 (tbl slot 0x82CD19D4), rodata
    //    flt_82004A20 = 0x41200000. 10 m/s is ~36 km/h, and the saturate() above then fades the
    //    kick-back out linearly to zero as the closing speed reaches it -- which is a strong
    //    independent CORROBORATION of the inferred name: a value of 10 in a `speed` role fits
    //    KVF_MAX_PROP_SPEED_MPS and would fit nothing else in the DWARF's file scope. The
    //    mapping stays flagged as an inference (corroboration is not proof), but it is now a
    //    better-supported one than it was.
    extern VecFloat       KVF_MAX_PROP_SPEED_MPS;            // X360 0x82FB9470  (INFERRED MAPPING,
                                                             //   RECOVERED VALUE = Splat(10.0f))

    // =====================================================================================
    // BrnPhysics::Props::PropManager -- FULL member sequence, DWARF declaration order
    // (references/DecFIGS/.../BrnPropManager.h:245..301), landing gap-free on every offset
    // the X360 asm touches. The offset column is NOT a host layout claim (x64 widens the
    // pointers); it is the evidence column -- each line is an actual load or store in the
    // ARTIST image, and the run being gap-free is what makes the name<->offset mapping a
    // proof rather than a proposal. The derivation, its arithmetic self-checks, and the
    // (now settled) history are in BrnPropManager.cpp's banner.
    //
    // ⭐ THE CLASS HAS NO BASE. Two things settle it: the dwarfdump prints base classes and
    //    prints `struct BrnPhysics::Props::PropManager {` with none, and Construct @0x82627390
    //    calls PropDebugComponent::Construct with r3 == r4 == this, i.e. &mDebugComponent ==
    //    this. The `bl BaseCollisionGenerator::Destruct` in Destruct's tail (which an older
    //    note read as a base-class call) is an ICF fold: THREE different empty `void f(T*)`
    //    bodies in this one subsystem call that same address -- PropDebugComponent::Construct
    //    @0x825BAD74 where DebugComponent::Construct belongs, PropDebugComponent::OnRegister
    //    @0x822A9750 (a bare `b` to it) where DebugComponent::OnRegister belongs, and
    //    PropManager::Destruct @0x825E33E4 where mDebugComponent's own Destruct tail belongs.
    // =====================================================================================
    class PropManager
    {
    public:
        // DWARF BrnPropManager.h:103..107 (nested). Sized by Construct's own allocation
        // request: 0x600 bytes for KI_MAX_DEBUG_WORLD_CONTACTS(32) entries == 48 bytes each,
        // which is exactly three Vector3s.
        struct DebugWorldContactInfo
        {
            Vector3 mPoint0;
            Vector3 mPoint1;
            Vector3 mNormal;
        };

        static const s32 KI_MAX_DEBUG_WORLD_CONTACTS = 32;    // DWARF BrnPropManager.h:110

        // ADDITIVE 2026-08-04 (task #135) -- the stage-5 arm of BrnPhysics::PhysicsModule::
        // Prepare @0x825ADB68 (`bl` at 0x825ADDCC, result tested with a `bne` so the return is a
        // bool). Its one argument is the physics resource allocator (bank 23). Declaration-only;
        // the body is a named LINK STUB in WorldLinkStubs.cpp:516 until this manager's own
        // prepare pass lands, so the drop is one greppable symbol rather than a silent
        // `return true` for the whole physics module.
        //
        // ⚠️ ADDRESS CORRECTED 2026-08-18 (round 2): this comment used to say "@0x82C08ED0".
        //    0x82C08ED0 is `__savegprlr_22`, the PPC register-save runtime helper -- it is the
        //    FIRST `bl` in Prepare's own prologue, not Prepare. The real address is
        //    **0x8260EE18** (126 instructions), confirmed by progress/identity.json and by its
        //    xrefs_to being exactly {PhysicsModule::Prepare @0x825ADB68}.
        // MEASURED body shape (for whoever lands it): `std 0, 0x80(this)` + `std 0, 0x90(this)`
        //    (mUsedProps / mUsedParts cleared) -> DebugComponent::Register() -> two
        //    IResourceAllocator::Allocate calls through the allocator's vtable slot +0x10, one
        //    named "PropInstances" with size 0x690 == 15 * 112 and one named "PropPartInstances"
        //    with size 0x780 == 30 * 64, whose results land in mpaPropInstances (+0x7C) and
        //    mpaPartInstances (+0x8C) under the asserts "mpaPropInstances != NULL"
        //    (BrnPropManager.cpp:205) and "mpaPartInstances != NULL" (:223) -> `li r3,1`,
        //    i.e. it returns true unconditionally. NOTE the two console sizes are 15*112 and
        //    30*64 on the CONSOLE; on the host use `KU_MAX_PHYSICAL_PROPS * sizeof(PropInstance)`
        //    and `KU_MAX_PHYSICAL_PROP_PARTS * sizeof(PropPartInstance)` -- both structs are
        //    pointer-free today, but the sizes must not be spelled as console immediates.
        //    Prepare stores to EXACTLY four `this` offsets and no others (grepped its 126
        //    instructions): +0x80 mUsedProps, +0x90 mUsedParts, +0x7C mpaPropInstances,
        //    +0x8C mpaPartInstances. ⚠️ IT DOES NOT WRITE +0x88 muNumberOfPropInstances OR
        //    +0x98 muNumberOfPartInstances, and no other PropManager function that HAS a
        //    per-address export writes them either (38 exports scanned, ZERO hits).
        //    ⚠️ SCOPE OF THAT SCAN, tightened round 3 -- it is not the universal claim this
        //    line used to make ("and neither does anything else"): the three export holes
        //    listed further down (RemoveAllPropsAndParts @0x8260F010,
        //    ProcessRemovePropInstanceEvents @0x82627778, AddContactResultsToQueue
        //    @0x82612F08) have no JSON to scan, and a writer OUTSIDE this class (e.g. an
        //    initialiser inlined into PhysicsModule) was not searched for. What is measured is
        //    that the 38 scannable exports contain no such store -- which is already enough to
        //    say the two counters Release() and Destruct() loop on have no producer that this
        //    wave could find. Stated, not smoothed over; see PropManager_wQ_01.cpp.
        //
        // ⚠️ DWARF/TREE PARAMETER-TYPE DELTA, deliberately NOT changed here -- conductor call.
        //    The DecFIGS DWARF declares `bool Prepare(rw::LinearResourceAllocator*)`
        //    (dwarfdump BrnPropManager.h:128 and the .cpp scope at source :174); the tree
        //    declares the base `rw::IResourceAllocator*`. Both bind the one real call site,
        //    BrnPhysicsModule.cpp:369 `mPropManager.Prepare(lpAllocatorList->
        //    GetRWLinearResourceAllocator(23))`, which already hands over a
        //    `rw::LinearResourceAllocator*` (BrnGameDataAllocatorList.h:60) -- so narrowing the
        //    declaration to the DWARF type is a strict improvement AND a two-file edit: the gate
        //    definition at WorldLinkStubs.cpp:516 spells `struct rw::IResourceAllocator *` and
        //    would stop compiling the moment this line changes alone. WorldLinkStubs.cpp is not
        //    this TU's to edit, so the pair is left for the conductor to land together.
        //    (The parameter NAME is separable from the type and was corrected on its own in
        //    round 3: DWARF BrnPropManager.cpp source :174 spells it `lpPhysicsAllocator`.
        //    Name-only, and WorldLinkStubs.cpp:516 spells the definition with no parameter
        //    name at all, so nothing moves.)
        bool Prepare( rw::IResourceAllocator* lpPhysicsAllocator );

        // ==========================================================================
        // ⭐ ADDED 2026-08-09 (conductor wave -- PhysicsModule::Update @0x825B0640's
        // prop legs). Signatures per the PS3 DecFIGS mangles (0x77F694 names
        // OutputUpdatedProps; the generation pair and ReadUpdatedBodies carry their
        // param lists in the same export set). ⚠ FLAG: DECLARED for the conductor's
        // closure; all four bodies are LOUD one-shot gates in
        // BrnPhysicsConductorGates.cpp until reconstructed.
        // ==========================================================================
        void BeginPropWorldContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpCollisionGenerator,
            CgsMemory::LinearMalloc* lpLinearMalloc,
            VecFloat lvfTimeStep );                              // @0x82628CB0

        void EndPropWorldContactGeneration(
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpCollisionGenerator,
            CgsSceneManager::EntityId lWorldEntityId );          // @0x82628E18

        // ⭐ PARAMETER NAMES CORRECTED 2026-08-18 (round 3) to the DecFIGS DWARF's own spellings
        // (dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp, source :962):
        // lpUpdatedBodyQueue -> lpUpdatedBodies, lpSceneInterface -> lpSceneInput,
        // lpSimInputBuffer -> lpSimModuleInputBuffer. The old three were the 2026-08-09
        // conductor wave's, written before the DWARF was read; round 2's rename pass covered
        // the eighteen catch-all-family signatures and this one sits in the earlier block, so
        // it was missed. NAME-ONLY: no type, order, count or const change, so no caller moves,
        // and the parked body already spells the DWARF names.
        // (The DWARF also prints the 4th parameter `const VecFloat` -- a top-level const on a
        // by-value parameter is not part of the signature and is correctly dropped tree-wide.)
        void ReadUpdatedBodies(
            const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
            VecFloat lvfTimeStep );                              // @0x82632918

        // @0x82627EC8. FOURTEEN instructions, and the whole body is two calls -- MEASURED:
        //   `mr r3,r4 ; bl PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface`
        //   `addi r4, r31, 0x680 ; bl PropOutputInterface::AppendUpdatedProps`
        // i.e. `lpOutput->GetPropManagerOutputInterface()->AppendUpdatedProps(mUpdatedProps);`
        // (+0x680 == mUpdatedProps). Parameter name is the DWARF's (source :943).
        //
        // ⛔ BLOCKED -- CORRECTED 2026-08-18 (round 3). This block used to end "Both callees are
        // HOMED ... so this one has no blocker at all". The ACCESSOR is homed; its RETURN TYPE
        // is not. `PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface()` returns the
        // opaque placeholder `struct PropOutputInterfaceStorage { unsigned char maBytes[1]; }`
        // (BrnPhysicsModuleIO.h:76, returned at :89/:90), NOT PropOutputInterface -- so the
        // one-statement body does not compile. REPRODUCED: selfcheck of
        // scratchpad/waveQ2/probe_physfix/probe_outputupdatedprops.cpp -> STATUS=fail with
        // EXACTLY one diagnostic, C2039 "AppendUpdatedProps is not a member of
        // BrnPhysics::PhysicsModuleIO::OutputBuffer::PropOutputInterfaceStorage".
        // Do NOT reinterpret_cast over the placeholder -- that is a type fork, and the 1-byte
        // stand-in also makes the enclosing OutputBuffer's own layout wrong.
        // The unblock is a foreign-header promotion (the same one the three VEHICLE seats at
        // BrnPhysicsModuleIO.h:74-75 got on 2026-08-09) PLUS re-deriving the
        // `maDeformationPad[148656 - 71793]` expression at :138, whose 71793 bakes in
        // sizeof(the 1-byte placeholder). See PropManager.spec.md REQUEST 8 / physfix.owner.md
        // §5 N1. The body is parked verbatim at
        // scratchpad/waveQ2/parked/PropManager_06_OutputUpdatedProps.cpp; the one-shot gate at
        // BrnPhysicsConductorGates.cpp:507 stays until that lands.
        void OutputUpdatedProps(
            BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput ); // @0x82627EC8

        // ⭐ ADDED 2026-08-10 (create-path wave -- PhysicsModule::PostSceneUpdate @0x825ABC10's
        // prop leg, its fifth call). @0x8263AF30 (209 insns). Drains the prop input interface's
        // add/remove prop-and-part instance queues and re-runs the jointed-prop update; its own
        // callees (ProcessRemovePropInstanceEvents / ProcessRemovePartInstanceEvents /
        // RemoveAllPropsAndParts / ProcessAddPropInstanceEvents / ProcessAddPartInstanceEvents /
        // UpdateJointedProps) are none of them reconstructed. ⚠ FLAG: DECLARED for
        // PostSceneUpdate's closure; body is a LOUD one-shot gate (BrnPhysicsConductorGates.cpp).
        //
        // ⚠️ THE BOOL WAS MIS-NAMED -- CORRECTED 2026-08-18 (round 2). It was `lbNetworkCatchup`
        // with a comment calling it "the network-catchup flag". That was an INTERPRETATION of
        // the caller's expression, and the DWARF contradicts it: the source name (dwarfdump
        // BrnPropManager.cpp, source :299) is **lbSimPaused**. Both facts stand together and
        // both are now stated -- PhysicsModule::PostSceneUpdate @0x825ABCE4 does
        // `clrlwi r23, r28, 31` and passes that in r6 (i.e. `lUpdateSet & 1`), and the callee's
        // ONLY use of the byte is the tail guard at 0x8263B254..0x8263B268:
        // `lbz r11, arg_2F ; cmplwi r11,0 ; bne <skip> ; bl UpdateJointedProps`, i.e.
        // `if (!lbSimPaused) UpdateJointedProps(lpSimModuleInputBuffer);` -- skipping the joint
        // solve when the flag is set, which is what a "paused" flag does and not what a
        // "catch-up" flag would do.
        void ProcessInputsPreScene(
            const BrnPhysics::Props::PropInputInterface* lpInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            bool lbSimPaused,
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer ); // @0x8263AF30

        // @0x826119A0 (116 insns; 573-instruction closure). Arm 2 of
        // PhysicsModule::UpdateCachedPositions @0x8259C370: per live prop, post one
        // InEventUpdateCachedPosition for that prop's triangle-cache slot. Signature from the PS3
        // DWARF (..PropManager19UpdateTriangleCacheEPN15CgsSceneManager14SceneManagerIO18
        // InputBuffer_UpdateE). ⚠ FLAG: DECLARED for UpdateCachedPositions' closure; body is a
        // LOUD one-shot gate (BrnPhysicsConductorGates.cpp) -- props own ZERO triangle-cache
        // slots today, so a gate here drops nothing.
        void UpdateTriangleCache(
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update);

        // ==================================================================================
        // ⭐ ADDED 2026-08-18 (breakable-props keystone wave). The rest of the DWARF's PUBLIC
        // surface that the X360 ledger attests -- every one is either one of this TU's own
        // eleven un-bodied functions or a direct callee of one. Signatures are the DecFIGS
        // DWARF's (BrnPropManager.h line numbers on each), confirmed against the register
        // usage in each body's ARTIST asm; the addresses are ARTIST's.
        //
        // ⚠️ DELIBERATELY OMITTED (DWARF declares them, the X360 ledger does NOT attest them,
        // so by the AGENTS.md gating rule they are PS3-only or fully inlined and are left
        // out rather than guessed): ProcessInputs_Prepare (:140), GetInputInterface (:158 --
        // and note PropManager owns no PropInputInterface member for it to return),
        // GetStaticFriction (:176), GetDynamicFriction (:179).
        // ==================================================================================

        // DWARF :132 / X360 0x825BAC88. Ten instructions: `lwz r11,0x88(r3)` then a
        // count-down loop whose only surviving body is `clrlwi r3,r3,31` -- i.e. the source
        // loop `for(i<muNumberOfPropInstances) lbSuccess &= mpaPropInstances[i].Release();`
        // with a constant-true callee (the DWARF names those two locals at
        // BrnPropManager.cpp:245/246). Returns lbSuccess.
        bool Release();

        // DWARF :136 / X360 0x825E3398. mDebugComponent.Destruct() inlined (its
        // "mpPropManager != NULL" assert bakes BrnPropDebugComponent.cpp:69 and it nulls
        // this+0xC), then a tail `bl BaseCollisionGenerator::Destruct` that is an ICF fold of
        // this class's own empty tail -- see the banner in BrnPropManager.cpp. The DWARF's
        // luIndex local (BrnPropManager.cpp:275) says a per-instance Destruct loop was in the
        // source and the compiler deleted it because PropInstance::Destruct is empty.
        void Destruct();

        // DWARF :151 / X360 0x82631260 (938 insns -- the second-largest body in this class).
        // Re-solve the jointed (leaning/tilting) props, break the joints flagged in
        // mBreakPropJoints, and re-queue their UpdatePropEvents. Register map: r3 = this,
        // r4 = lpSimModuleInputBuffer (spilled at 0x82631278); the walk starts from
        // `addi r10, r22, 0x670` == &mUsedPropJoints.
        void UpdateJointedProps( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer );

        // DWARF :199 / X360 0x826115C0. Look up the scene triangle-cache slot and query
        // radius for one physical prop. Returns false when the prop owns no cache slot.
        // Register truth from UpdateTriangleCache's call site @0x82611A5C:
        // r4 = lEntityId, r5 = sign-extended miPhysicsSlot, r6/r7 = the two out-params.
        // Names are the DWARF's (source :2193) as of 2026-08-18 (round 2). Re-measured
        // @0x826115C0 (247 insns): r3 = this, r4 = lPropEntityId (the `clrlwi r11,r31,22`
        // part-index tripwire), r5 = liPhysicalPropIndex, r6/r7 = the two out-references.
        // ⚠️ The DWARF's per-body scope prints this as returning `void` and prints both
        // out-params as `const &`; the CLASS declaration (dwarfdump BrnPropManager.h:199) is
        // the shape authority and says `bool ...(PropEntityID, int32_t, int32_t&, float32_t&)`.
        // The caller agrees: UpdateTriangleCache @0x82611A60 tests the result with a `beq`
        // early-out. The dumper prints mutable references as `const &` throughout this file
        // (it does the same to `int32_t & liNumJobsAdded` below, which is provably written).
        bool GetTriangleCacheSlotAndRadius( PropEntityID lPropEntityId,
                                            s32          liPhysicalPropIndex,
                                            s32&         liOutCacheSlotIndex,
                                            f32&         lfOutCacheSphereRadius );

        // DWARF :222 / X360 0x82612640 and :226 / X360 0x82612AC8. The box-inertia builders:
        // accumulate every collision volume's AABB (rw::collision::Volume's GetBBox under an
        // identity transform), reduce the accumulated corners to a per-axis HALF-EXTENT, and
        // fold `mass * (1/3) * (d_j^2 + d_k^2)` per axis. GetPropInertia additionally
        // substitutes K_LAMPOST_INERTIA_BOX for the two lamppost graphics ids.
        //
        // ⚠️⚠️ CORRECTED 2026-08-18 (round 2) -- THE TWO NUMBERS THAT USED TO BE HERE WERE BOTH
        // WRONG, and together they made every breakable prop 4x too easy to spin. What is
        // MEASURED, from the raw asm of both bodies:
        //   * `d_i = Max( |lMin_i| , |lMax_i| )`, NOT `lMax_i - lMin_i`. The fold at
        //     0x8261294C..0x82612990 (GetPartInertia 0x82612DD4..0x82612E44) is
        //     `vspltisw v0,-1 ; vslw v0,v0,v0` (== 0x80000000) then `vandc` on BOTH corners --
        //     i.e. clear the sign bit on each -- then `vmaxfp`. Neither body emits a single
        //     `vsubfp` anywhere (grepped: zero hits across both dumps). So `d_i` is a
        //     half-extent measured about the ORIGIN, not a box dimension about the centre.
        //   * the scale is `flt_820065E0 == 0.33333334f` (1/3), NOT 1/12. Loaded at 0x826129F0,
        //     splatted (`vspltw v10,v10,0` @0x82612A38) and multiplied into the three pairwise
        //     sums built at 0x82612A40/0x82612A50/0x82612A5C. Value cross-decoded from seven
        //     unrelated exports whose Hex-Rays renders the same rodata address as the literal
        //     0.33333334 (0x82208FC8 among them, `lfs f0, flt_820065E0` @0x82209098).
        // The two corrections are ONE correction and they self-check against each other:
        // `(m/12) * (2h)^2 == (m/3) * h^2`. Do not "restore" either half on its own.
        //
        // ⚠️ CALLING CONVENTION: both return Vector3 through a HIDDEN POINTER -- the asm is
        // r3 = &result, r4 = this, r5 = lpType (`mr r25,r3 ; mr r29,r5 ; ... stvx128 v0,r0,r25`).
        // Hex-Rays renders that as a three-int signature; it is a normal by-value return here.
        //
        // ⚠️ THE GetBBox DISPATCH IS **NOT** A C++ VIRTUAL CALL. `volume+0x40` is the rwcollision
        // per-TYPE descriptor pointer (`maType`, documented at volume_debug_access.h:16-18, whose
        // own GetType() double-dereferences it); the function pointer sits at descriptor+4. The
        // class body is `u8 maPayload[96]` with no vptr and BrnPhysicsPropTypeData.h:213 asserts
        // `sizeof(rw::collision::Volume) == 96`. Whoever lands the requested GetBBox declaration
        // MUST NOT declare it `virtual` -- that would add a vptr to a static_asserted 96-byte
        // serialised record.
        Vector3 GetPropInertia( const PropTypeData*     lpType );
        Vector3 GetPartInertia( const PropPartTypeData* lpType );

        // DWARF :234 / X360 0x82605E10 (205 insns). Part-instance twin of FindPropIndex:
        // linear-scan the used-PART bit-set (`addi r19, r31, 0x90` == &mUsedParts, the
        // BitArray<30>) and return the slot whose stored PropEntityID matches, else
        // KI_PROP_INDEX_NOT_FOUND. r3 = this, r4 = lEntityId. The trailing `const` is the
        // DWARF's (dwarfdump BrnPropManager.h:234) and the asm does not contradict it: every
        // store in the 205 instructions goes either to the stack frame or to
        // CgsDev::Assert::gpcMessageBuffer (the one non-stack store, `stb r22, 0(r31)` at
        // 0x82605FA8, is on r31 AFTER it was reloaded with that global at 0x82605F88 -- it is
        // the assert-message path, not a write into the object).
        s32 FindPartIndex( PropEntityID lEntityId ) const;

        // DWARF :239 / X360 0x8260F010. Drain every live prop and part back out of the
        // simulation (the PropInputInterface's mbRemoveAllPropsAndParts request).
        void RemoveAllPropsAndParts(
            CgsPhysics::PhysicsSimulationIO::InputBuffer*             lpSimInputBuffer,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*  lpSceneInterface );

        static const s32 KI_PROP_INDEX_NOT_FOUND     = -1;    // DWARF BrnPropManager.h:245

        typedef CgsModule::EventQueue<UpdatePropEvent, 200> UpdatePropEventQueue;
        typedef CgsModule::EventQueue<UpdatePropEvent, 15>  UpdateJointedPropEventQueue;

        PropDebugComponent           mDebugComponent;              // X360 +0x0000  (sizeof 0x48)
        bool                         mbRenderCOM;                  // X360 +0x0048  stb 0
        bool                         mbUseOverides;                // X360 +0x0049  stb 0
        f32                          mfMassOverride;               // X360 +0x004C  = 10.0f
        f32                          mfMaxLeanAngleOverride;       // X360 +0x0050  =  0.0f

        // X360 +0x0054. PropDebugComponent::RenderStats @0x826131E8 reaches it as
        // `addi r3,r11,0x54` into ResourcePtr<T>::operator-> -- identified by that call's baked
        // assert line 0x220 == 544, the non-const operator->'s line in CgsResourcePtr.h -- and
        // then reads word 0 of the header == muNumberOfPropTypes. Construct does NOT touch it.
        CgsResource::ResourcePtr<PropPhysicsDataHeader> mpPhysicsData;

        f32                          mfStaticFriction;             // X360 +0x0074  = 0.3f
        f32                          mfDynamicFriction;            // X360 +0x0078  = 0.6f

        // The instance-array slice. The three query getters (FindPropIndex /
        // HasPropJustBeenRemoved / HasPartJustBeenRemoved) attest each stride/offset:
        // PropInstance stride 112 with mEntityId @+0x60; PropPartInstance stride 64 with
        // mEntityId @+0x30; the two BitArrays read at this+0x80 / this+0x90.
        PropInstance*                mpaPropInstances;             // X360 +0x007C
        CgsContainers::BitArray<15>  mUsedProps;                   // X360 +0x0080  std 0
        u32                          muNumberOfPropInstances;      // X360 +0x0088
        PropPartInstance*            mpaPartInstances;             // X360 +0x008C
        CgsContainers::BitArray<30>  mUsedParts;                   // X360 +0x0090  std 0
        u32                          muNumberOfPartInstances;      // X360 +0x0098

        // The perf-monitor slice. miNumJobsAdded is attested by
        // BeginPropWorldContactGeneration @0x82628CFC (`stw r26,0x9C(r31)`, r26 == 0);
        // mpPrimitiveWithTriangleStream by Construct (`stw r30,0xA0(r31)`) and by that same
        // function storing a stream producer there; the five monitor ids by the two
        // Construct*PerfMonitors bodies, whose own NAMES name the members they zero.
        // miProcessBreakPropPM is written by NEITHER constructor -- a fact of the shipped
        // image, stated rather than smoothed over.
        s32                          miNumJobsAdded;               // X360 +0x009C
        CgsMemory::SimpleDataStreamProducer* mpPrimitiveWithTriangleStream;  // X360 +0x00A0
        s32                          miContactGeneratorWaitPM;     // X360 +0x00A4
        s32                          miProcessRemovePropPM;        // X360 +0x00A8
        s32                          miProcessRemovePartPM;        // X360 +0x00AC
        s32                          miProcessAddPropInstancePM;   // X360 +0x00B0
        s32                          miProcessAddPartInstancePM;   // X360 +0x00B4
        s32                          miProcessBreakPropPM;         // X360 +0x00B8

        // The prop-joint block. Untouched by Construct except the two bit-sets; the strides
        // are what close the run from +0xC0 to +0x680 exactly (15*16 + 15*16 + 15 (padded to
        // 16) + 15*64 == 0x5B0, i.e. 0xC0 + 0x5B0 == 0x670).
        Vector3                      maPropJointPositions[15];     // X360 +0x00C0
        Vector3                      maLastJointRotation[15];      // X360 +0x01B0
        u8                           mauPropIndexForJoint[15];     // X360 +0x02A0
        Matrix44Affine               maCurrentJointTransforms[15]; // X360 +0x02B0
        CgsContainers::BitArray<15>  mUsedPropJoints;              // X360 +0x0670  std 0
        CgsContainers::BitArray<15>  mBreakPropJoints;             // X360 +0x0678  std 0

        // Construct calls EventQueue<UpdatePropEvent,200>::Construct on this+0x680 and
        // EventQueue<UpdatePropEvent,15>::Construct on this+0x5E10. The difference,
        // 0x5790 == 0x10 + 200*112, and 0x64B0 - 0x5E10 == 0x6A0 == 0x10 + 15*112, are the
        // two arithmetic self-checks on the already-committed sizeof(UpdatePropEvent) == 112.
        UpdatePropEventQueue         mUpdatedProps;                // X360 +0x0680
        UpdateJointedPropEventQueue  mUpdatedJointedProps;         // X360 +0x5E10

        DebugWorldContactInfo*       mpDebugWorldContacts;         // X360 +0x64B0
        s32                          miNumDebugWorldContacts;      // X360 +0x64B4  stw 0
        bool                         mbDisableFreezing;            // X360 +0x64B8  stb 0
        PropEntityID                 maPropsAddedToContactGen[45]; // X360 +0x64BC  (45*4 == 0xB4)
        s32                          miNumPropsAddedToContactGen;  // X360 +0x6570  stw 0

        // X360 0x82627390 (82 asm). Defined in BrnPropManager.cpp.
        void Construct();

        // X360 0x825BAC60. Four instructions: `li r11,0 ; stw r11,0xA4(r3) ; blr`.
        void ConstructContactGenerationPerfMonitors();

        // X360 0x825BAC70. Six instructions: zero the four pre-scene process-event monitor ids.
        void ConstructPreScenePerfMonitors();

        // X360 0x825BACB0 (private prop/race-car helper; DWARF BrnPropManager.h:311).
        // Retargets the RACE-CAR side of a prop/race-car potential contact onto the shared
        // "dummy" race car by overwriting that RigidBodyId's EntityId owner-type with the
        // dummy-car owner (11). Does not touch PropManager state (no `this` use). Defined
        // out-of-line in BrnPropManager_RoutePropVsRaceCarContactToDummyCar.cpp.
        void RoutePropVsRaceCarContactToDummyCar(
            bool                                             lbPropIsEntityA,
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact );

        // ⭐ ADDED 2026-08-06 (bridge de-facade wave). DWARF BrnPropManager.h:172; X360 emission
        // @0x825A53A0 (bl target of PhysicsModule::StoreContact @0x825A5FA0 -- the console body
        // was header-inline here, its asserts bake BrnPropManager.h:529/530/533/534/550/563).
        // Build a ContactSpy::PropContact from a resolved prop spy + its potential contact:
        // BaseContact::Construct, entity word/type/state/flags from the prop or prop-part
        // instance tables, and the smash-gate / billboard graphics-id flag bits. Defined in
        // BrnPropManager.cpp.
        void CreateContactEvent( ContactSpy::PropContact* lpOutPropContact,
                                 const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
                                 const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact );

        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). @0x82628190 (PS3 DecFIGS
        // 0x79008C -- the mangle is the signature authority). Validate + set up one prop-vs-X
        // potential contact for the simulation (called by PhysicsModule::
        // BridgeContactsToSimulation when either owner is a PROP); returns false to drop the
        // contact.
        // ⭐⭐ BODY LANDED 2026-08-18 (wave Q4) in the sibling partfile PropManager_wQ4_01.cpp,
        // all 573 instructions; the trap stub in BrnPropManager.cpp is DELETED. The old FLAG on
        // this declaration ("body still a TRAP STUB ... 572 asm lines / 24 callees") is retired.
        // ⭐ PARAMETER NAMES CORRECTED in the same wave to the DecFIGS DWARF's own spellings
        // (dwarfdump BrnPropManager.cpp, source line :1298): lpAddContactEvent -> lpOutContact,
        // lpPotentialContact -> lpInPotentialContact, lpSimModuleInputBuffer -> lpSimInputBuffer,
        // lbFrozen -> lbOtherEntityIsFrozen. NAME-ONLY -- no type, order or count changed, so no
        // call site moved.
        // ⚠️ lpPropRaceCarContactBuffer AND lWorldRigidBodyId ARE DEAD IN THE ARTIST EMISSION:
        // neither r8 nor r9 is copied in the prologue and neither is read in the 573 instructions
        // (both are later reused as scratch, which is the compiler proving to itself they were
        // dead). They are KEPT because the DWARF declares them and the asm cannot disprove an
        // unused parameter -- flagged so nobody "discovers" the drop later and deletes them.
        // What it means for the seam: this function does NOT write the PropRaceCarContactBuffer.
        bool SetupAndValidatePropContact(
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*        lpOutContact,
            const CgsSceneManager::SceneManagerIO::PotentialContact*       lpInPotentialContact,
            BrnPhysics::Vehicle::VehicleManager*                           lpVehicleManager,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*                  lpSimInputBuffer,
            PropRaceCarContactBuffer*                                      lpPropRaceCarContactBuffer,
            CgsPhysics::RigidBodyId                                        lWorldRigidBodyId,
            bool                                                           lbOtherEntityIsFrozen,
            f32                                                            lfTimeStep );

        // X360 0x82606148 (DWARF BrnPropManager.h:230 -- corrected round 3; :250 is the
        // dumpfile's `bool mbUseOverides`, and the DWARF's own decl-line comment above this
        // declaration reads "// BrnPropManager.h:230"). Linear-scan the used-prop bit-set;
        // return the slot whose stored PropEntityID matches, else -1.
        int32_t FindPropIndex( PropEntityID lEntityId ) const;

        // X360 0x825DEAC0 (DWARF :314). True iff the prop slot is now free OR holds a
        // different entity than lEntityId (i.e. the prop was removed/recycled this frame).
        bool HasPropJustBeenRemoved( PropEntityID lEntityId, int32_t liPropIndex );

        // X360 0x825DE930 (DWARF :317). Part-instance analogue of HasPropJustBeenRemoved.
        bool HasPartJustBeenRemoved( PropEntityID lEntityId, int32_t liPartIndex );

        // ==================================================================================
        // ⭐ ADDED 2026-08-18 (breakable-props keystone wave) -- THE BREAK PIPELINE'S INTERNAL
        // HELPER BLOCK. The DecFIGS DWARF declares all of these PRIVATE (BrnPropManager.h
        // :313..:461); they are left PUBLIC here because every other member of this class
        // already is (the committed header made the whole layout public so the sibling
        // partfiles and the debug component can reach it) and flipping the access now would
        // be a behaviour-neutral change with a real chance of breaking a consumer. The DWARF
        // access is recorded in this comment instead of enforced.
        //
        // WHY THEY ARE HERE: with the exception of the two Handle* leaners, every one of them
        // is a direct callee of one of this TU's eleven un-bodied functions, and NONE of them
        // had a declaration anywhere in the tree -- which is the whole reason those eleven
        // were parked. Signatures are the DWARF's, each checked against the register usage of
        // its own ARTIST body (and, for the two that no per-address JSON covers, against a
        // headless IDA 9.3 read of IDA Files/BURNOUT_X360_ARTIST.XEX.i64).
        //
        // ⚠️ NONE OF THESE BODIES EXISTS IN THE TREE. They are declaration-only, so `cl /c`
        // is green while the eventual link is not -- which is the intended, greppable state:
        // each one is its own ledger function under a DIFFERENT TU (mostly the
        // GameShared/GameClasses/Development/CgsStrStream.h catch-all; ClampAcceleration
        // under the rw/math/fpu/vector3.h catch-all -- see the spec). Do NOT "helpfully"
        // stub any of them.
        //
        // ==================================================================================
        // ⭐ ROUND 2, 2026-08-18 -- THE CATCH-ALL FAMILY IS NOW PINNED. Every signature in this
        // block (plus Prepare, FindPartIndex, OutputUpdatedProps, UpdateJointedProps and
        // ProcessInputsPreScene above) was re-derived this round against BOTH the DecFIGS DWARF
        // and the register setup of its own ARTIST body, and the per-function evidence table --
        // address, exact declaration, DWARF line, the locals the DecFIGS scope names, the
        // callees and their homes, and the accessors a body will need -- lives in
        //     scratchpad/waveQ2/physfix.owner.md
        // Read that BEFORE writing any of these bodies; it is the file that says which
        // callees are declared-but-un-bodied and which are still un-homed.
        // What changed here: PARAMETER NAMES ONLY (to the DWARF's), plus the corrected
        // GetPropInertia fold, the corrected Prepare address, and the ProcessInputsPreScene
        // bool. No type, order, count or const-ness anywhere in this class changed, so no
        // caller was regated.
        //
        // ⚠️ THREE OF THIS FAMILY ARE EXPORT HOLES -- ALL THREE lack BOTH a .ida-exports
        // per-address JSON AND a progress/identity.json row, so all three are invisible to
        // `work show`/coverage tooling, not one. (CORRECTED round 3: the "(also has NO
        // identity.json row)" parenthesis used to hang off AddContactResultsToQueue alone,
        // which read as an assurance that the other two DO have ledger rows. They do not --
        // every `BrnPhysics::Props::PropManager::*` key in identity.json was enumerated and
        // none of the three appears; the only namesakes are BrnWorld::PropZoneManager::
        // RemoveAllPropsAndParts @0x822DEF50 and BrnPhysics::Vehicle::VehicleManager::
        // AddContactResultsToQueue @0x825EB350.) They are real; their names and addresses come
        // from the xrefs_from of their callers:
        //   RemoveAllPropsAndParts          @0x8260F010  (named in ProcessInputsPreScene's xrefs)
        //   ProcessRemovePropInstanceEvents @0x82627778  (ditto)
        //   AddContactResultsToQueue        @0x82612F08  (read with headless IDA; see its own
        //                                                 block below for the measured span)
        // ==================================================================================

        // :313 / X360 0x826274D8. Build the InAddRigidBody event for one whole prop: inertia
        // from GetPropInertia x KVF_INERTIA_SCALE, the five KF_PROP_* drag/limit scalars, the
        // static-vs-dynamic mass reciprocal (mbUseOverides picks mfMassOverride), and -- when
        // the second bool is set -- the K_PROP_EXTRA_COM_OFFSET shift of the body origin.
        // REGISTER MAP (measured @0x826274D8): r3 = this, r4 = lEntityId (the `srwi r11,r31,24 ;
        // cmplwi 3` owner tripwire), r5 = liPropIndex, r6 = &lTransform (the by-value
        // Matrix44Affine rides a hidden reference), r7 = lpType, r8 = lbStatic,
        // r9 = lbAddExtraComOffset, r10 = lpSimModuleInputBuffer, v1/v2 = the two Vector3s
        // (which consume NO GPR slot -- gotcha 3's vector twin). Names are the DWARF's
        // (source :568); `lbIsStatic` was renamed to `lbStatic` 2026-08-18 (round 2).
        void AddPropToSim( PropEntityID                                  lEntityId,
                           s32                                           liPropIndex,
                           Matrix44Affine                                lTransform,
                           const PropTypeData*                           lpType,
                           bool                                          lbStatic,
                           bool                                          lbAddExtraComOffset,
                           CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                           Vector3                                       lLinearVelocity,
                           Vector3                                       lAngularVelocity );

        // :320 / X360 0x8260F540 (273 insns)  and  :328 / X360 0x8260F988 (117 insns). Retire
        // one prop / one prop part: post the InRemoveRigidBody, drop the scene volume, free the
        // slot bit. Names are the DWARF's (source :760 / :803) as of 2026-08-18 (round 2).
        // Both index parameters are UNSIGNED in the DWARF and the asm agrees -- the bound tests
        // are `cmplwi r31, 0xF` (RemoveProp @0x8260F584, KU_MAX_PHYSICAL_PROPS) and
        // `cmplwi r29, 0x1E` (RemovePart @0x8260F9C0, KU_MAX_PHYSICAL_PROP_PARTS), both the
        // unsigned compare.
        void RemoveProp( PropEntityID                                             lEntityId,
                         u32                                                      luPropIndex,
                         CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                         CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );
        void RemovePart( PropEntityID                                             lEntityId,
                         u32                                                      luPartIndex,
                         CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                         CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );

        // :336 / X360 0x82628A88. THE BREAK ITSELF for a jointed prop (the smash gate / sign
        // post): cut the joint, hand the freed body KVF_BREAK_JOINT_LINEAR_VEL /
        // KVF_BREAK_JOINT_ANGULAR_VEL, and re-add it as a free rigid body.
        // REGISTER MAP (measured @0x82628A88, 137 insns): r3 = this, r4 = lEntityId (same
        // `srwi r11,r25,24 ; cmplwi 3` owner tripwire, assert BrnPropEntityID.h:278),
        // r5 = liPropIndex (its low halfword is OR'd into the 64-bit RigidBodyId at
        // 0x82628AD8..0x82628AE4), r6 = &lTransform, r7 = lpType, r8 = lpSimModuleInputBuffer.
        void BreakJoint( PropEntityID                                  lEntityId,
                         s32                                           liPropIndex,
                         Matrix44Affine                                lTransform,
                         const PropTypeData*                           lpType,
                         CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer );

        // :347 / X360 0x826278D0. THE PART SPAWN -- a smashed prop's shed panel becoming its
        // own physical body. ProcessAddPartInstanceEvents @0x826280F8 is its only caller and
        // its register map is the signature's proof: r4 = event.mEntityId (+0x40),
        // r5 = (s16)event.miPropTypeId (+0x44), r6 = event.miPartId (+0x46),
        // r7 = &event.mTransform (the Matrix44Affine rides by hidden reference, and the event
        // starts with it), v1/v2 = two ZERO Vector3s (the initial velocities),
        // r8 = (s16)event.miSlot (+0x48), r9/r10 = the scene interface and sim input buffer.
        // Names are the DWARF's (source :837) as of 2026-08-18 (round 2): luPropTypeId ->
        // luPropTypeIndex, li16PartId -> li16PartIndex, liSlot -> liSlotIndex.
        void CreatePart( PropEntityID                                             lEntityId,
                         u32                                                      luPropTypeIndex,
                         s16                                                      li16PartIndex,
                         Matrix44Affine                                           lTransform,
                         Vector3                                                  lLinearVelocity,
                         Vector3                                                  lAngularVelocity,
                         s32                                                      liSlotIndex,
                         CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                         CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );

        // :359 / X360 0x82627F00. ⭐ THIS TU'S EXTRA LEDGER FUNCTION (identity.json attributes
        // it to the SDKs/EATech/include/rw/math/fpu/vector3.h catch-all -- an inlining
        // artefact; it is this class's own helper and is bodied in a partfile of this class).
        // Clamp the frame's linear+angular acceleration of one updated body and, if either
        // was clamped, post a corrected InUpdateRigidBody. Register truth from the asm:
        // r3 = this, v1/v2 = the two previous velocities, r4 = the 64-bit RigidBodyId,
        // r5 = the OutUpdateRigidBody (the body itself is at +0x10), r6/r7 = the two in/out
        // velocity references, v3 = the timestep, v4 = 1/timestep, r8 = the sim input buffer.
        // ⭐ PARAMETER NAMES CORRECTED 2026-08-18 (round 2) to the DWARF's own spellings
        // (dwarfdump BrnPropManager.cpp, source line :1188). NAME-ONLY, no signature change:
        // lLastLinearVelocity -> lLinearVelocity, lLastAngularVelocity -> lAngularVelocity,
        // lpUpdatedBody -> lpUpdateBodyEvent, lrLinearVelocity -> lUpdatedLinearVelocity,
        // lrAngularVelocity -> lUpdatedAngularVelocity, lvfInvTimeStep -> lvfOneOverTimeStep.
        // (The parked body in scratchpad/waveQ/parked/PropManager_05_ClampAcceleration.cpp
        // already used the DWARF names, so declaration and definition now agree.)
        void ClampAcceleration( Vector3                                                lLinearVelocity,
                                Vector3                                                lAngularVelocity,
                                CgsPhysics::RigidBodyId                                lRigidBodyId,
                                const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody* lpUpdateBodyEvent,
                                Vector3&                                               lUpdatedLinearVelocity,
                                Vector3&                                               lUpdatedAngularVelocity,
                                VecFloat                                               lvfTimeStep,
                                VecFloat                                               lvfOneOverTimeStep,
                                CgsPhysics::PhysicsSimulationIO::InputBuffer*          lpSimModuleInputBuffer );

        // :366 / X360 0x82632108, :372 / X360 0x826280F8, :378 / X360 0x82627778,
        // :384 / X360 0x82627818. The four input-queue drains ProcessInputsPreScene runs, in
        // that order (after RemoveAllPropsAndParts). Each walks one of PropInputInterface's
        // four embedded EventQueues via the accessors added to BrnPropInputInterface.h in
        // this same wave.
        // Names are the DWARF's (source :470 / :1246 / :~64x / :711) as of round 2.
        // ⚠️ ProcessRemovePropInstanceEvents @0x82627778 is an EXPORT HOLE (no per-address JSON);
        // its address and name come from ProcessInputsPreScene's xrefs_from.
        void ProcessAddPropInstanceEvents(
            const PropInputInterface*                                lpInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );
        void ProcessAddPartInstanceEvents(
            const PropInputInterface*                                lpInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );
        void ProcessRemovePropInstanceEvents(
            const PropInputInterface*                                lpInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );
        void ProcessRemovePartInstanceEvents(
            const PropInputInterface*                                lpInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer );

        // :395 / X360 0x82611B70 and :404 / X360 0x826120E8. The per-UpdatePropEvent halves
        // BeginPropWorldContactGeneration dispatches on (part id != 0 picks the part one).
        // The s32& is miNumJobsAdded, passed by reference so each posted job bumps it -- the
        // asm hands `r7 = this + 0x9C`, and EndPropWorldContactGeneration then asserts it
        // equals the generator's GetNumUsedResultLists().
        // REGISTER MAP, identical for both (measured @0x82611B70 / @0x826120E8): r3 = this,
        // r4 = lpContactGenerator (null-asserted immediately), r5 = lpTriCache,
        // r6 = &lUpdate{Part,Prop}Event, r7 = &liNumJobsAdded, r8 = lpMalloc, v1 = lvfTimeStep
        // (a vector arg: it consumes no GPR slot, so its POSITION is DWARF-attested, not
        // register-attested). Names are the DWARF's (source :2444 / :2569) as of round 2.
        // ⚠️ TYPE-NAME DELTA, deliberate: the DWARF spells parameter 2
        // `const VehicleInputInterface::InTriangleCacheInterface*`. The tree's single home for
        // that interface is CgsSceneManager::SceneManagerIO::TriangleCacheInterface
        // (CgsSceneManagerIO_TriangleCache.h:23) and the committed siblings BrnVehicleManager.h
        // / BrnPhysicalTrafficManager.h already forward-declare exactly that spelling for the
        // same parameter, so the tree name is kept rather than forking a second interface type.
        void DoPartWorldContactGeneration(
            CgsSceneManager::CgsCollision::CollisionGenerator*                 lpContactGenerator,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*     lpTriCache,
            const UpdatePropEvent&                                             lUpdatePartEvent,
            s32&                                                               liNumJobsAdded,
            CgsMemory::LinearMalloc*                                           lpMalloc,
            VecFloat                                                           lvfTimeStep );
        void DoPropInstanceWorldContactGeneration(
            CgsSceneManager::CgsCollision::CollisionGenerator*                 lpContactGenerator,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*     lpTriCache,
            const UpdatePropEvent&                                             lUpdatePropEvent,
            s32&                                                               liNumJobsAdded,
            CgsMemory::LinearMalloc*                                           lpMalloc,
            VecFloat                                                           lvfTimeStep );

        // :410 / X360 0x82612F08. Turn the finished collision generator's result lists into
        // potential contacts on the physics module's contact interface -- the tail
        // EndPropWorldContactGeneration tail-calls.
        // ⚠️ 0x82612F08 has NO per-address JSON in .ida-exports and no identity.json row (a
        // double export hole). Its existence, name and extent were read out of the .i64 with
        // headless IDA 9.3; the argument map is EndPropWorldContactGeneration's own call site
        // @0x82628E8C (r4/r5/r6).
        // ⚠️ COUNT CORRECTED 2026-08-18 (round 3). This line used to read
        // "span (82612F08-826131E8, 736 insns)". 736 is the BYTE length (0x2E0); the
        // INSTRUCTION count is 184. Re-measured myself in this checkout with headless IDA
        // (scratchpad/waveQ2/probe_fixer/dump_globals.py): start_ea 0x82612F08,
        // end_ea 0x826131E8 (exclusive), last instruction at 0x826131E4, 184 heads, 736 bytes.
        // ⭐ PARAMETER NAMES CORRECTED in the same round to the DWARF's own spellings
        // (dwarfdump BrnPropManager.cpp:2975, source :2805): lpContactInterface ->
        // lpPotentialContactInterface, lpCollisionGenerator -> lpContactGenerator. There is no
        // asm to check the names against, but the DWARF is exactly what supplied the other
        // seventeen names in this block, so it is followed here too. NAME-ONLY.
        // NOTE the landed definition in PropManager_wQ2_03.cpp still spells the two old names
        // (C++ does not require them to match, so nothing breaks); folding it is that file
        // owner's one-line job.
        void AddContactResultsToQueue(
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface,
            CgsSceneManager::CgsCollision::CollisionGenerator*      lpContactGenerator,
            CgsSceneManager::EntityId                               lWorldEntityId );

        // :420 / X360 0x826113F8. ⭐ ONE OF THIS TU'S ELEVEN. Push a prop sideways/upward out
        // of the path of a race car that is herding it, using the five KVF_ANTI_HERD_* /
        // speed gains. Register truth: r3 = this (NEVER READ -- the body overwrites r3 with
        // r4 before its only call), r4 = the sim input buffer, r5 = the race car,
        // r6 = the 64-bit RigidBodyId, v1/v2/v3 = the three vector params.
        // ⚠️ THE DWARF'S SEVENTH PARAMETER (the second trailing Vector3, which would ride v4)
        // IS NEVER READ by the ARTIST body: v4 is overwritten at 0x82611448 before any use.
        // Declared per the DWARF anyway (the asm cannot disprove an unused parameter) and
        // flagged here so nobody "discovers" the drop later and deletes it.
        // ⭐ PARAMETER NAMES CORRECTED 2026-08-18 (round 2) to the DecFIGS DWARF's own spellings
        // (dwarfdump BrnPropManager.cpp, source line :2111): lContactPoint -> lPropWorldPos,
        // lvfScale -> lvfPropMass, lContactNormal -> lPropLinearVelocity, lUnusedInArtist ->
        // lCollisionNormal. NAME-ONLY: no type, order or count change, so no call site moves.
        // The old `lvfScale` for what the source calls a MASS is exactly the kind of name a
        // body author would have multiplied by instead of divided.
        void ApplyAntiHerdingForce(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer,
            BrnPhysics::Vehicle::RaceCarPhysics*          lpRaceCar,
            CgsPhysics::RigidBodyId                       lPropRigidBodyId,
            Vector3                                       lPropWorldPos,
            VecFloat                                      lvfPropMass,
            Vector3                                       lPropLinearVelocity,
            Vector3                                       lCollisionNormal );

        // :428 / X360 0x825E3560 (50 insns). The impulse the CAR takes back from a prop it
        // smashed (accumulated via RaceCarPhysics::AddPropCollisionImpulse).
        // ⚠️ PARAMETER NAMES CORRECTED 2026-08-18 (round 2): the last two were `lImpulse` and
        // `lContactPoint`; the DWARF (source :1685) names them lNormal and lPointOnCar, and the
        // ASM AGREES -- v1 is dotted against the relative velocity (`vmsum3fp128 v13,v13,v1`
        // @0x825E35B4), which only makes sense for a unit NORMAL, and v2 is the point the body
        // subtracts the car origin from (`vsubfp v12, v2, <car transform Pos>` @0x825E3584).
        // Calling the first of those `lImpulse` inverted the meaning of the parameter: the
        // impulse is what this function COMPUTES (DWARF local `Vector3 lImpulse` at :1710),
        // not what it is handed. NAME-ONLY change; the register map is unchanged
        // (r3=this reading mbUseOverides @+0x49, r4=lpRaceCar, r5=lpProp, r6=lpType, v1, v2).
        void ApplyPropRaceCarCollisionImpulse(
            BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar,
            PropInstance*                        lpProp,
            const PropTypeData*                  lpType,
            Vector3                              lNormal,
            Vector3                              lPointOnCar );

        // :442 / X360 0x8260FB60 (854 insns) and :456 / X360 0x826108B8 (720 insns). The two
        // jointed-prop contact resolvers SetupAndValidatePropContact dispatches on (lean vs
        // tilt), keyed by PropTypeData::GetJointType(). Not called by any of this TU's eleven --
        // declared with them because they are the same private block and the same DWARF page,
        // and SetupAndValidatePropContact (the TU's other trap stub) needs both.
        //
        // ⚠️⚠️ THE THREE VECTOR PARAMETER NAMES WERE WRONG AND MISLEADING -- CORRECTED
        // 2026-08-18 (round 2). They read (lContactPoint, lContactNormal, lRelativeVelocity),
        // i.e. they said slot 5 was a POINT and slot 6 was a NORMAL. The DWARF (source :1771 /
        // :1903) names them (lNormal, lPointOnProp, lPointOnCar) -- the normal comes FIRST and
        // there is no relative-velocity parameter at all. An implementer trusting the old names
        // would have swapped the normal and the contact point on every jointed-prop hit.
        //
        // REGISTER MAP (measured from both prologues; note gotcha 3 -- the f32 rides f1 and
        // SKIPS its GPR slot, and the three Vector3s ride v1/v2/v3 consuming no GPR at all).
        // ⚠️ ONE ROW IS WEAKER THAN THE OTHERS AND THE HEADING USED TO HIDE IT (split round 3):
        //   r5 = liPropIndex is REGISTER-ATTESTED IN TILT ONLY (`mr r14,r5` @0x82610930).
        //   LEAN NEVER READS r5 -- I scanned all 854 instructions of 0x8260FB60: every
        //   occurrence is a `li r5, <n>` writing an assert line number (the first is
        //   `li r5,0x146` @0x8260FD4C), and there is no `mr rN,r5` anywhere in its prologue.
        //   So r5's position in the LEAN signature rests on the DWARF, not on the asm.
        //   r3 = this, r4 = lpPropInstance, r5 = liPropIndex, r6 = lpType, r7 = lpRaceCar,
        //   v1 = lNormal, v2 = lPointOnProp, v3 = lPointOnCar,
        //   r8 = lpOutContact (both bodies open with `stfs f0, 0x48(r8)` writing 0.0f into it),
        //   r9 = lbPropIsEntityA, f1 = lfTimeStep.
        // Lean: 0x8260FB80 mr r20,r4 / 0x8260FB98 mr r19,r6 / 0x8260FB88 mr r26,r7 /
        //       0x8260FB8C-98 vmr128 v127,v1 v120,v2 v122,v3 / 0x8260FB7C fmr f31,f1.
        // Tilt: 0x826108E0 mr r23,r4 / 0x8261090C mr r27,r6 / 0x826108E8 mr r30,r7 /
        //       0x826108D8 mr r16,r8 / 0x82610910 mr r15,r9 / 0x826108D4 fmr f31,f1.
        // So the ORDER in the declaration was already right; only the names were wrong.
        void HandleContactWithLeanProp(
            PropInstance*                                                  lpPropInstance,
            s32                                                            liPropIndex,
            const PropTypeData*                                            lpType,
            BrnPhysics::Vehicle::RaceCarPhysics*                           lpRaceCar,
            Vector3                                                        lNormal,
            Vector3                                                        lPointOnProp,
            Vector3                                                        lPointOnCar,
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*        lpOutContact,
            bool                                                           lbPropIsEntityA,
            f32                                                            lfTimeStep );
        void HandleContactWithTiltProp(
            PropInstance*                                                  lpPropInstance,
            s32                                                            liPropIndex,
            const PropTypeData*                                            lpType,
            BrnPhysics::Vehicle::RaceCarPhysics*                           lpRaceCar,
            Vector3                                                        lNormal,
            Vector3                                                        lPointOnProp,
            Vector3                                                        lPointOnCar,
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*        lpOutContact,
            bool                                                           lbPropIsEntityA,
            f32                                                            lfTimeStep );
    };
}
}
