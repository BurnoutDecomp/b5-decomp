#include "GameShared/GameClasses/Fsm/CgsVariable.h"
#include "GameShared/GameClasses/Fsm/CgsLuaState.h"   // CgsFsm::LuaState

#include <cstring>   // strncpy

// CgsFsm::Variable hierarchy -- reconstructed from the DecFIGS DWARF (CgsVariable.cpp) +
// BURNOUT_X360_ARTIST.XEX. Each concrete kind marshals through CgsFsm::LuaState's typed
// setters/getters with its own name id as the Lua-side key.

namespace CgsFsm
{

Variable::Variable() : mId(0) {}
void Variable::ToLua(LuaState* /*lpLuaState*/, const char* /*lpcFunctionName*/) const {}
void Variable::FromLua(LuaState* /*lpLuaState*/, const char* /*lpcFunctionName*/) {}

// ---- VariableInt ----
void VariableInt::Construct(CgsID lId, s32 liValue) { mId = lId; miValue = liValue; }
void VariableInt::ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const
{
    lpLuaState->SetInt(lpcFunctionName, mId, miValue);
}
void VariableInt::FromLua(LuaState* lpLuaState, const char* lpcFunctionName)
{
    miValue = lpLuaState->GetInt(lpcFunctionName, mId);
}

// ---- VariableBool ----
void VariableBool::Construct(CgsID lId, bool lbValue) { mId = lId; mbValue = lbValue; }
void VariableBool::ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const
{
    lpLuaState->SetBool(lpcFunctionName, mId, mbValue);
}
void VariableBool::FromLua(LuaState* lpLuaState, const char* lpcFunctionName)
{
    // No dedicated Lua bool getter; read it as an int (non-zero = true), matching the X360.
    mbValue = lpLuaState->GetInt(lpcFunctionName, mId) != 0;
}

// ---- VariableString ----
void VariableString::Construct(CgsID lId, char* lpcValue, s32 liBufferSize)
{
    mId = lId; mpcValue = lpcValue; miBufferSize = liBufferSize;
}
void VariableString::ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const
{
    lpLuaState->SetString(lpcFunctionName, mId, mpcValue);
}
void VariableString::FromLua(LuaState* lpLuaState, const char* lpcFunctionName)
{
    lpLuaState->GetString(lpcFunctionName, mId, mpcValue, miBufferSize);
}

} // namespace CgsFsm
