#include "GameSource/Game/BrnGameModuleIO.h"

// BrnGame::BrnGameModuleIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnGame::BrnGameModuleIO) bodies the single
// X360-emitted InputBuffer function at 0x827E3100 (see BrnGameModuleIO.h for the truncated-symbol
// / ICF-fold FLAG notes).
//
// X360 0x827E3100 store-for-store:
//   mflr r12 / stw / stwu            -- frame prologue (LR save only; no locals)
//   mr   r3, r4                      -- pass the second argument (r4) as the destruct `this`
//   bl   <empty ICF-folded Destruct> -- invoke the empty destructor on the passed buffer
//   li   r3, 1                       -- return value = 1
//   addi r1 / lwz / mtlr / blr       -- frame epilogue
// `this` (r3 on entry) is not read anywhere in the body.

namespace BrnGame
{
namespace BrnGameModuleIO
{
    // X360 0x827E3100. Invoke the (empty, ICF-folded) IOBuffer destructor on the supplied buffer
    // and return 1. See the header FLAG: the source method name is truncated ("...InputBuffer,cla")
    // and the callee is the empty fold representative (CgsModule::IOBuffer::Destruct, folded with
    // CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct). No behaviour is added beyond
    // the emitted empty-destruct call and the constant 1 return.
    int InputBuffer::DestroyBuffer(CgsModule::IOBuffer* lpBuffer)
    {
        lpBuffer->Destruct();
        return 1;
    }
}
}
