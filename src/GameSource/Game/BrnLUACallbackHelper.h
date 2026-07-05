#ifndef BRN_LUA_CALLBACK_HELPER_H
#define BRN_LUA_CALLBACK_HELPER_H

#include "types.hpp"

// BrnGame::LUACallbackHelper - the counterpart to BrnGame::LUACall used on the
// callback side: when a game-side Lua callback fires, this helper reads the
// arguments that Lua pushed onto the stack (by 1-based param index) and pushes
// results back. Consumed by the automated-test driver (BrnGame::AutoTestManager).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetParamType    @ 0x823C0370
//   GetInt32Param   @ 0x823C5D70
//   GetStringParam  @ 0x823C5DF8
//
// Layout + method shape are authoritative from DWARF (BrnLUACallbackHelper.h):
//   +0x0 miNumParams (int32)  +0x4 miNumResults (int32)  +0x8 mpLUAHandle (lua_State*)
//
// The Lua C API (lua_State / lua_type / lua_isnumber / lua_isstring / lua_tonumber /
// lua_tolstring) is an external third-party library; per the naming convention its
// native identifiers are retained.

// ---- Minimal external Lua C API surface used by this TU. -----------------------------
// lua_State is opaque here (the engine only stores/forwards the handle pointer).
struct lua_State;

extern "C"
{
    // lua_type(L, idx) -> type tag of the value at the stack index (0 == LUA_TNIL,
    // 1 == LUA_TBOOLEAN).
    int         lua_type(lua_State* lpLuaState, int liIndex);
    // lua_isnumber(L, idx) -> non-zero if the value at idx is (convertible to) a number.
    int         lua_isnumber(lua_State* lpLuaState, int liIndex);
    // lua_isstring(L, idx) -> non-zero if the value at idx is (convertible to) a string.
    int         lua_isstring(lua_State* lpLuaState, int liIndex);
    // lua_tonumber(L, idx) -> the number at idx as a double.
    double      lua_tonumber(lua_State* lpLuaState, int liIndex);
    // lua_tolstring(L, idx, &len) -> the string at idx; len may be null.
    const char* lua_tolstring(lua_State* lpLuaState, int liIndex, usize* lpuLen);
}

namespace BrnGame
{
    class LUACallbackHelper
    {
    public:
        // BrnLUACallbackHelper.h:50 (DWARF). Argument-type tag returned by GetParamType.
        enum EArgType
        {
            E_NUMBER  = 0,
            E_STRING  = 1,
            E_NIL     = 2,
            E_BOOL    = 3,
            E_UNKNOWN = 4,
        };

        // DECLARE-ONLY: bodies for the rest of the class live in other TUs.
        explicit LUACallbackHelper(lua_State* lpLUAHandle);
        s32         GetNumParams() const;
        f32         GetFloat32Param(s32 liParamIndex);

        // @ 0x823C5D70. Assert the param is a number, then truncate lua_tonumber to s32.
        s32         GetInt32Param(s32 liParamIndex);

        // @ 0x823C5DF8. Assert the param is a string, then return lua_tolstring (null len).
        const char* GetStringParam(s32 liParamIndex) const;

        bool        GetBoolParam(s32 liParamIndex) const;
        bool        IsParamNumber(s32 liParamIndex) const;
        bool        IsParamString(s32 liParamIndex) const;
        bool        IsParamBool(s32 liParamIndex) const;
        bool        IsParamNil(s32 liParamIndex) const;

        // @ 0x823C0370. Classify the param: nil / number / string / bool / unknown.
        EArgType    GetParamType(s32 liParamIndex) const;

        void        ClearParams();
        s32         GetReturnValue() const;
        void        AddResult(f32 lfValue);
        void        AddResult(s32 liValue);
        void        AddResult(const char* lpcValue);
        void        AddResult(bool lbValue);
        void        AddNILResult();

    private:
        s32 ParamIndexToStackIndex(s32 liParamIndex) const;

        s32        miNumParams;   // +0x0
        s32        miNumResults;  // +0x4
        lua_State* mpLUAHandle;   // +0x8
    };
}

#endif // BRN_LUA_CALLBACK_HELPER_H
