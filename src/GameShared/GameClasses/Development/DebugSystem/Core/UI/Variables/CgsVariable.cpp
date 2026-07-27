#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"

// CgsDev::DebugUI::Variable - the register-path body: Prepare stores the value Variant + name and
// clears the metadata list (matching the X360 VariableManager::RegisterVariable direct fill
// 0x82829A80: copy the Variant, set name, null metadata). GetValue/GetName back the manager.
// The metadata attach + value formatting (Decrement/Increment/GetDisplayString/...) is the
// variable-edit follow-on.
//
// DECOMPILED (not stubs): FindMetadata / IsReadOnly / IsSaveEnabled -- see the per-function comments.

namespace CgsDev
{
    namespace DebugUI
    {
        bool Variable::Prepare(const Variant& lrVariant, const char* lpcName)
        {
            mVariant.Copy(lrVariant);
            mpcName    = lpcName;
            mpMetadata = nullptr;
            return true;
        }

        Variant&    Variable::GetValue()       { return mVariant; }
        const char* Variable::GetName() const  { return mpcName; }

        // Linear scan of the attribute chain for the first node of leType (DecFIGS CgsVariable.cpp:610,
        // local `lpMetadata` at :613). The X360 always inlines it; the walk is attested twice inside
        // ScriptInterface::SaveState -- 0x828328C4-0x828328E4 (leType == E_TYPE_READONLY) and
        // 0x8283290C-0x82832928 (leType == E_TYPE_SAVEENABLED). Both load meType at +4, compare against
        // the literal 3 / 4, and follow mpNextMetadata at +8 until null. Those are X360 offsets, so this
        // walks by named member instead.
        VariableMetadata* Variable::FindMetadata(VariableMetadata::Type leType) const
        {
            VariableMetadata* lpMetadata = mpMetadata;
            while (lpMetadata != nullptr)
            {
                if (lpMetadata->meType == leType)
                    return lpMetadata;

                lpMetadata = lpMetadata->mpNextMetadata;
            }

            return nullptr;
        }

        // Read-only flag (DecFIGS CgsVariable.cpp:262, local `lpReadOnly` at :264). Inlined in the X360;
        // attested at 0x828328B8-0x82832904 in ScriptInterface::SaveState: walk for E_TYPE_READONLY, and
        // on a hit `lbz r11,0(r11)` reads the attribute's stored bool out of mValue. When the attribute
        // is absent the result falls through to r31, which `li r31,0` @0x82832808 pins to FALSE.
        bool Variable::IsReadOnly()
        {
            VariableMetadata* lpReadOnly = FindMetadata(VariableMetadata::E_TYPE_READONLY);
            if (lpReadOnly != nullptr)
                return lpReadOnly->mValue.mbBool;

            return false;
        }

        // Save-enabled flag (DecFIGS CgsVariable.cpp:284, local `lpSaveEnabled` at :286). Same inlined
        // shape at 0x82832908-0x82832938, but the ABSENT default is the opposite one: the no-hit exit
        // 0x8283292C is `li r11,1`, i.e. a variable with no E_TYPE_SAVEENABLED attribute IS saved. On a
        // hit, 0x82832964 `lbz r11,0(r11)` returns the stored bool. The asymmetry with IsReadOnly above
        // is what the binary does, not a slip.
        bool Variable::IsSaveEnabled()
        {
            VariableMetadata* lpSaveEnabled = FindMetadata(VariableMetadata::E_TYPE_SAVEENABLED);
            if (lpSaveEnabled != nullptr)
                return lpSaveEnabled->mValue.mbBool;

            return true;
        }

        // --- variable-edit follow-on: menu-tick surface, dead on the loading-screen boot
        // (no debug menu is ticked/drawn during loading). Stubbed so MenuItemVariable links. ---
        void Variable::Increment() {}
        void Variable::Decrement() {}
        void Variable::Select()    {}
        bool Variable::IsVisible() { return true; }
        void Variable::GetValueAsString(char* lpcBuffer, int liBufferLen)
        {
            if (liBufferLen > 0)
                lpcBuffer[0] = '\0';
        }
    }
}
