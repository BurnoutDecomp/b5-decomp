#ifndef BRN_LUA_CALL_H
#define BRN_LUA_CALL_H

#include "types.hpp"

// BrnGame::LUACall - a small RAII-ish helper that pushes a Lua function and its arguments
// onto a lua_State, invokes it via lua_pcall, and exposes the results. Used by the
// automated-test driver (BrnGame::AutoTestManager) to call into game-side Lua scripts.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGame::LUACall::Call @ 0x823AA320
//
// Layout (DWARF BrnLUACall.h):
//   +0x0 miNumParams (int32)  +0x4 miNumResults (int32)
//   +0x8 mpLuaHandle (lua_State*)  +0xC mbCalledFunction (bool)
//
// The Lua C API (lua_State / lua_pcall / lua_tolstring / lua_gettop) is an external
// third-party library; per the naming convention its native identifiers are retained.

// ---- Minimal external Lua C API surface used by this TU. -----------------------------
// lua_State is opaque here (the engine only stores/forwards the handle pointer).
struct lua_State;

extern "C"
{
    // @ call sites: lua_pcall(L, nargs, nresults, errfunc) -> 0 on success.
    int         lua_pcall(lua_State* lpLuaState, int liNumArgs, int liNumResults, int liErrFunc);
    // @ call site: lua_tolstring(L, idx, &len) -> the string at the given stack index.
    const char* lua_tolstring(lua_State* lpLuaState, int liIndex, usize* lpuLen);
    // @ call site: lua_gettop(L) -> index of the top stack element (== result count here).
    int         lua_gettop(lua_State* lpLuaState);
    // @ call site: lua_type(L, idx) -> LUA_T* tag (0 == LUA_TNIL, 1 == LUA_TBOOLEAN).
    int         lua_type(lua_State* lpLuaState, int liIndex);
    // @ call site: lua_isnumber(L, idx) -> nonzero if the value is a number.
    int         lua_isnumber(lua_State* lpLuaState, int liIndex);
    // @ call site: lua_isstring(L, idx) -> nonzero if the value is a string.
    int         lua_isstring(lua_State* lpLuaState, int liIndex);
    // @ call site: lua_tonumber(L, idx) -> the value at the index as a double.
    double      lua_tonumber(lua_State* lpLuaState, int liIndex);
}

namespace BrnGame
{
    class LUACall
    {
    public:
        // BrnLUACall.h:62 (DWARF). Argument-type tag returned by GetArgType.
        enum EArgType
        {
            E_NUMBER  = 0,
            E_STRING  = 1,
            E_NIL     = 2,
            E_BOOL    = 3,
            E_UNKNOWN = 4,
        };

        // DECLARE-ONLY: bodies for the rest of the class live in other TUs.
        explicit LUACall(lua_State* lpLuaHandle);
        void     SetFunction(const char* lpacFunctionName);
        void     AddArgument(bool lbValue);
        void     AddArgument(f32 lfValue);
        void     AddArgument(s32 liValue);
        void     AddArgument(const char* lpacValue);

        // @ 0x823AA320. Invoke the pushed function via lua_pcall; on failure fire an assert
        // with the Lua error string; then reset the param count and cache the result count.
        void Call();

        s32         GetNumResults();
        f32         GetFloat32(s32 liResultIndex);
        s32         GetInt32(s32 liResultIndex);
        const char* GetString(s32 liResultIndex) const;
        bool        GetBool(s32 liResultIndex) const;
        bool        IsNumber(s32 liResultIndex) const;
        bool        IsString(s32 liResultIndex) const;
        bool        IsBool(s32 liResultIndex) const;
        bool        IsNil(s32 liResultIndex) const;
        EArgType    GetArgType(s32 liResultIndex) const;
        void        ResetStack();

    private:
        s32 ResultIndexToStackIndex(s32 liResultIndex) const;

        s32        miNumParams;       // +0x0
        s32        miNumResults;      // +0x4
        lua_State* mpLuaHandle;       // +0x8
        bool       mbCalledFunction;  // +0xC
    };
}

#endif
