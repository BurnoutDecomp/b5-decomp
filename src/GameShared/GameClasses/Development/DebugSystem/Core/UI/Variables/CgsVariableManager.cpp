#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"           // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"             // GetUI().GetMenuManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"           // Menu::AddMenuItem
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"  // Variable::Prepare
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"  // MenuItemVariable::Prepare
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // gpDebugPrint, gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT

#include <string.h>

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

        Variable* VariableManager::FindVariable(void* lpValue)
        {
            for (s32 liIndex = 0; liIndex < mVariablePool.GetActiveCount(); ++liIndex)
            {
                Variable* lpVariable = mVariablePool.GetActiveAt(liIndex);
                if (lpVariable->GetValue().mValue.mpValue == lpValue)
                    return lpVariable;
            }
            return nullptr;
        }

        MenuItemVariable* VariableManager::FindMenuItem(Variable* lpVariable)
        {
            for (s32 liIndex = 0; liIndex < mMenuItemPool.GetActiveCount(); ++liIndex)
            {
                MenuItemVariable* lpMenuItem = mMenuItemPool.GetActiveAt(liIndex);
                if (lpMenuItem->GetVariable() == lpVariable)
                    return lpMenuItem;
            }
            return nullptr;
        }

        Variable* VariableManager::FindVariableFromPath(const char* lpcPath)
        {
            if (!lpcPath)
                return nullptr;
            if (*lpcPath == '/')
                ++lpcPath;

            char lacPath[512];
            for (s32 liIndex = 0; liIndex < mMenuItemPool.GetActiveCount(); ++liIndex)
            {
                MenuItemVariable* lpMenuItem = mMenuItemPool.GetActiveAt(liIndex);
                Menu* lpMenu = GetUI().GetMenuManager().FindMenu(lpMenuItem);
                if (!lpMenu)
                    continue;
                lpMenu->GetPath(lacPath, sizeof(lacPath));
                GetUI().SafeStringCat(lacPath, "/", sizeof(lacPath));
                GetUI().SafeStringCat(lacPath, lpMenuItem->GetVariable()->GetName(), sizeof(lacPath));
                const char* lpcCompare = lacPath[0] == '/' ? lacPath + 1 : lacPath;
                if (_stricmp(lpcCompare, lpcPath) == 0)
                    return lpMenuItem->GetVariable();
            }
            return nullptr;
        }

        VariableMetadata* VariableManager::SetMetadata(Variable* lpVariable,
            const Variant& lrValue, VariableMetadata::Type leType)
        {
            VariableMetadata* lpMetadata = lpVariable->FindMetadata(leType);
            if (lpMetadata)
            {
                lpMetadata->mValue = lrValue.mValue;
                return lpMetadata;
            }

            lpMetadata = mMetadataPool.Allocate();
            if (lpMetadata)
            {
                lpMetadata->Prepare(lrValue, leType);
                lpVariable->AddMetadata(lpMetadata);
            }
            return lpMetadata;
        }

        void VariableManager::RemoveMetadata(Variable* lpVariable, VariableMetadata::Type leType)
        {
            VariableMetadata* lpMetadata = lpVariable->FindMetadata(leType);
            if (lpMetadata)
            {
                lpVariable->RemoveMetadata(lpMetadata);
                mMetadataPool.Free(lpMetadata);
            }
        }

        void VariableManager::RecycleMetadata(VariableMetadata* lpMetadata)
        {
            if (lpMetadata)
                mMetadataPool.Free(lpMetadata);
        }

        void VariableManager::RegisterVariable(f32* p, const char* g, const char* n) { RegisterVariable(Variant(p), g, n); }
        void VariableManager::RegisterVariable(s32* p, const char* g, const char* n) { RegisterVariable(Variant(p), g, n); }
        void VariableManager::RegisterVariable(u32* p, const char* g, const char* n) { RegisterVariable(Variant(p), g, n); }
        void VariableManager::RegisterVariable(bool* p, const char* g, const char* n){ RegisterVariable(Variant(p), g, n); }

        void VariableManager::SetRange(f32* p, f32 a, f32 b) { SetRange(p, Variant(a), Variant(b)); }
        void VariableManager::SetRange(s32* p, s32 a, s32 b) { SetRange(p, Variant(a), Variant(b)); }
        void VariableManager::SetRange(u32* p, u32 a, u32 b) { SetRange(p, Variant(a), Variant(b)); }
        void VariableManager::SetStep(f32* p, f32 v) { SetStep(p, Variant(v)); }
        void VariableManager::SetStep(s32* p, s32 v) { SetStep(p, Variant(v)); }
        void VariableManager::SetStep(u32* p, u32 v) { SetStep(p, Variant(v)); }

        void VariableManager::SetRange(void* lpValue, const Variant& lrMin, const Variant& lrMax)
        {
            Variable* lpVariable = FindVariable(lpValue);
            if (!lpVariable)
                return;
            CGS_ASSERT(lrMin.meType == lpVariable->GetValue().GetDereferenceType(),
                       "lMin.meType == lpVariable->GetValue().GetDereferenceType()");
            CGS_ASSERT(lrMax.meType == lpVariable->GetValue().GetDereferenceType(),
                       "lMax.meType == lpVariable->GetValue().GetDereferenceType()");
            SetMetadata(lpVariable, lrMin, VariableMetadata::E_TYPE_MIN);
            SetMetadata(lpVariable, lrMax, VariableMetadata::E_TYPE_MAX);
        }

        void VariableManager::SetStep(void* lpValue, const Variant& lrStep)
        {
            Variable* lpVariable = FindVariable(lpValue);
            if (!lpVariable)
                return;
            CGS_ASSERT(lrStep.meType == lpVariable->GetValue().GetDereferenceType(),
                       "lStep.meType == lpVariable->GetValue().GetDereferenceType()");
            SetMetadata(lpVariable, lrStep, VariableMetadata::E_TYPE_STEP);
        }

        void VariableManager::SetReadOnly(void* lpValue, bool lbValue)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (lbValue) SetMetadata(v, Variant(true), VariableMetadata::E_TYPE_READONLY);
            else RemoveMetadata(v, VariableMetadata::E_TYPE_READONLY);
        }

        void VariableManager::SetSaveEnabled(void* lpValue, bool lbValue)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (!lbValue) SetMetadata(v, Variant(false), VariableMetadata::E_TYPE_SAVEENABLED);
            else RemoveMetadata(v, VariableMetadata::E_TYPE_SAVEENABLED);
        }

        void VariableManager::SetVisible(void* lpValue, bool lbValue)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (!lbValue) SetMetadata(v, Variant(false), VariableMetadata::E_TYPE_VISIBLE);
            else RemoveMetadata(v, VariableMetadata::E_TYPE_VISIBLE);
        }

        void VariableManager::SetOptions(s32* lpValue, const StringList* lpOptions)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (lpOptions) SetMetadata(v, Variant(lpOptions), VariableMetadata::E_TYPE_STRINGLIST);
            else RemoveMetadata(v, VariableMetadata::E_TYPE_STRINGLIST);
        }

        void VariableManager::SetChangeCallback(void* lpValue,
            Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (!lpfCallback)
            {
                RemoveMetadata(v, VariableMetadata::E_TYPE_CHANGE_CALLBACK);
                RemoveMetadata(v, VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);
                return;
            }
            SetMetadata(v, Variant(lpfCallback), VariableMetadata::E_TYPE_CHANGE_CALLBACK);
            Variant lParam; lParam.SetVoidPointerType(lpUserData);
            SetMetadata(v, lParam, VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);
        }

        void VariableManager::SetSelectCallback(void* lpValue,
            Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData)
        {
            Variable* v = FindVariable(lpValue); if (!v) return;
            if (!lpfCallback)
            {
                RemoveMetadata(v, VariableMetadata::E_TYPE_SELECT_CALLBACK);
                RemoveMetadata(v, VariableMetadata::E_TYPE_SELECT_CALLBACK_PARAM);
                return;
            }
            SetMetadata(v, Variant(lpfCallback), VariableMetadata::E_TYPE_SELECT_CALLBACK);
            Variant lParam; lParam.SetVoidPointerType(lpUserData);
            SetMetadata(v, lParam, VariableMetadata::E_TYPE_SELECT_CALLBACK_PARAM);
        }

        void VariableManager::SetVariableName(void* lpValue, const char* lpcName)
        {
            Variable* v = FindVariable(lpValue);
            if (v) v->mpcName = lpcName;
        }

        void VariableManager::SetCustomMenuItem(void* lpValue, MenuItemVariable* lpCustom)
        {
            Variable* v = FindVariable(lpValue);
            MenuItemVariable* lpOriginal = v ? FindMenuItem(v) : nullptr;
            if (!v || !lpOriginal || !lpCustom)
                return;
            lpCustom->Prepare(v);
            GetUI().GetMenuManager().ReplaceMenuItem(lpOriginal, lpCustom);
            mMenuItemPool.Free(lpOriginal);
        }

        void VariableManager::UnregisterVariable(void* lpValue)
        {
            Variable* v = FindVariable(lpValue);
            if (!v) return;
            MenuItemVariable* item = FindMenuItem(v);
            if (item)
            {
                GetUI().GetMenuManager().RemoveMenuItem(item);
                mMenuItemPool.Free(item);
            }
            while (v->mpMetadata)
            {
                VariableMetadata* metadata = v->mpMetadata;
                v->RemoveMetadata(metadata);
                mMetadataPool.Free(metadata);
            }
            v->Release();
            mVariablePool.Free(v);
        }
    }
}
