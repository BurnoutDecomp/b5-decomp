// Real named manual-reset channels -> extracted PC input helpers/assignment ->
// actual pad output fields. Two processes exercise the cached default-off gate.
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
#include "GameShared/GameClasses/System/CgsHarnessSlot.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static std::vector<std::string> gLog;
namespace CgsDev { namespace Log {void WriteToLog(const char* value){gLog.emplace_back(value);} } }
namespace {
#include "camera_harness_helpers.inc"
void FillCamera(bool lbXPad,const XInputState& lXState,CgsInput::InputIO::PadOutputInformation& lrPad)
{
#include "camera_harness_delivery.inc"
}
}
int main(int argc,char** argv)
{
    const bool enabled=argc>1 && std::strcmp(argv[1],"enabled")==0;
    _putenv_s("BRN_INPUT_ALLOW_BACKGROUND",enabled?"1":"");
    _putenv_s("BRN_HARNESS_SLOT","99991");
    const char* names[]={"CameraLeft","CameraRight","CameraUp","CameraDown"};
    HANDLE events[4];
    for(unsigned i=0;i<4;++i){
        char base[80],name[96];std::snprintf(base,sizeof(base),"Local\\BurnoutPC_Input_%s",names[i]);
        events[i]=CreateEventA(nullptr,TRUE,FALSE,CgsSystem::HarnessSlot::Name(name,sizeof(name),base));
        if(!events[i])return 2;
    }
    unsigned checks=0,failures=0;
    auto check=[&](bool value,const char* name){++checks;if(!value){++failures;if(failures<=12)std::printf("FAIL %s\n",name);}};
    auto normal=[](short thumb){const float raw=thumb<=0?thumb/32768.f:thumb/32767.f;
        // DeviceX360Pad::Construct pins saturation0.9/deadzone0.2 independently
        // of this harness; the real extracted normalization must retain it.
        return raw<=-.2f?(std::fmax(raw,-.9f)+.2f)/.7f:raw>=.2f?(std::fmin(raw,.9f)-.2f)/.7f:0.f;};
    const short pads[][2]={{0,0},{32767,-32768},{16384,-20000},{-12000,22000}};
    for(unsigned pad=0;pad<4;++pad)for(unsigned mask=0;mask<16;++mask){
        for(unsigned i=0;i<4;++i){ResetEvent(events[i]);if(mask&(1u<<i))SetEvent(events[i]);}
        XInputState state{};state.Gamepad.sThumbRX=pads[pad][0];state.Gamepad.sThumbRY=pads[pad][1];
        const bool attached=pad!=0;
        const float baseX=attached?normal(pads[pad][0]):0,baseY=attached?normal(pads[pad][1]):0;
        float wantedX=baseX,wantedY=baseY;
        if(enabled && mask){wantedX=std::fmax(-1.f,std::fmin(1.f,baseX+((mask&2)?1.f:0)-((mask&1)?1.f:0)));
            wantedY=std::fmax(-1.f,std::fmin(1.f,baseY+((mask&4)?1.f:0)-((mask&8)?1.f:0)));}
        CgsInput::InputIO::PadOutputInformation output{};
        output.mfStickLX=.375f;output.mfStickLY=-.125f;output.maActionInfo[0].mfValue=.75f;
        FillCamera(attached,state,output);
        check(std::fabs(output.mfStickRX-wantedX)<1e-6f,"camera RX delivered with correct sign and accumulation");
        check(std::fabs(output.mfStickRY-wantedY)<1e-6f,"camera RY delivered with correct sign and accumulation");
        check(output.mfStickLX==.375f && output.mfStickLY==-.125f,"left stick unchanged");
        check(output.maActionInfo[0].mfValue==.75f,"driving action unchanged");
        FillCamera(attached,state,output);
        check(std::fabs(output.mfStickRX-wantedX)<1e-6f && std::fabs(output.mfStickRY-wantedY)<1e-6f,"manual hold survives second poll");
        for(HANDLE event:events)ResetEvent(event);
        FillCamera(attached,state,output);
        check(std::fabs(output.mfStickRX-baseX)<1e-6f && std::fabs(output.mfStickRY-baseY)<1e-6f,"release restores actual host axes");
    }
    if(enabled){bool release=false;for(const auto& line:gLog)if(line.find(" held 0 ")!=std::string::npos)release=true;
        check(release,"bounded observer reports actual release");check(gLog.size()<=64,"observer cap respected");}
    else check(gLog.empty(),"default-off gate supplies no camera diagnostics");
    for(HANDLE event:events){ResetEvent(event);CloseHandle(event);}
    std::printf("CameraStickHarness %s: %u checks, %u failures\n",enabled?"enabled":"disabled",checks,failures);
    return failures?1:0;
}
