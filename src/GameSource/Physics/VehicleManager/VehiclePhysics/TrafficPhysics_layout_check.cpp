// Layout check for the BrnPhysics::Vehicle::TrafficPhysics OWN-MEMBER BLOCK (X360 0x13F0..0x1430).
//
// WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// Until 2026-08-03 the 0x1430 stride of `PhysicalTrafficManager::maFullTrafficPhysics[20]` was
// "checked" by `static_assert(sizeof(TrafficPhysics) == 5168)` in BrnVehicleManager_layout_check.cpp
// -- a HOST sizeof gate on a byte-pinned `u8[5168]` STAND-IN. It asserted that the stand-in was
// still 5168 bytes of nothing. Now that the member is the real class that assert is meaningless (the
// host class is 4960), and the console 0x1430 has to be checked where it is actually derived: as
// arithmetic over the recovered member seats. That is what this file does.
//
// WHY THE GATE IS ARITHMETIC AND NOT `offsetof` -- the same argument
// VehiclePhysics_layout_check.cpp makes one level down, and for the same reason: the block sits on
// top of VehiclePhysics, which owns two pointers and several minimal-slice sub-objects, so the host
// puts this block at 0x1320 rather than the console's 0x13F0. An absolute `offsetof` gate here would
// be false. What IS checkable, and what this wave actually recovered, is that walking the DWARF
// member ORDER (TrafficPhysics.h:117-122) with the host-checkable member SIZES from the base anchor
// reproduces EVERY independently asm-literal seat and lands exactly on 0x1430.
//
// The five anchors on the right-hand side, none of them computed from the left:
//     0x13F0  == X360Layout::KU_VP_SIZEOF, where VehiclePhysics's own block closes -- derived by a
//                DIFFERENT wave from DIFFERENT functions, and asserted by its own layout check.
//     0x13F4  SetFreakedOut @0x825B8948  `stb  r11, 0x13F4(r3)`
//     0x13F8  SetFreakedOut @0x825B8948  `stfs f1,  0x13F8(r3)`
//     0x1400  Construct     @0x8262E980  `addi r11, r31, 0x1400`, then `stw r7, 0(r11)`
//     0x1420  Construct     @0x8262E980  `std  r8,  0x20(r11)`   (mRandom.muSeed)
//     0x1428  Construct     @0x8262E980  `stw  r30, 0x28(r11)`   (mRandom.muOldestBufferIndex)
//     0x1430  PhysicalTrafficManager::Construct @0x82636CA8 `mulli r11, r29, 0x1430`, x20
// Two of them (0x1420/0x1428) are INSIDE mRandom, so the gate also pins that this member really is
// a 48-byte CgsNumeric::Random and not the 4-byte local `struct Random { u32 muState; }` this
// header forked until 2026-08-03 -- the fork that made the class close on 0x1404.
//
// THE BLIND SPOT, stated rather than hidden: the arithmetic cannot see a member that occupies no
// space. Between mu8FreakOutState (0x13F4) and mfFreakOutDirection (0x13F8) there are three bytes of
// alignment pad, so a byte added in that hole moves nothing. Between mfFreakOutTime (0x13FC) and
// mRandom (0x1400) there is NO slack, which PART 1 asserts explicitly, so the same trick does not
// work anywhere else in the block. What covers the one hole is PART 2's named-member ORDER checks
// plus the per-member `sizeof` asserts -- a member cannot be deleted, renamed, reordered or
// re-widened silently, only ADDED, and only there.
//
// TAMPER-TESTED 2026-08-03, EIGHT cases, **seven fire**:
//   FIRES  mRandom typed back to a 4-byte `struct Random { u32 muState; }`   (closure + tail check)
//   FIRES  mOwnerID widened to u64                                           (state seat + width)
//   FIRES  mu8FreakOutState changed to u32                                   (width assert)
//   FIRES  swap mfFreakOutDirection / mfFreakOutTime                         (PART 2 order check)
//   FIRES  delete mfFreakOutTime                                             (mRandom lands 0x13FC)
//   FIRES  CgsNumeric::KU_FLOAT_BUFFER_SIZE 8 -> 4                           (seed seat + closure)
//   FIRES  a second Random declared ahead of the two timers                  (block size 64 -> 112)
//   SILENT insert a bool between mu8FreakOutState and mfFreakOutDirection    (the 3 bytes of pad)
// Case 3 was SILENT on the first run and is the reason the four `sizeof(member)` asserts exist in
// PART 2: u8-plus-3-bytes-of-pad and u32 occupy the same space, so widening the state byte was
// invisible to every offset in the file even though the console plainly uses `stb`/`lbz`. The gate
// was not finished until a case that had passed was made to fail.

#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"

#include <cstddef>   // offsetof, size_t

namespace BrnPhysics
{
namespace Vehicle
{
    // ==============================================================================================
    // PART 1 -- the CONSOLE arithmetic. Pure integer maths over the X360Layout literals plus the
    // host-checkable sizes of the pointer-free members; it does not use a single host offsetof, so
    // it measures the RECOVERED LAYOUT and nothing else.
    // ==============================================================================================
    namespace TrafficPhysicsX360LayoutCheck
    {
        using namespace X360Layout;

        static_assert(KU_TP_OWN_BASE == KU_VP_SIZEOF,
                      "TP: the own block must start exactly where VehiclePhysics's closes (0x13F0) -- "
                      "an anchor a different wave derived from different functions");

        // DWARF TrafficPhysics.h:117 -- EntityId, one 32-bit packed handle on both platforms.
        const unsigned KU_A_OWNERID       = KU_TP_OWN_BASE;                       // :117  0x13F0
        const unsigned KU_A_FREAKOUTSTATE = KU_A_OWNERID + 4u;                    // :118
        static_assert(KU_A_FREAKOUTSTATE == KU_TP_FREAKOUTSTATE_OFF,
                      "TP: mOwnerID (EntityId, 4) must land mu8FreakOutState on the asm-literal "
                      "0x13F4 (SetFreakedOut `stb r11, 0x13F4(r3)`)");

        // :118 is one byte; :119 is an f32, so it 4-aligns.
        const unsigned KU_A_FREAKOUTDIR   = ((KU_A_FREAKOUTSTATE + 1u) + 3u) & ~3u;   // :119
        static_assert(KU_A_FREAKOUTDIR == KU_TP_FREAKOUTDIR_OFF,
                      "TP: the u8 state plus 4-alignment must land mfFreakOutDirection on the "
                      "asm-literal 0x13F8 (SetFreakedOut `stfs f1, 0x13F8(r3)`)");

        const unsigned KU_A_FREAKOUTTIME  = KU_A_FREAKOUTDIR + 4u;                // :120
        // :122 is CgsNumeric::Random, which is 16-ALIGNED (its DWARF union carries a
        // VectorIntrinsic[2]) -- so it rounds up, and on the console it happens to need no pad.
        const unsigned KU_A_RANDOM        = ((KU_A_FREAKOUTTIME + 4u) + 15u) & ~15u;  // :122
        static_assert(KU_A_RANDOM == KU_TP_RANDOM_OFF,
                      "TP: the two f32 timers plus Random's 16-alignment must land mRandom on the "
                      "asm-literal 0x1400 (Construct `addi r11, r31, 0x1400`, then `stw <1.0f>, 0`)");
        static_assert(KU_A_RANDOM == KU_A_FREAKOUTTIME + 4u,
                      "TP: ...and with ZERO alignment slack -- 0x13FC + 4 is already 16-aligned, so "
                      "there is no room here for an unmodelled member");

        // ---- inside CgsNumeric::Random (CgsRandom.h:179-183): the ring, then the seed, then the
        // ---- index. Both interior seats are asm literals of TrafficPhysics::Construct.
        const unsigned KU_A_RANDOM_SEED  = KU_A_RANDOM
                                         + ((CgsNumeric::KU_FLOAT_BUFFER_SIZE * 4u) + 7u) / 8u * 8u;
        static_assert(KU_A_RANDOM_SEED == KU_TP_RANDOM_SEED_OFF,
                      "TP: the 8-slot float ring (8 * 4 == 32, and muSeed is 8-aligned) must land "
                      "muSeed on the asm-literal 0x1420 (Construct `std r8, 0x20(r11)`; Update "
                      "@0x82639824 reads the same 0x1420 to step the LCG)");

        const unsigned KU_A_RANDOM_INDEX = KU_A_RANDOM_SEED + 8u;
        static_assert(KU_A_RANDOM_INDEX == KU_TP_RANDOM_INDEX_OFF,
                      "TP: the u64 seed must land muOldestBufferIndex on the asm-literal 0x1428 "
                      "(Construct `stw r30, 0x28(r11)`)");

        // THE CLOSURE. muOldestBufferIndex's last data byte is 0x142B; Random is 16-aligned so
        // it rounds to 0x1430 -- which is the per-element stride PhysicalTrafficManager::Construct
        // bakes as a literal for maFullTrafficPhysics[20]. Nothing in this file knows about that
        // function; it just has to land on it.
        const unsigned KU_A_TP_SIZEOF = (KU_A_RANDOM_INDEX + 4u + 15u) & ~15u;
        static_assert(KU_A_TP_SIZEOF == KU_TP_SIZEOF,
                      "⭐⭐ TP CLOSURE: the own block must end exactly at 0x1430 == the 20-element "
                      "stride `mulli r11, r29, 0x1430` in PhysicalTrafficManager::Construct");
        static_assert(KU_A_TP_SIZEOF - KU_TP_OWN_BASE == 0x40u,
                      "TP: the whole own block is 64 bytes -- 16 of scalars + a 48-byte Random");
    }

    // ==============================================================================================
    // PART 2 -- the HOST-side facts the arithmetic depends on, plus the named-member ORDER checks
    // that cover PART 1's alignment blind spot.
    // ==============================================================================================
    void TrafficPhysics::_AssertOwnBlockLayout()
    {
        static_assert(sizeof(EntityId) == 4,
                      "EntityId is one packed 32-bit handle on both platforms (BrnCommonTypes.h:27) "
                      "-- if it ever widens, every seat in PART 1 moves");
        static_assert(sizeof(CgsNumeric::Random) == 0x30 && alignof(CgsNumeric::Random) == 16,
                      "CgsNumeric::Random: 48 bytes, 16-aligned. Both halves matter here -- the SIZE "
                      "closes the block on 0x1430 and the ALIGNMENT is what seats it on 0x1400 with "
                      "no pad. This is the assert that fires if anyone re-forks the 4-byte "
                      "`struct Random { u32 muState; }` this header used to carry.");
        static_assert(CgsNumeric::KU_FLOAT_BUFFER_SIZE == 8,
                      "the 8-slot ring TrafficPhysics::Construct primes -- it emits exactly ONE "
                      "direct store (slot 0 = 1.0f) plus SEVEN unrolled refills");

        // THE MEMBER WIDTHS PART 1 ASSUMES, ASSERTED. Added after tamper case 3 came back SILENT:
        // mu8FreakOutState sits in front of three bytes of alignment pad, so widening it u8 -> u32
        // moves NOTHING and the offset arithmetic cannot see it -- yet the console plainly stores it
        // with `stb` (SetFreakedOut @0x825B89B0) and loads it with `lbz` (Update @0x82639690). These
        // four lines are the only thing between that and a silent width change.
        static_assert(sizeof(TrafficPhysics::mOwnerID) == 4,
                      "mOwnerID is one 32-bit packed EntityId");
        static_assert(sizeof(TrafficPhysics::mu8FreakOutState) == 1,
                      "mu8FreakOutState is a BYTE -- SetFreakedOut `stb r11, 0x13F4(r3)` and Update "
                      "`lbz r11, 0x13F4(r30)`");
        static_assert(sizeof(TrafficPhysics::mfFreakOutDirection) == 4,
                      "mfFreakOutDirection is an f32 -- SetFreakedOut `stfs f1, 0x13F8(r3)`");
        static_assert(sizeof(TrafficPhysics::mfFreakOutTime) == 4,
                      "mfFreakOutTime is an f32 -- Update `lfs f0, 0x13FC(r30)` / `stfs f0, 0x13FC`");

        // The block's own members must still EXIST, under these names, in this ORDER. Absolute host
        // offsets are deliberately not asserted (see the banner); the relative order is the part
        // PART 1's arithmetic assumes and cannot itself see.
        typedef TrafficPhysics T;
        static_assert(offsetof(T, mOwnerID) < offsetof(T, mu8FreakOutState), "DWARF :117 < :118");
        static_assert(offsetof(T, mu8FreakOutState) < offsetof(T, mfFreakOutDirection), "DWARF :118 < :119");
        static_assert(offsetof(T, mfFreakOutDirection) < offsetof(T, mfFreakOutTime), "DWARF :119 < :120");
        static_assert(offsetof(T, mfFreakOutTime) < offsetof(T, mRandom), "DWARF :120 < :122");

        // And the block really is the TAIL of the class: nothing may be declared behind mRandom, or
        // the console's 0x1430 stride would not be the whole object.
        static_assert(offsetof(T, mRandom) + sizeof(CgsNumeric::Random) == sizeof(T),
                      "mRandom is the last member -- the block closes the class, exactly as the "
                      "0x1430 stride requires on the console");

        // The base anchor, host side: TrafficPhysics adds this block and nothing else to
        // VehiclePhysics, so the host size difference must be the host size of the block.
        static_assert(sizeof(TrafficPhysics) - sizeof(VehiclePhysics) == 64,
                      "TP adds 64 host bytes to its base -- the same 0x40 PART 1 derives for the "
                      "console block. The two platforms agree here because every member of this "
                      "block is pointer-free.");
    }
}
}
