#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // DebugComponent (mbActive/OnRegister/DebugUISectionCallback/RenderHUD)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetVariableManager()/GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"        // Variant
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mp2dRender Begin/End/SetRenderBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT

// CgsDev::DebugManager - the in-game debug systems owner.
//
// Boot/loading path: the per-frame tick is a no-op so the game-module update spine links and runs;
// the full Update (driving the UI) is the manager-construction follow-on.
//
// Registration/lifecycle (what DebugComponent::Register drives): ThreadSafeAquire/Release bracket
// the singleton behind the debug critical section (X360 0x821F1E50 / ThreadSafeRelease);
// RegisterComponent[Simple] thread the component onto the list and surface it in the menu - Simple
// as an mbActive bool toggle (X360 0x8282E1E0), the full form as a section callback row
// (X360 0x82832008). Both clear mbActive and fire the component's OnRegister.
//
// The debug critical section is the same single-threaded-boot no-op as the assert mutex; the real
// lock is deferred to the threading wiring. mpUI is wired by DebugManager::Construct (the
// manager-construction follow-on); GetUI hands it back.

namespace CgsDev
{
    DebugManager* DebugManager::mpInstance = nullptr;

    DebugManager::DebugManager()
        : mpUI(nullptr)
        , mp2dRender(nullptr)
        , mp3dRender(nullptr)
    {
        mComponentList.Clear();
    }

    DebugManager::~DebugManager() {}

    void DebugManager::Update(f32) {}

    DebugManager* DebugManager::ThreadSafeAquire()
    {
        CGS_ASSERT(mpInstance, "mpInstance");
        // X360 enters the debug-manager critical section here (no-op on the single-threaded boot).
        return mpInstance;
    }

    void DebugManager::ThreadSafeRelease(DebugManager* lpDebugManager)
    {
        // X360 leaves the debug-manager critical section here (no-op on the single-threaded boot).
        CGS_ASSERT(lpDebugManager == mpInstance, "lpDebugManager == mpInstance");
    }

    DebugUI::DebugUI& DebugManager::GetUI()
    {
        return *mpUI;
    }

    bool DebugManager::IsComponentRegistered(DebugComponent* lpComponent)
    {
        return mComponentList.IsAdded(lpComponent);
    }

    // X360 0x82832008. Register a component as a full menu section: a function row that, when
    // selected, fires DebugComponent::DebugUISectionCallback for it.
    void DebugManager::RegisterComponent(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        if (lpcName)
            GetUI().GetFunctionManager().RegisterFunction(&DebugComponent::DebugUISectionCallback, lpComponent, lpcPath, lpcName);

        lpComponent->OnRegister();
    }

    // X360 0x8282E1E0. Register a "simple" component: a single bool row wired to its mbActive flag.
    void DebugManager::RegisterComponentSimple(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");
        CGS_ASSERT(lpcName, "lpcName");

        if (!lpcPath)
            lpcPath = "";

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        GetUI().GetVariableManager().RegisterVariable(DebugUI::Variant(&lpComponent->mbActive), lpcPath, lpcName);

        lpComponent->OnRegister();
    }

    // X360 RenderHUD 0x8282E108 (2D screen-space pass - the debug squares): open the 2D renderer,
    // let each active component draw its HUD, close. The X360 also flushes the buffered debug prims
    // (DebugRender::Dispatch2D over mBufferedRenderer) and renders the DebugUI menus between Begin and
    // the component loop; those two paths are the buffered-render / menu-render follow-on.
    void DebugManager::RenderHUD()
    {
        mp2dRender->Begin();

        for (DebugComponent* lpComponent = mComponentList.GetFirst();
             lpComponent;
             lpComponent = mComponentList.GetNext(lpComponent))
        {
            if (lpComponent->IsActive())
                lpComponent->RenderHUD(mp2dRender);
        }

        mp2dRender->End();
    }

    // X360 Render 0x8282F770: point the renderers at this frame's buffers, then RenderWorld + RenderHUD.
    // The 3D path (mp3dRender setup + RenderWorld) is the Debug3D follow-on; the 2D path drives the
    // squares into lp2dRenderBuffer.
    void DebugManager::Render(const Matrix44& /*lViewProjection*/, const Vector3& /*lCameraPosition*/,
                             CgsGraphics::Im2d* /*lp3dRenderBuffer*/, CgsGraphics::Im2d* lp2dRenderBuffer)
    {
        mp2dRender->SetRenderBuffer(lp2dRenderBuffer);
        RenderHUD();
    }
}
