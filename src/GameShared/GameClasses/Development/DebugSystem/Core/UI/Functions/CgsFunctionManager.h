#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"  // Function::DebugCallbackFunction

// CgsDev::DebugUI::FunctionManager - owns every debug-menu action: RegisterFunction allocates a
// Function out of mFunctionPool keyed on (callback, userData) and creates the menu row that calls
// it; CallFunction looks one up by path and fires it. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h). DebugComponent::RegisterFunction
// resolves the component-relative path then calls RegisterFunction directly (X360 0x8282F720); the
// (callback, userData, path, name) overload is public, so no friendship is required. Function /
// MenuItemFunction are pooled by pointer and forward-declared; bodies follow with the pool/element
// reconstruction.

namespace CgsDev
{
    struct DebugManagerConstructParameters;

    namespace DebugUI
    {
        struct MenuItemFunction;

        struct FunctionManager : public Internal::DebugInternal
        {
        private:
            Internal::DebugStaticPool<Function>         mFunctionPool;
            Internal::DebugStaticPool<MenuItemFunction> mMenuItemPool;

        public:
            void Construct(const DebugManagerConstructParameters* lpParameters);
            void Destruct();

            void RegisterFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcGroup, const char* lpcName);
            void UnregisterFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData);
            void SetFunctionName(Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcName);
            void CallFunction(const char* lpcPath);

        private:
            Function* FindFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData);
        };
    }
}
