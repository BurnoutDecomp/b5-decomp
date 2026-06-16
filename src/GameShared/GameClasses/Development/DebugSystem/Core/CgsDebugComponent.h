#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"

// CgsDev::DebugComponent - the base every in-game debug component derives from (perfmon
// overlays, the takedown/mode-manager debug menus, etc.). Subclasses register their member
// variables and action callbacks with the debug menu (RegisterVariable / SetRange / SetStep /
// RegisterFunction / ...) and override OnActivate / RenderHUD / GetName / GetPath. Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/CgsDebugComponent.h). Method bodies live
// in CgsDebugComponent.cpp (its own TU); this header is the declaration surface callers compile
// against. Heavy UI types reached only by pointer (render contexts, menu items) are forward-
// declared to avoid a transitive cascade.

namespace CgsDev
{
    class Debug2DImmediateRender;
    class Debug3DImmediateRender;

    namespace DebugUI
    {
        struct MenuItem;
        struct MenuItemVariable;
    }

    class DebugComponent : public Internal::DebugInternal
    {
    public:
        DebugComponent();

        void Construct();
        virtual void Update();
        void Destruct();
        void Register();

        virtual void RenderWorld(Debug3DImmediateRender* lpRender);
        virtual void RenderHUD(Debug2DImmediateRender* lpRender);

        bool IsActive() const;

        // Register a member/global variable with the debug menu, with or without a group path.
        void RegisterVariable(f32* lpfValue, const char* lpcGroup, const char* lpcName);
        void RegisterVariable(s32* lpiValue, const char* lpcGroup, const char* lpcName);
        void RegisterVariable(u32* lpuValue, const char* lpcGroup, const char* lpcName);
        void RegisterVariable(bool* lpbValue, const char* lpcGroup, const char* lpcName);
        void RegisterVariable(f32* lpfValue, const char* lpcName);
        void RegisterVariable(s32* lpiValue, const char* lpcName);
        void RegisterVariable(u32* lpuValue, const char* lpcName);
        void RegisterVariable(bool* lpbValue, const char* lpcName);

        void SetRange(f32* lpfValue, f32 lfMin, f32 lfMax);
        void SetRange(s32* lpiValue, s32 liMin, s32 liMax);
        void SetRange(u32* lpuValue, u32 luMin, u32 luMax);

        void SetStep(f32* lpfValue, f32 lfStep);
        void SetStep(s32* lpiValue, s32 liStep);
        void SetStep(u32* lpuValue, u32 luStep);

        void SetReadOnly(void* lpValue, bool lbReadOnly);
        void SetSaveEnabled(void* lpValue, bool lbSaveEnabled);
        void SetVisible(void* lpValue, bool lbVisible);
        void SetCustomMenuItem(void* lpValue, DebugUI::MenuItemVariable* lpMenuItem);
        void SetOptions(s32* lpiValue, const DebugUI::StringList* lpStringList);
        void SetChangeCallback(void* lpValue, DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData);
        void SetSelectCallback(void* lpValue, DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData);
        void SetVariableName(void* lpValue, const char* lpcName);
        void UnregisterVariable(void* lpValue);

        void RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcGroup, const char* lpcName);
        void RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcName);
        void UnregisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData);
        void SetFunctionName(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcName);

        void AddMenuItem(const char* lpcName, DebugUI::MenuItem* lpMenuItem);
        void AddMenuItem(DebugUI::MenuItem* lpMenuItem);
        void RemoveMenuItem(DebugUI::MenuItem* lpMenuItem);

        void GetComponentPath(char* lpcBuffer, s32 liBufferLen) const;

    protected:
        virtual const char* GetName() const;
        virtual const char* GetPath() const;
        virtual bool        IsSimple() const;
        virtual void        OnActivate();
        virtual void        OnRegister();

        void MakeFullPath(char* lpcBuffer, s32 liBufferLen, const char* lpcLeaf);

    private:
        void DebugUISectionCallback(void* lpUserData);

        bool            mbActive;
        DebugComponent* mpDebugLinkedListNext;
    };
}
