// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_PostScene.cpp
//
// ⭐ NEW 2026-08-18 (wave Q round 2, world-side prop IO-buffer pass).
//
// Out-of-line bodies for BrnWorld::PropEntityIO::InputBuffer_PostScene -- the buffer the crash
// module fills with "this race car finished crashing" notifications and
// BrnWorld::PropEntityModule::PostSceneUpdate @0x822C4718 drains. Both functions are real X360
// ledger symbols with per-address exports; until now the buffer was the placeholder
// `struct { u8 maDeferredPayload[16]; }` and neither was declared anywhere, so
// IOBufferStack::CreateIOBuffer<InputBuffer_PostScene> bound T::Construct to the inherited
// CgsModule::IOBuffer::Construct (status byte only) and the embedded queue's mpEvents stayed
// raw pool garbage -- the same latent fault the 2026-08-15 zero-fill removal exposed across
// this whole IO family.
//
//   Construct @0x822EFC40 -- FIVE instructions (0x822EFC40..0x822EFC50, counted), the entire
//   class. The sixth word at 0x822EFC54 is `.long 0` alignment padding, not an instruction:
//       mr   r11, r3
//       li   r10, 1
//       addi r3, r11, 8
//       stb  r10, 0(r11)                      -- IOBuffer::Construct(): status = constructed
//       b    BrnWorld__CrashIO__RaceCarCrashCompleteEvent_10___Construct
//     The tail-call target's own IDA symbol carries the capacity (10), and the `addi r3,r11,8`
//     is the ONLY member seat the class has.
//   Destruct  @0x822DC398 -- thirteen instructions:
//       bl   CgsModule__IOBuffer__Destruct
//       stw  r11(0), 0x10(r31)                -- +0x10 == queue base (+8) + 8 == miLength
//     i.e. the base teardown followed by the queue's own Clear(), folded. Unlike almost every
//     other Destruct in this family it is NOT the ICF representative -- it has that extra
//     store, so it is its own symbol.
// ============================================================================
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include <cstddef>   // offsetof

namespace BrnWorld
{
namespace PropEntityIO
{
    // The buffer has exactly one member, so the only invariant worth pinning is that it
    // follows the IOBuffer base (console +0x08). No host byte offset is claimed: the console
    // queue is 12 + 10*16 == 172 bytes and the host one is 16 + 10*16 == 176 (mpEvents widens
    // 4 -> 8), which is precisely why the old 16-byte placeholder could not be cast onto.
    void InputBuffer_PostScene::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PostScene, mRaceCarCrashCompleteEventQueue)
                          >= sizeof(CgsModule::IOBuffer),
                      "mRaceCarCrashCompleteEventQueue follows the IOBuffer base (console +0x08)");
        static_assert(RaceCarCrashCompleteEventQueue::KI_LENGTH == 10,
                      "EventQueue<RaceCarCrashCompleteEvent,10> -- capacity is in the "
                      "Construct callee symbol at 0x822EFC50");
    }

    // X360 0x822EFC40 (DWARF :505).
    void InputBuffer_PostScene::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mRaceCarCrashCompleteEventQueue.Construct();
    }

    // X360 0x822DC398 (DWARF :509).
    void InputBuffer_PostScene::Destruct()
    {
        CgsModule::IOBuffer::Destruct();

        mRaceCarCrashCompleteEventQueue.Clear();
    }
}
}
