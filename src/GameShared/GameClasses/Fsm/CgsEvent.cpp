#include "GameShared/GameClasses/Fsm/CgsEvent.h"
#include "GameShared/GameClasses/Fsm/CgsVariable.h"   // CgsFsm::Variable
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT

// CgsFsm::Event -- reconstructed from the DecFIGS DWARF (CgsEvent.h). A small builder: set the
// id, then AddVariable() the typed values to carry, before handing it to ScriptedFsm::SendEvent.

namespace CgsFsm
{

void Event::Construct(CgsID lId)
{
    mId = lId;
    miVariableCount = 0;
    for (s32 li = 0; li < KI_MAX_VARIABLE_COUNT; ++li)
        maVariables[li] = 0;
}

void Event::AddVariable(const Variable* lpVariable)
{
    CGS_ASSERT(miVariableCount < KI_MAX_VARIABLE_COUNT, "event variable overflow");
    if (miVariableCount < KI_MAX_VARIABLE_COUNT)
        maVariables[miVariableCount++] = lpVariable;
}

} // namespace CgsFsm
