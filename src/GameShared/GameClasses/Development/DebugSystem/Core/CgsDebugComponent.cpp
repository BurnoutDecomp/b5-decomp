#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"        // GetUI().GetXManager(), SafeStringCat
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"       // DebugManager::ThreadSafeAquire/RegisterComponent
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"// DebugInterface (RAII manager accessor)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                // CgsCore::SPrintf

// CgsDev::DebugComponent method bodies. Reconstructed from the X360 ARTIST build (the thin
// per-overload wrappers sub_8282D560 / sub_8282D800 / sub_8282F598 / sub_8282F720, the named
// GetComponentPath 0x82822218 + MakeFullPath 0x828222C8) cross-checked against a real consumer
// (DebugComponentPerfMonCpu::OnActivate 0x82831DC8) and the DecFIGS DWARF.
//
// The pattern every register/set call follows: build a Variant tag for the value, resolve the
// component-relative menu path (GetComponentPath for the leaf form, MakeFullPath for the grouped
// form), then funnel into the shared VariableManager / FunctionManager core reached through
// GetUI(). DebugComponent is a friend of VariableManager, so it calls the private
// (Variant&, path, name) / (void*, Variant&, Variant&) / (void*, Variant&) cores directly - which
// is exactly what the X360 emits (e.g. v6[0]=8 is Variant(bool*) == E_TYPE_PTR_BOOL, v7[0]=2 is
// Variant(s32) == E_TYPE_INT32).
//
// The lifecycle Register (X360 0x828331D8) is reconstructed below: it thread-safe-acquires the
// DebugManager and threads the component onto its list via RegisterComponent / RegisterComponentSimple
// (chosen by IsSimple()). Construct / Destruct are the base two-phase init/teardown over the two
// members (the X360 inlines them; bodies reconstructed from the attested member set).
//
// NOT in this TU yet (the link-closure follow-on): the menu-row helpers AddMenuItem / RemoveMenuItem /
// DebugUISectionCallback (they need the Menu/MenuItem tree), and the VariableManager /
// FunctionManager / DebugManager method bodies themselves. The variable/function registration
// surface below is the keystone that every in-game DebugComponent subclass compiles against.

namespace CgsDev
{
    DebugComponent::DebugComponent()
        : mbActive(false)
        , mpDebugLinkedListNext(nullptr)
    {
    }

    bool DebugComponent::IsActive() const
    {
        return mbActive;
    }

    // ---- lifecycle -----------------------------------------------------------------------------

    // Base two-phase init (CgsDebugComponent.cpp:51). The X360 inlines it; reconstructed over the
    // attested members - the component starts inactive and unlinked from the registration list.
    void DebugComponent::Construct()
    {
        mbActive = false;
        mpDebugLinkedListNext = nullptr;
    }

    // Base teardown (CgsDebugComponent.cpp:79). Marks the component inactive; the X360 keeps no
    // manager-side unregister entry point, so removal from the list is left to the manager.
    void DebugComponent::Destruct()
    {
        mbActive = false;
    }

    // X360 0x828331D8. Thread-safe-acquire the debug manager, then thread this component onto its
    // list - RegisterComponentSimple for a "simple" (flat) component, RegisterComponent for one that
    // wants the full menu hierarchy - keyed on the component's path + name. The lock is released
    // explicitly once registration is done.
    void DebugComponent::Register()
    {
        DebugManager* lpManager = DebugManager::ThreadSafeAquire();
        DebugInterface debugInterface(lpManager);

        if (IsSimple())
            debugInterface.GetDebugManager().RegisterComponentSimple(this, GetPath(), GetName());
        else
            debugInterface.GetDebugManager().RegisterComponent(this, GetPath(), GetName());

        DebugManager::ThreadSafeRelease(lpManager);
    }

    // ---- path resolution -----------------------------------------------------------------------

    // X360 0x82822218. Build the component's own menu path "<path>/<name>" into the caller buffer.
    // GetName() must be non-null (asserted); a null GetPath() degrades to an empty prefix (the X360
    // substitutes the empty string), giving a leading-'/'' path that DebugManager::FindComponentByName
    // tolerates.
    void DebugComponent::GetComponentPath(char* lpcBuffer, s32 liBufferLen) const
    {
        const char* lpcPath = GetPath();
        const char* lpcName = GetName();

        CGS_ASSERT(lpcName, "lpcName");

        if (!lpcPath)
            lpcPath = "";

        CgsCore::SPrintf(lpcBuffer, liBufferLen, "%s/%s", lpcPath, lpcName);
    }

    // X360 0x828222C8. Component path followed by a leaf (a register group), inserting a single '/'
    // separator unless the leaf already starts with one (the path produced by GetComponentPath never
    // ends in a separator, so the X360's end-of-string scan always reaches that conclusion).
    void DebugComponent::MakeFullPath(char* lpcBuffer, s32 liBufferLen, const char* lpcLeaf)
    {
        GetComponentPath(lpcBuffer, liBufferLen);

        if (lpcLeaf)
        {
            if (*lpcLeaf != '\\' && *lpcLeaf != '/')
            {
                s32 liLength = 0;
                while (lpcBuffer[liLength])
                    ++liLength;

                const char lcLast = (liLength > 0) ? lpcBuffer[liLength - 1] : '\0';
                if (lcLast != '\\' && lcLast != '/')
                    GetUI().SafeStringCat(lpcBuffer, "/", liBufferLen);
            }

            GetUI().SafeStringCat(lpcBuffer, lpcLeaf, liBufferLen);
        }
    }

    // ---- variable registration (grouped form: MakeFullPath weaves in the group) ----------------

    void DebugComponent::RegisterVariable(f32* lpfValue, const char* lpcGroup, const char* lpcName)
    {
        DebugUI::Variant variant(lpfValue);     // E_TYPE_PTR_FLOAT
        char acPath[256];
        MakeFullPath(acPath, 256, lpcGroup);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(s32* lpiValue, const char* lpcGroup, const char* lpcName)
    {
        DebugUI::Variant variant(lpiValue);     // E_TYPE_PTR_INT32 (X360 v7[0] = 6)
        char acPath[256];
        MakeFullPath(acPath, 256, lpcGroup);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(u32* lpuValue, const char* lpcGroup, const char* lpcName)
    {
        DebugUI::Variant variant(lpuValue);     // E_TYPE_PTR_UINT32
        char acPath[256];
        MakeFullPath(acPath, 256, lpcGroup);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(bool* lpbValue, const char* lpcGroup, const char* lpcName)
    {
        DebugUI::Variant variant(lpbValue);     // E_TYPE_PTR_BOOL
        char acPath[256];
        MakeFullPath(acPath, 256, lpcGroup);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    // ---- variable registration (leaf form: path is the component's own) ------------------------

    void DebugComponent::RegisterVariable(f32* lpfValue, const char* lpcName)
    {
        DebugUI::Variant variant(lpfValue);
        char acPath[256];
        GetComponentPath(acPath, 256);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(s32* lpiValue, const char* lpcName)
    {
        DebugUI::Variant variant(lpiValue);
        char acPath[256];
        GetComponentPath(acPath, 256);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(u32* lpuValue, const char* lpcName)
    {
        DebugUI::Variant variant(lpuValue);
        char acPath[256];
        GetComponentPath(acPath, 256);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    void DebugComponent::RegisterVariable(bool* lpbValue, const char* lpcName)
    {
        DebugUI::Variant variant(lpbValue);     // X360 sub_8282D800: v6[0] = 8
        char acPath[256];
        GetComponentPath(acPath, 256);
        GetUI().GetVariableManager().RegisterVariable(variant, acPath, lpcName);
    }

    // ---- range / step (immediate-valued Variants: X360 v7[0] = 2 == E_TYPE_INT32) --------------

    void DebugComponent::SetRange(f32* lpfValue, f32 lfMin, f32 lfMax)
    {
        DebugUI::Variant minVariant(lfMin);
        DebugUI::Variant maxVariant(lfMax);
        GetUI().GetVariableManager().SetRange(lpfValue, minVariant, maxVariant);
    }

    void DebugComponent::SetRange(s32* lpiValue, s32 liMin, s32 liMax)
    {
        DebugUI::Variant minVariant(liMin);
        DebugUI::Variant maxVariant(liMax);
        GetUI().GetVariableManager().SetRange(lpiValue, minVariant, maxVariant);
    }

    void DebugComponent::SetRange(u32* lpuValue, u32 luMin, u32 luMax)
    {
        DebugUI::Variant minVariant(luMin);
        DebugUI::Variant maxVariant(luMax);
        GetUI().GetVariableManager().SetRange(lpuValue, minVariant, maxVariant);
    }

    void DebugComponent::SetStep(f32* lpfValue, f32 lfStep)
    {
        DebugUI::Variant stepVariant(lfStep);
        GetUI().GetVariableManager().SetStep(lpfValue, stepVariant);
    }

    void DebugComponent::SetStep(s32* lpiValue, s32 liStep)
    {
        DebugUI::Variant stepVariant(liStep);
        GetUI().GetVariableManager().SetStep(lpiValue, stepVariant);
    }

    void DebugComponent::SetStep(u32* lpuValue, u32 luStep)
    {
        DebugUI::Variant stepVariant(luStep);
        GetUI().GetVariableManager().SetStep(lpuValue, stepVariant);
    }

    // ---- per-variable attributes (forwarded straight to the manager) ---------------------------

    void DebugComponent::SetReadOnly(void* lpValue, bool lbReadOnly)
    {
        GetUI().GetVariableManager().SetReadOnly(lpValue, lbReadOnly);
    }

    void DebugComponent::SetSaveEnabled(void* lpValue, bool lbSaveEnabled)
    {
        GetUI().GetVariableManager().SetSaveEnabled(lpValue, lbSaveEnabled);
    }

    void DebugComponent::SetVisible(void* lpValue, bool lbVisible)
    {
        GetUI().GetVariableManager().SetVisible(lpValue, lbVisible);
    }

    void DebugComponent::SetCustomMenuItem(void* lpValue, DebugUI::MenuItemVariable* lpMenuItem)
    {
        GetUI().GetVariableManager().SetCustomMenuItem(lpValue, lpMenuItem);
    }

    void DebugComponent::SetOptions(s32* lpiValue, const DebugUI::StringList* lpStringList)
    {
        GetUI().GetVariableManager().SetOptions(lpiValue, lpStringList);
    }

    void DebugComponent::SetChangeCallback(void* lpValue, DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData)
    {
        GetUI().GetVariableManager().SetChangeCallback(lpValue, lpfCallback, lpUserData);
    }

    void DebugComponent::SetSelectCallback(void* lpValue, DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback, void* lpUserData)
    {
        GetUI().GetVariableManager().SetSelectCallback(lpValue, lpfCallback, lpUserData);
    }

    void DebugComponent::SetVariableName(void* lpValue, const char* lpcName)
    {
        GetUI().GetVariableManager().SetVariableName(lpValue, lpcName);
    }

    void DebugComponent::UnregisterVariable(void* lpValue)
    {
        GetUI().GetVariableManager().UnregisterVariable(lpValue);
    }

    // ---- function (action) registration --------------------------------------------------------

    void DebugComponent::RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcGroup, const char* lpcName)
    {
        char acPath[256];
        MakeFullPath(acPath, 256, lpcGroup);
        GetUI().GetFunctionManager().RegisterFunction(lpfCallback, lpUserData, acPath, lpcName);
    }

    void DebugComponent::RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcName)
    {
        char acPath[256];                       // X360 sub_8282F720
        GetComponentPath(acPath, 256);
        GetUI().GetFunctionManager().RegisterFunction(lpfCallback, lpUserData, acPath, lpcName);
    }

    void DebugComponent::UnregisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData)
    {
        GetUI().GetFunctionManager().UnregisterFunction(lpfCallback, lpUserData);
    }

    void DebugComponent::SetFunctionName(DebugUI::Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcName)
    {
        GetUI().GetFunctionManager().SetFunctionName(lpfCallback, lpUserData, lpcName);
    }

    // ---- overridable hooks (base defaults; real components override the meaningful ones) --------

    void        DebugComponent::Update()                              {}
    void        DebugComponent::RenderWorld(Debug3DImmediateRender*)  {}
    void        DebugComponent::RenderHUD(Debug2DImmediateRender*)    {}
    const char* DebugComponent::GetName() const                      { return ""; }
    const char* DebugComponent::GetPath() const                      { return ""; }
    bool        DebugComponent::IsSimple() const                     { return false; }
    void        DebugComponent::OnActivate()                          {}
    void        DebugComponent::OnRegister()                          {}

    // X360: ActivateComponent(this) -> GetComponentPath -> MenuManager::GetMenuFromPath -> Open. The
    // menu-open path (DebugManager::ActivateComponent / MenuManager::GetMenuFromPath / Open) is the
    // menu-navigation follow-on; stubbed here - this fires only when a debug section is selected,
    // never during loading.
    void DebugComponent::DebugUISectionCallback(void* lpUserData)
    {
        (void)lpUserData;
    }
}
