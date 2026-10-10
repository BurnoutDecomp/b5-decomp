#include <cstdio>
static int giChecks=0,giFailures=0;
static void Check(bool lbValue,const char* lpcName)
{++giChecks;if(!lbValue){++giFailures;std::printf("FAIL %s\n",lpcName);}}
using f32=float;
namespace CgsDev::DebugUI {
enum InputEvent {E_INPUTEVENT_NONE,E_INPUTEVENT_SELECT};
struct Manager {int miClosed=0;template<class T>void Close(T*){++miClosed;}};
struct UI {Manager mManager;Manager& GetMenuManager(){return mManager;}};
static UI sUI;UI& GetUI(){return sUI;}
struct Menu {
    Menu* mpParent=nullptr;int miOpened=0;bool mbUseful=false;
    bool IsMenuUseful()const{return mbUseful;}
    void OpenAsWindow(){++miOpened;}
    void Update(f32,InputEvent);
};
}
#include "pc_debug_menu_parents.inc"
int main(){
    using namespace CgsDev::DebugUI;
    Menu lRoot,lChild;lChild.mpParent=&lRoot;
    lChild.Update(0,E_INPUTEVENT_NONE);
    Check(lChild.miOpened==0 && sUI.mManager.miClosed==0,"ordinary updates leave the window stack alone");
    lChild.Update(0,E_INPUTEVENT_SELECT);
    Check(lChild.miOpened==1,"select opens the child menu");
    Check(sUI.mManager.miClosed==0,"a parent containing only submenus remains visible");
    sUI.mManager.miClosed=0;
    lRoot.mbUseful=true;lChild.Update(0,E_INPUTEVENT_SELECT);
    Check(sUI.mManager.miClosed==0,"a parent containing actions remains visible");
    lRoot.Update(0,E_INPUTEVENT_SELECT);
    Check(lRoot.miOpened==1 && sUI.mManager.miClosed==0,"opening the root does not close other menus");
    std::printf("PCDebugMenuParents: %d checks, %d failures\n",giChecks,giFailures);return giFailures?1:0;
}
