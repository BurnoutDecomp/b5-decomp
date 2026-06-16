#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"  // Variant, StringList

// CgsDev::DebugUI::Variable + VariableMetadata - the stored form of a registered debug variable.
// A Variable wraps the Variant (the value, immediate or by-pointer) plus its display name and an
// intrusive list of VariableMetadata (range / step / read-only / options / callbacks). Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Variables/CgsVariable.h). VariableManager
// pools these and calls Prepare/AddMetadata/FindMetadata on them.

namespace CgsDev
{
    namespace DebugUI
    {
        // One attribute attached to a Variable (X360 keyed by Type). Holds the attribute value in a
        // raw UValue (interpreted per meType) and threads onto the variable's metadata list.
        struct VariableMetadata
        {
            enum Type
            {
                E_TYPE_MIN = 0,
                E_TYPE_MAX = 1,
                E_TYPE_STEP = 2,
                E_TYPE_READONLY = 3,
                E_TYPE_SAVEENABLED = 4,
                E_TYPE_VISIBLE = 5,
                E_TYPE_STRINGLIST = 6,
                E_TYPE_CHANGE_CALLBACK = 7,
                E_TYPE_CHANGE_CALLBACK_PARAM = 8,
                E_TYPE_SELECT_CALLBACK = 9,
                E_TYPE_SELECT_CALLBACK_PARAM = 10,
                E_TYPE_COUNT = 11,
            };

            bool Prepare(const Variant& lrValue, Type leType);

        private:
            Variant::UValue   mValue;
            Type              meType;
            VariableMetadata* mpNextMetadata;
        };

        // A registered debug variable: value + name + metadata list.
        struct Variable
        {
            bool Prepare(const Variant& lrVariant, const char* lpcName);
            void Release();

            bool Prepare(f32 lfValue, const char* lpcName);
            bool Prepare(s32 liValue, const char* lpcName);
            bool Prepare(u32 luValue, const char* lpcName);
            bool Prepare(bool lbValue, const char* lpcName);
            bool Prepare(f32* lpfValue, const char* lpcName);
            bool Prepare(s32* lpiValue, const char* lpcName);
            bool Prepare(u32* lpuValue, const char* lpcName);
            bool Prepare(bool* lpbValue, const char* lpcName);

            Variant&    GetValue();
            const char* GetName() const;
            void        GetDisplayString(char* lpcBuffer);
            void        GetValueAsString(char* lpcBuffer, int liBufferLen);
            void        SetValueFromString(const char* lpcString);
            f32         GetValueAsFloat() const;
            f32         GetMinAsFloat(f32 lfDefault) const;
            f32         GetMaxAsFloat(f32 lfDefault) const;

            void Decrement();
            void Increment();
            void Select();

            bool IsReadOnly();
            bool IsSaveEnabled();
            bool IsVisible();

            const StringList* GetStringList();

            void              AddMetadata(VariableMetadata* lpMetadata);
            void              RemoveMetadata(VariableMetadata* lpMetadata);
            VariableMetadata* FindMetadata(VariableMetadata::Type leType) const;

        private:
            void OnChange();

            Variant           mVariant;
            const char*       mpcName;
            VariableMetadata* mpMetadata;
        };
    }
}
