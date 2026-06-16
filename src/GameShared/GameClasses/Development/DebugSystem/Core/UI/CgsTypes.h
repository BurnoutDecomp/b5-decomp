#pragma once

#include "types.hpp"

// CgsDev::DebugUI core value types. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/CgsTypes.h). This is the incremental subset the debug
// component/menu API actually needs: the input-event enum, the int->name string-list entry,
// and the Variant tagged-union (whose UValue carries the variable-callback typedef the
// DebugComponent change/select-callback API takes). Palette/Metrics (RGBA-valued) are part of
// the full header but are deferred until a consumer needs them.

namespace CgsDev
{
    namespace DebugUI
    {
        enum InputEvent
        {
            E_INPUTEVENT_NONE = 0,
            E_INPUTEVENT_SELECT = 1,
            E_INPUTEVENT_BACK = 2,
            E_INPUTEVENT_CLOSE = 3,
            E_INPUTEVENT_CURSORUP = 4,
            E_INPUTEVENT_CURSORDOWN = 5,
            E_INPUTEVENT_CURSORLEFT = 6,
            E_INPUTEVENT_CURSORRIGHT = 7,
            E_INPUTEVENT_TOGGLEPIN = 8,
            E_INPUTEVENT_TOGGLECONSOLE = 9,
            E_INPUTEVENT_MAINMENU = 10,
            E_INPUTEVENT_NEXTWINDOW = 11,
            E_INPUTEVENT_PREVWINDOW = 12,
            E_INPUTEVENT_TOGGLEUI = 13,
            E_INPUTEVENT_DOCKTOP = 14,
            E_INPUTEVENT_DOCKBOTTOM = 15,
            E_INPUTEVENT_DOCKLEFT = 16,
            E_INPUTEVENT_DOCKRIGHT = 17,
            E_INPUTEVENT_COUNT = 18,
        };

        // An int value paired with its display name (the options table for enum-style variables).
        struct StringList
        {
            s32         miValue;
            const char* mpcName;
        };

        // Tagged union holding any debug-variable value (immediate or by-pointer) or a callback.
        struct Variant
        {
            enum Type
            {
                E_TYPE_NONE = 0,
                E_TYPE_FLOAT = 1,
                E_TYPE_INT32 = 2,
                E_TYPE_UINT32 = 3,
                E_TYPE_BOOL = 4,
                E_TYPE_PTR_FLOAT = 5,
                E_TYPE_PTR_INT32 = 6,
                E_TYPE_PTR_UINT32 = 7,
                E_TYPE_PTR_BOOL = 8,
                E_TYPE_PTR_VOID = 9,
                E_TYPE_UI_STRINGLISTINT32 = 10,
                E_TYPE_UI_VARIABLECALLBACK = 11,
                E_TYPE_COUNT = 12,
            };

            union UValue
            {
                f32  mfFloat;
                s32  miInt32;
                u32  muUInt32;
                bool mbBool;
                f32* mpfFloat;
                s32* mpiInt32;
                u32* mpuUInt32;
                bool* mpbBool;
                const StringList* mpInt32StringList;

                typedef void (*VariableCallbackFunction)(void*, void*);
                VariableCallbackFunction mpVariableCallbackFunction;

                void* mpValue;
            };

            Type   meType;
            UValue mValue;

            Variant();
            Variant(const Variant& lrOther);
            Variant(f32 lfValue);
            Variant(s32 liValue);
            Variant(u32 luValue);
            Variant(bool lbValue);
            Variant(f32* lpfValue);
            Variant(s32* lpiValue);
            Variant(u32* lpuValue);
            Variant(bool* lpbValue);
            Variant(const StringList* lpStringList);
            Variant(UValue::VariableCallbackFunction lpfCallback);

            void Copy(const Variant& lrOther);
            void Clear();
            void ConvertToString(char* lpcBuffer, s32 liBufferLen);
            void ConvertFromString(const char* lpcString);
            void SetVoidPointerType(void* lpValue);
            bool Dereference(const Variant& lrOther);
            Type GetDereferenceType() const;
        };
    }
}
