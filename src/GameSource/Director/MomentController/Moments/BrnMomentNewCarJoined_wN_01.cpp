#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"

// BrnDirector::MomentNewCarJoined -- reconstructed from the console executable
// (home file BrnMomentNewCarJoined.cpp; member/method names verbatim
// from the declarations). Wave-N partfile 01.
//
// Bodied here (2 of this partfile's 3 assigned ledger functions):
//   GetName
//   GetInstanceType
//
// Construct is fully decoded but CANNOT compile in
// this tree: it calls Camera::BehaviourLooseAttachment::Parameters::Construct()
// and that member is not declared in the
// committed BrnBehaviourLooseAttachment.h. The complete body is parked at
// scratchpad/waveN/parked/BrnMomentNewCarJoined_01_Construct.cpp with the exact
// one-line header edit that unblocks it (spec section 7, request B).

namespace BrnDirector
{

// The whole function: load the address of the string "MomentNewCarJoined" and return it.
const char* MomentNewCarJoined::GetName() const
{
    return "MomentNewCarJoined";
}

// The whole function: return the constant 0xA.
// 0xA == Moment::E_MOMENT_NEW_CAR_JOINED (the enumerator is committed in
// BrnMoment.h with exactly this value; the console immediate is the VALUE, not
// an offset, so it carries over to the host unchanged).
Moment::EType MomentNewCarJoined::GetInstanceType()
{
    return E_MOMENT_NEW_CAR_JOINED;
}

}
