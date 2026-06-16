#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // DebugComponent (mbActive/OnRegister/DebugUISectionCallback/RenderHUD)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetVariableManager()/GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"        // Variant
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mp2dRender Begin/End/SetRenderBuffer/Construct
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"     // Internal::SetDebugSingletons
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
    // The debug system's process-wide instances (X360 CgsDebugManager.cpp:85/90): the UI and the
    // renderers are single global objects the manager wires itself to - not heap-allocated. (The 3D
    // renderer g3dInternalDebugRender + the perfmon globals are the render/perfmon follow-on.)
    DebugUI::DebugUI       gInternalDebugUI;
    Debug2DImmediateRender g2dInternalDebugRender;

    // X360 CgsDebugManager.cpp:113 DebugManagerConstructParameters::DEFAULT - the built-in debug
    // configuration the engine boots with. The pool sizes are reconstructed generously (the exact
    // X360 data-section values are TBD; they affect only capacity, not behaviour - the bounded
    // loading build registers only the perfmon). mpRwAllocator is null: the debug pools' backing
    // currently comes from the global heap (CgsDebugCollections.cpp), which ignores the allocator;
    // the real GetDefaultAllocator wiring is the allocator follow-on.
    const DebugManagerConstructParameters DebugManagerConstructParameters::DEFAULT =
    {
        /* miPerfMonCpuCount          */ 64,
        /* miPerfMonLogBufferSize     */ 8192,
        /* miMenuWindowPoolSize       */ 16,
        /* miMenuPoolSize             */ 64,
        /* miFunctionPoolSize         */ 128,
        /* miVariablePoolSize         */ 256,
        /* miVariableMetadataPoolSize */ 256,
        /* miConsoleLineCount         */ 64,
        /* mpRwAllocator              */ nullptr,
    };

    DebugManager* DebugManager::mpInstance = nullptr;

    DebugManager::DebugManager()
        : mpUI(nullptr)
        , mp2dRender(nullptr)
        , mp3dRender(nullptr)
    {
        mComponentList.Clear();
    }

    DebugManager::~DebugManager() {}

    // X360 Construct 0x828332C0 (bounded). Claim the singleton, wire the debug-internal accessors,
    // reset the component list + renderer pointers, then construct the UI (-> the three managers ->
    // their pools). The full X360 bring-up also creates the rw debug Manager instance + default
    // allocator, the debug critical section, the two VariableEventQueues, and the perfmon log buffer;
    // those are the bring-up follow-on (none is needed to construct + render the perfmon HUD - the
    // pools take their backing from the global heap, so the allocator is threaded through but unused).
    void DebugManager::Construct(const DebugManagerConstructParameters* lpParameters)
    {
        mpInstance = this;
        mp2dRender = nullptr;
        mp3dRender = nullptr;
        mComponentList.Clear();

        mpUI = &gInternalDebugUI;

        // Wire the singletons every DebugInternal-derived class reaches through GetUI()/
        // GetDebugManager()/GetAllocator(); must precede the manager Constructs + any registration.
        Internal::SetDebugSingletons(this, mpUI, lpParameters->mpRwAllocator);

        mpUI->Construct(lpParameters);
    }

    // X360 ConstructRenderer (CgsDebugManager.cpp:248, bounded). Construct the 2D debug renderer at
    // the screen's virtual resolution and hand it to the UI. The X360 reads that size from the UI
    // Metrics (a deferred heavy member), so the bounded path uses the loading screen's render
    // resolution directly - the Im2d space the squares are drawn in (pixel coords, 1280x720). The 3D
    // renderer + the assert-overlay renderer hookup (Assert::Manager::SetRenderer) are the render
    // follow-on. The renderer's backing/allocator is unused (the box path batches into a fixed array).
    void DebugManager::ConstructRenderer()
    {
        const f32 lfVirtualScreenWidth  = 1280.0f;
        const f32 lfVirtualScreenHeight = 720.0f;

        mp2dRender = &g2dInternalDebugRender;
        mp2dRender->Construct(nullptr, lfVirtualScreenWidth, lfVirtualScreenHeight);

        mpUI->Set2DRenderer(mp2dRender);
    }

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
