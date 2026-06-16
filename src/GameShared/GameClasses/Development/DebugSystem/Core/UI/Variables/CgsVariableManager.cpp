#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"           // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"             // GetUI().GetMenuManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"           // Menu::AddMenuItem
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"  // Variable::Prepare
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"  // MenuItemVariable::Prepare
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // gpDebugPrint, gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT

// CgsDev::DebugUI::VariableManager::RegisterVariable - the shared core every typed RegisterVariable
// overload (and DebugComponent, as a friend) funnels into. X360 0x82829A80: resolve the menu path,
// pull a Variable + a MenuItemVariable from their pools, hang the row on the menu, fill the variable
// with the value Variant + name, then bind the row to it. Out-of-pool failures emit the filter-gated
// debug spew and bail.
//
// SetRange/SetStep/SetMetadata/FindVariable + the typed RegisterVariable overloads + the attribute
// setters are the metadata follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 CgsVariableManager.cpp:60. Size the three pools from the construct parameters, then
        // Clear each (Construct allocates + element-constructs the backing block; Clear fills the free
        // list). The MenuItemVariable pool is sized 1:1 with the variable pool (each registered
        // variable gets one menu row); the metadata pool has its own size field.
        void VariableManager::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            rw::IResourceAllocator* lpAllocator = lpParameters->mpRwAllocator;

            mVariablePool.Construct(lpParameters->miVariablePoolSize, lpAllocator);
            mMenuItemPool.Construct(lpParameters->miVariablePoolSize, lpAllocator);
            mMetadataPool.Construct(lpParameters->miVariableMetadataPoolSize, lpAllocator);

            mVariablePool.Clear();
            mMenuItemPool.Clear();
            mMetadataPool.Clear();
        }

        // X360 CgsVariableManager.cpp:86 is empty: the debug allocator owns the pool backing and is
        // torn down wholesale, so the manager has nothing to release per-pool.
        void VariableManager::Destruct() {}

        void VariableManager::RegisterVariable(const Variant& lrVariant, const char* lpcPath, const char* lpcName)
        {
            Menu* lpMenu = GetUI().GetMenuManager().CreateMenuPath(lpcPath, nullptr);
            if (!lpMenu)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "We've run out of debug Menu Memory.\n";
                return;
            }

            Variable* lpVariable = mVariablePool.Allocate();
            if (!lpVariable)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "We've run out of debug variable memory.\n";
                return;
            }

            MenuItemVariable* lpMenuItem = mMenuItemPool.Allocate();
            CGS_ASSERT(lpMenuItem, "lpMenuItem");

            lpMenu->AddMenuItem(lpMenuItem);
            lpVariable->Prepare(lrVariant, lpcName);
            lpMenuItem->Prepare(lpVariable);
        }

        // --- attribute/metadata setters: the metadata path (FindVariable + SetMetadata over
        // mMetadataPool, SetRange X360 0x8282ECB8) is the next faithful brick. Stubbed here so the
        // subsystem links into the exe; this code is dead in the loading build (no variable is
        // registered or edited during loading). ---
        void VariableManager::SetReadOnly(void*, bool) {}
        void VariableManager::SetSaveEnabled(void*, bool) {}
        void VariableManager::SetVisible(void*, bool) {}
        void VariableManager::SetCustomMenuItem(void*, MenuItemVariable*) {}
        void VariableManager::SetOptions(s32*, const StringList*) {}
        void VariableManager::SetChangeCallback(void*, Variant::UValue::VariableCallbackFunction, void*) {}
        void VariableManager::SetSelectCallback(void*, Variant::UValue::VariableCallbackFunction, void*) {}
        void VariableManager::SetVariableName(void*, const char*) {}
        void VariableManager::UnregisterVariable(void*) {}
        void VariableManager::SetRange(void*, const Variant&, const Variant&) {}
        void VariableManager::SetStep(void*, const Variant&) {}
    }
}
