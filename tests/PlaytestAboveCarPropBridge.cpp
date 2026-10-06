// Actual bridge body and payload/queue types; module IO getters are test boundaries.
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

namespace CgsDev { namespace Assert {
static int assertions;
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} namespace Log { decltype(gpDebugPrint) gpDebugPrint=nullptr; } }
u64 CgsDev::Message::gxMessageFilterFlags=0;

struct RequestInterface { CgsModule::VariableEventQueue<1024,16> mRequestQueue; };
struct WorldRequests { CgsModule::VariableEventQueue<4096,16> mRequestQueue; };
namespace BrnWorldIO {
struct UpdateOutputBuffer {
    WorldRequests requests;
    CgsModule::VariableEventQueue<32768,16> gui;
    WorldRequests* GetResourceRequestResourceInterface(){return &requests;}
    CgsModule::VariableEventQueue<32768,16>* GetGuiEventQueue(){return &gui;}
}; }
namespace BrnWorld { namespace PropEntityIO {
struct OutputBuffer_PreScene {
    RequestInterface requests;
    BrnGui::GuiOverheadSignInfoEvent::VisibleOverheadSignArray signs;
    const RequestInterface* GetResourceRequestInterface() const{return &requests;}
    const BrnGui::GuiOverheadSignInfoEvent::VisibleOverheadSignArray* GetVisibleOverheadSignArray() const{return &signs;}
}; } }
#include "playtest_prop_sign_bridge.inc"
static int checks,failures;
static void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
int main(){
    static BrnWorldIO::UpdateOutputBuffer world;
    static BrnWorld::PropEntityIO::OutputBuffer_PreScene prop;
    world.requests.mRequestQueue.Construct();world.gui.Construct();
    prop.requests.mRequestQueue.Construct();prop.signs.Construct();
    u32 request=0xAB123456u;prop.requests.mRequestQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&request),37,4);
    const int counts[]={0,1,32};
    for(int count : counts) {
        world.requests.mRequestQueue.Clear();world.gui.Clear();prop.signs.Clear();
        for(int i=0;i<count;++i){BrnGui::OverheadSignScore s{};s.mWorldSpacePosition={float(i),float(i+1),float(i+2),0};s.muEntityIndex=u16(500+i);prop.signs.Append(s);}
        WorldModule::BridgePropToOutput_PreScene(nullptr,&world,&prop);
        Check(world.requests.mRequestQueue.GetLength()==1,"resource request leg retained");
        const CgsModule::Event* event=nullptr;s32 bytes=0;const s32 id=world.gui.GetFirstEvent(&event,&bytes);
        Check(id==210&&bytes==1040&&event,"every frame emits original sign event and byte extent");
        if(event){
            const auto& result=reinterpret_cast<const BrnGui::GuiOverheadSignInfoEvent*>(event)->mVisibleOverheadSignArray;
            Check(result.GetLength()==u32(count),"empty and full sign counts copied");
            for(int i=0;i<count;++i)Check(result.GetItem(i).muEntityIndex==500+i&&result.GetItem(i).mWorldSpacePosition.z==i+2,"complete sign records copied");
        }
    }
    Check(CgsDev::Assert::assertions==0,"no assertions");
    std::printf("PlaytestAboveCarPropBridge: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
