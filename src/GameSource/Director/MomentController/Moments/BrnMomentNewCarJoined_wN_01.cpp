#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"

// BrnDirector::MomentNewCarJoined -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentNewCarJoined.cpp; member/method names verbatim
// from the DecFIGS DWARF). Wave-N partfile 01.
//
// Bodied here (2 of this partfile's 3 assigned ledger functions):
//   GetName         @0x821F7678  (DWARF cpp:260)
//   GetInstanceType @0x828A80F0  (DWARF cpp:246)
//
// Construct @0x8225F408 (DWARF cpp:34) is fully decoded but CANNOT compile in
// this tree: it calls Camera::BehaviourLooseAttachment::Parameters::Construct()
// @0x821FA920 (the `bl` at 0x8225F4D0), and that member is not declared in the
// committed BrnBehaviourLooseAttachment.h. The complete body is parked at
// scratchpad/waveN/parked/BrnMomentNewCarJoined_01_Construct.cpp with the exact
// one-line header edit that unblocks it (spec section 7, request B).
//
// ⚠️ Mutual exclusion (noted in the leaf header): this TU must NOT include
// GameSource/Director/MomentController/BrnMomentSubclasses.h -- that header
// carries a layout stub of the same class.

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// @0x821F7678 -- cpp:260. Two instructions plus the return:
//   lis  r11, aMomentnewcarjo@ha
//   addi r3,  r11, aMomentnewcarjo@l   # "MomentNewCarJoined"
//   blr
// ----------------------------------------------------------------------------
const char* MomentNewCarJoined::GetName() const
{
    return "MomentNewCarJoined";
}

// ----------------------------------------------------------------------------
// @0x828A80F0 -- cpp:246. The whole function:
//   li   r3, 0xA
//   blr
// 0xA == Moment::E_MOMENT_NEW_CAR_JOINED (the enumerator is committed in
// BrnMoment.h with exactly this value; the console immediate is the VALUE, not
// an offset, so it carries over to the host unchanged).
// ----------------------------------------------------------------------------
Moment::EType MomentNewCarJoined::GetInstanceType()
{
    return E_MOMENT_NEW_CAR_JOINED;
}

}
