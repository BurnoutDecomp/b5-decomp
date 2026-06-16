#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"

// CgsDev::DebugUI::Variable - the register-path body: Prepare stores the value Variant + name and
// clears the metadata list (matching the X360 VariableManager::RegisterVariable direct fill
// 0x82829A80: copy the Variant, set name, null metadata). GetValue/GetName back the manager.
// The metadata attach/find + value formatting (Decrement/Increment/IsReadOnly/GetDisplayString/...)
// is the variable-edit follow-on.

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
    }
}
