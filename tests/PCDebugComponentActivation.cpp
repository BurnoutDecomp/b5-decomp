#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <thread>
#include <initializer_list>
#include "pc/debug/ComponentActivation.h"
using f32 = float;
using s32 = int;
using u32 = unsigned;
#define CGS_ASSERT(value, ...) do { if (!(value)) std::abort(); } while (0)

namespace CgsDev {
namespace Log {void WriteToLog(const char*){}}
using RGBA = u32;
struct Debug2DImmediateRender {
    RGBA mFill=0,mFrame=0,mText=0;const char* mMessage=nullptr;
    void DrawBox(f32,f32,f32,f32,RGBA c){mFill=c;}
    void DrawFrame(f32,f32,f32,f32,RGBA c,f32){mFrame=c;}
    void DrawTextInBox(const char* t,f32,f32,f32,f32,f32,RGBA c,f32){mMessage=t;mText=c;}
};
struct DebugInterface {};
struct DebugComponent;
namespace DebugUI {
enum InputEvent {E_INPUTEVENT_NONE,E_INPUTEVENT_SELECT,E_INPUTEVENT_BACK,E_INPUTEVENT_CLOSE};
struct Palette {RGBA mColourErrorText=0xEF204080u,mColourErrorWindow=0xD0010203u;};
struct Metrics {f32 mfTextSize=16,mfErrorWindowBorder=4;};
struct Function {};
struct MenuItemFunction {};
struct Menu {};
struct FunctionManager {
    bool mbRegistered=true;int miRemoved=0;Function mFunction;MenuItemFunction mItem;
    Function* FindFunction(void(*)(void*),void*){return mbRegistered?&mFunction:nullptr;}
    MenuItemFunction* FindMenuItem(Function*){return &mItem;}
    void UnregisterFunction(void(*)(void*),void*){mbRegistered=false;++miRemoved;}
};
struct MenuManager {
    bool mbHasMenu=false;int miMoved=0,miOpened=0;Menu mMenu;
    Menu* GetMenuFromPath(const char*,void*){return mbHasMenu?&mMenu:nullptr;}
    void MoveItemAfter(MenuItemFunction*,Menu*){++miMoved;}
    void Open(Menu*){++miOpened;}
};
struct UI {
    Palette mPalette;Metrics mMetrics;MenuManager mMenus;FunctionManager mFunctions;
    int miErrors=0,miRemoved=0;bool mbVisible=false;
    MenuManager& GetMenuManager(){return mMenus;}
    FunctionManager& GetFunctionManager(){return mFunctions;}
    const Palette& GetPalette(){return mPalette;}
    const Metrics& GetMetrics(){return mMetrics;}
    void ShowErrorMessage(const char*){++miErrors;mbVisible=true;}
    template<class T>void RemoveWindow(T*){++miRemoved;}
};
static UI sUI;
UI& GetUI(){return sUI;}
struct Window {
    void Render(Debug2DImmediateRender*){}
    f32 GetX(){return 10;}f32 GetY(){return 20;}
    f32 GetWidth(){return 640;}f32 GetHeight(){return 144;}
};
struct ErrorWindow : Window {
    const char* mpcErrorMessage="Unavailable diagnostic controls";f32 mfPulse=0;
    void Render(Debug2DImmediateRender*);
    void Update(f32,InputEvent);
};
struct ScriptInterface {
    int miMessages=0;
    bool ExecuteScriptComponent(const char*,const char**,s32);
    static void ScriptCommand_ActivateComponent(ScriptInterface*,const char**,s32);
    void OutputMessage(const char*){++miMessages;}
};
}
struct DebugComponent {
    virtual ~DebugComponent()=default;
    bool mbActive=false,mbControls=false;int miActivations=0;
    bool IsActive(){return mbActive;}
    void OnActivate(){++miActivations;if(mbControls)DebugUI::sUI.mMenus.mbHasMenu=true;}
    void GetComponentPath(char* p,int){std::strcpy(p,"Test component");}
    static void DebugUISectionCallback(void*);
};
struct DebugManager {
    DebugComponent* mpComponent=nullptr;
    static DebugManager* GetInstance();
    DebugUI::UI& GetUI(){return DebugUI::sUI;}
    DebugComponent* FindComponentByName(const char* p){return std::strcmp(p,"missing")?mpComponent:nullptr;}
    void ActivateComponent(DebugComponent*);
};
static DebugManager sManager;
DebugManager* DebugManager::GetInstance(){return &sManager;}
namespace DebugUI {DebugManager& GetDebugManager(){return sManager;}}
}
#include "pc/debug/ComponentCapabilities.h"
#include "pc_debug_component_activation.inc"
static int giChecks=0,giFailures=0;
static void Check(bool value,const char* name)
{++giChecks;if(!value){++giFailures;std::printf("FAIL %s\n",name);}}
static void Reset(CgsDev::DebugComponent& component)
{
    CgsDev::DebugUI::sUI={};CgsDev::sManager.mpComponent=&component;
    CgsPC::Debug::sbComponentMenuRequest=false;
}
int main()
{
    using namespace CgsDev;using namespace CgsDev::DebugUI;
    for(bool visible : {false,true}) {
        DebugComponent hud;Reset(hud);sUI.mbVisible=visible;
        sManager.ActivateComponent(&hud);
        Check(hud.IsActive(),"engine activation accepts a component with no menu controls");
        Check(sUI.miErrors==0 && sUI.mbVisible==visible,"network-style activation never opens a modal");
        Check(sUI.mFunctions.miRemoved==1,"headless activation retires its one-shot action as in ARTIST");
        sManager.ActivateComponent(&hud);
        Check(hud.miActivations==1,"an already-active HUD component is not reactivated");
    }
    DebugComponent unsupported;Reset(unsupported);
    DebugComponent::DebugUISectionCallback(&unsupported);
    Check(!unsupported.IsActive() && sUI.miErrors==1,"an explicit unavailable menu request explains itself");
    Check(sUI.mFunctions.mbRegistered,"unavailable menu retains its action row");
    DebugComponent::DebugUISectionCallback(&unsupported);
    Check(sUI.miErrors==2 && unsupported.miActivations==2,"unavailable menu can be retried after dismissal");
    Check(!CgsPC::Debug::sbComponentMenuRequest,"menu context ends before subsequent engine updates");
    DebugComponent hud;Reset(hud);ScriptInterface script;const char* parameters[]={"Test component"};
    ScriptInterface::ScriptCommand_ActivateComponent(&script,parameters,1);
    Check(sUI.miErrors==1 && !hud.IsActive(),"explicit COMPONENT command retains unavailable-page feedback");
    Reset(hud);
    Check(script.ExecuteScriptComponent("Test component",nullptr,0) && sUI.miErrors==1,
          "component-name command uses the same explicit request context");
    DebugComponent supported;supported.mbControls=true;Reset(supported);
    DebugComponent::DebugUISectionCallback(&supported);
    Check(supported.IsActive() && supported.miActivations==1 && sUI.miErrors==0,
          "supported menu activates without an error");
    Check(sUI.mMenus.miOpened==1 && sUI.mMenus.miMoved==1 && sUI.mFunctions.miRemoved==1,
          "supported menu replaces its action and opens normally");
    for(int mode=0;mode<3;++mode) {
        CgsNetwork::VersionDisplay banner;Reset(banner);
        if(mode==0)DebugComponent::DebugUISectionCallback(&banner);
        else if(mode==1)ScriptInterface::ScriptCommand_ActivateComponent(&script,parameters,1);
        else script.ExecuteScriptComponent("Test component",nullptr,0);
        Check(banner.IsActive() && sUI.miErrors==0,"a recovered HUD-only component also accepts explicit activation");
        Check(sUI.mFunctions.miRemoved==1,"explicit HUD-only activation retires its one-shot action");
    }
    {
        CgsPC::Debug::ComponentMenuRequest outer;
        {CgsPC::Debug::ComponentMenuRequest inner;}
        Check(CgsPC::Debug::sbComponentMenuRequest,"nested requests preserve their enclosing context");
        bool other=true;std::thread thread([&]{other=CgsPC::Debug::sbComponentMenuRequest;});thread.join();
        Check(!other,"UI request context does not turn another thread's HUD activation into a menu request");
    }
    Check(!CgsPC::Debug::sbComponentMenuRequest,"outer context restores ordinary activation");
    ErrorWindow error;Debug2DImmediateRender renderer;
    for(float pulse : {0.0f,127.5f,255.0f,-127.5f}) {
        error.mfPulse=pulse;error.Render(&renderer);
        Check(renderer.mFill==sUI.mPalette.mColourErrorWindow,"error dialog uses its separate background colour");
        Check(renderer.mText==sUI.mPalette.mColourErrorText,"error message uses the unscaled readable text colour");
        const RGBA expected=std::fabs(pulse)==127.5f?0xEF102040u:
            (pulse==0?0xEF000000u:sUI.mPalette.mColourErrorText);
        Check(renderer.mFrame==expected,"only the error frame pulses its text colour");
    }
    Check(std::strcmp(renderer.mMessage,error.mpcErrorMessage)==0,"error text reaches the wrapped-text renderer");
    for(InputEvent event : {E_INPUTEVENT_SELECT,E_INPUTEVENT_BACK,E_INPUTEVENT_CLOSE}) {
        const int before=sUI.miRemoved;error.Update(0,event);
        Check(sUI.miRemoved==before+1,"confirm, Back and close dismiss the error dialog");
    }
    std::printf("PCDebugComponentActivation: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures?1:0;
}
