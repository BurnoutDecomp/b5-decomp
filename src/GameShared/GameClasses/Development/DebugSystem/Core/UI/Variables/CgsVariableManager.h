#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"   // Variant, StringList

// CgsDev::DebugUI::VariableManager - owns every debug-menu variable: the typed RegisterVariable
// overloads build a Variant tag + resolve the menu path, then funnel into the private
// RegisterVariable(const Variant&, path, name) core which allocates a Variable out of mVariablePool.
// SetRange/SetStep/options/callbacks attach VariableMetadata; SetCustomMenuItem swaps the rendered
// row. Recovered from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h).
//
// DebugComponent is the privileged caller: it builds the Variant + the component-relative path
// itself and calls the private (Variant&, path, name) / (void*, Variant&, Variant&) cores directly
// (X360 0x8282D560 / 0x8282D800 / 0x8282F598), so it is declared a friend.
//
// The three DebugStaticPool members hold their elements by pointer, so Variable / MenuItemVariable
// / VariableMetadata are forward-declared here; their definitions + the manager method bodies are
// reconstructed in the link-closure follow-on.

namespace CgsDev
{
    class DebugComponent;
    struct DebugManagerConstructParameters;

    namespace DebugUI
    {
        struct Variable;
        struct MenuItemVariable;
        struct VariableMetadata;
        struct ScriptInterface;

        struct VariableManager : public Internal::DebugInternal
        {
            friend class CgsDev::DebugComponent;
            friend class CgsDev::DebugManager;   // registers a component's mbActive toggle via the core
            friend struct ScriptInterface;       // the console resolves a variable by path (FindVariableFromPath)

        private:
            Internal::DebugStaticPool<Variable>         mVariablePool;
            Internal::DebugStaticPool<MenuItemVariable> mMenuItemPool;
            Internal::DebugStaticPool<VariableMetadata> mMetadataPool;

        public:
            void Construct(const DebugManagerConstructParameters* lpParameters);
            void Destruct();

            void RegisterVariable(f32* lpfValue, const char* lpcGroup, const char* lpcName);
            void RegisterVariable(s32* lpiValue, const char* lpcGroup, const char* lpcName);
            void RegisterVariable(u32* lpuValue, const char* lpcGroup, const char* lpcName);
            void RegisterVariable(bool* lpbValue, const char* lpcGroup, const char* lpcName);

            void SetRange(f32* lpfValue, f32 lfMin, f32 lfMax);
            void SetRange(s32* lpiValue, s32 liMin, s32 liMax);
            void SetRange(u32* lpuValue, u32 luMin, u32 luMax);

            void SetStep(f32* lpfValue, f32 lfStep);
            void SetStep(s32* lpiValue, s32 liStep);
            void SetStep(u32* lpuValue, u32 luStep);

            void SetReadOnly(void* lpValue, bool lbReadOnly);
            void SetSaveEnabled(void* lpValue, bool lbSaveEnabled);
            void SetVisible(void* lpValue, bool lbVisible);
            void SetCustomMenuItem(void* lpValue, MenuItemVariable* lpMenuItem);
            void SetOptions(s32* lpiValue, const StringList* lpStringList);
            void SetChangeCallback(void* lpValue, Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData);
            void SetSelectCallback(void* lpValue, Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData);
            void SetVariableName(void* lpValue, const char* lpcName);
            void UnregisterVariable(void* lpValue);

        private:
            Variable*         FindVariable(void* lpValue);
            MenuItemVariable* FindMenuItem(Variable* lpVariable);
            Variable*         FindVariableFromPath(const char* lpcPath);

            // The shared cores DebugComponent funnels into (it does its own Variant build + path
            // resolution). DWARF CgsVariableManager.h:110-112.
            void RegisterVariable(const Variant& lrVariant, const char* lpcPath, const char* lpcName);
            void SetRange(void* lpValue, const Variant& lrMin, const Variant& lrMax);
            void SetStep(void* lpValue, const Variant& lrStep);

            // SetMetadata / RemoveMetadata (DWARF :115/:118) take VariableMetadata::Type and so
            // need the full VariableMetadata definition; reconstructed with it in the follow-on.
            void RecycleMetadata(VariableMetadata* lpMetadata);
        };
    }
}
