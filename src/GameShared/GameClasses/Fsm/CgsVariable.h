#ifndef CGS_FSM_VARIABLE_H
#define CGS_FSM_VARIABLE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"   // CgsID

// CgsFsm::Variable - a typed, named value carried by a CgsFsm::Event. The variable knows how
// to push itself into ("ToLua") and read itself back from ("FromLua") the FSM's Lua state,
// through a named Lua setter/getter function (e.g. "SetEventVariable" / "GetEventVariable").
// Reconstructed from the DecFIGS DWARF (CgsVariable.h) + BURNOUT_X360_ARTIST.XEX. The three
// concrete kinds (Int/Bool/String) route through CgsFsm::LuaState::Set*/Get*.

namespace CgsFsm
{
    class LuaState;

    struct Variable
    {
        Variable();

        // vtable[0]/[1]: marshal this variable's value into / out of the Lua state via the
        // named Lua function. The base is a no-op; the concrete kinds override.
        virtual void ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const;
        virtual void FromLua(LuaState* lpLuaState, const char* lpcFunctionName);

        CgsID GetId() const { return mId; }

    protected:
        CgsID mId;   // the variable's name id (the key passed to the Lua setter)
    };

    struct VariableInt : public Variable
    {
        void Construct(CgsID lId, s32 liValue);
        s32  GetValue() const { return miValue; }

        virtual void ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const;
        virtual void FromLua(LuaState* lpLuaState, const char* lpcFunctionName);

    protected:
        s32 miValue;
    };

    struct VariableBool : public Variable
    {
        void Construct(CgsID lId, bool lbValue);
        bool GetValue() const { return mbValue; }

        virtual void ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const;
        virtual void FromLua(LuaState* lpLuaState, const char* lpcFunctionName);

    protected:
        bool mbValue;
    };

    struct VariableString : public Variable
    {
        void        Construct(CgsID lId, char* lpcValue, s32 liBufferSize);
        const char* GetValue() const { return mpcValue; }

        virtual void ToLua(LuaState* lpLuaState, const char* lpcFunctionName) const;
        virtual void FromLua(LuaState* lpLuaState, const char* lpcFunctionName);

    protected:
        char* mpcValue;       // caller-owned string buffer
        s32   miBufferSize;
    };
}

#endif // CGS_FSM_VARIABLE_H
