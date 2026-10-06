"""Run the production director-live gate and world handoff, including paused cameras.

--rev HEAD before the repair exercises the exact old rejected states. No GPU,
game, trail geometry or simulated clock is created by this fixture.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, settings, REPO, WORKFLOW

FIXTURE = r'''
#include <cstdio>
#include <cstring>
#include <cmath>
#include <initializer_list>
#include "types.hpp"
struct Vector { f32 x=0,y=0,z=0,w=0; };
struct Matrix44Affine { Vector xAxis,yAxis,zAxis,wAxis; };
namespace rw { namespace math { namespace vpu { using Matrix44Affine=::Matrix44Affine; } } }
namespace CgsDev { namespace Log {
struct Stream { template<class T> Stream& operator<<(T) { return *this; } };
Stream* gpDebugPrint=nullptr;
} }
namespace BrnDirector {
class Arbitrator { public:
#include "arb_enum.inc"
    EState state=E_STATE_PREPARE;EState GetState() const { return state; }
};
struct GameState { enum {E_JY_INACTIVE=0,E_JY_ACTIVE=1};s32 meJunkyardState=E_JY_INACTIVE; };
struct MainDirector {
    bool flyby=false;GameState game;Arbitrator arb;
    bool IsGameIntroFlybyActive() const { return flyby; }
    const GameState& GetGameState() const { return game; }
    const Arbitrator& GetArbitrator() const { return arb; }
};
namespace Camera {
struct Effects { bool setTimeOfDay=false;f32 timeOfDay=0;
    bool IsTimeOfDaySet() const { return setTimeOfDay; }f32 GetTimeOfDay() const { return timeOfDay; } };
struct Camera {
    Matrix44Affine transform;f32 fov=72;bool junkyard=false;u32 mState_uFlags=123;Effects effects;
    const Matrix44Affine& GetTransform() const { return transform; }
    f32 GetFOV() const { return fov; }bool IsInJunkyard() const { return junkyard; }
    const Effects& GetEffects() const { return effects; }
};
} }
struct DirectorModule { BrnDirector::MainDirector director;
    const BrnDirector::MainDirector& GetMainDirector() const { return director; } };
struct World {
    u32 calls=0,flags=0;Matrix44Affine transform;f32 fov=0,time=0;bool junkyard=false,setTime=false;
    void SetBringUpCameraOverride(const Matrix44Affine& pose,f32 field,bool jy,bool hasTime,f32 timeOfDay,u32 cameraFlags) {
        ++calls;transform=pose;fov=field;junkyard=jy;setTime=hasTime;time=timeOfDay;flags=cameraFlags;
    }
};
struct Bridge {
    DirectorModule mDirectorModule;World mWorldModule;bool mbDirectorCameraLive=false;
    void Update(bool lbPostGui) {
#include "camera_gate.inc"
    }
    void Dispatch(const BrnDirector::Camera::Camera* lpDispatchCamera) {
#include "camera_dispatch.inc"
    }
};
u32 checks=0,failures=0;
void Check(bool value,const char* name) {
    ++checks;if(!value){++failures;if(failures<=16)std::printf("FAIL %s\n",name);}
}
BrnDirector::Camera::Camera CameraAt(u32 step) {
    BrnDirector::Camera::Camera c;const f32 angle=float(step)*0.05f;
    c.transform.xAxis={std::cos(angle),0,-std::sin(angle),0};
    c.transform.yAxis={0,1,0,0};c.transform.zAxis={std::sin(angle),0,std::cos(angle),0};
    c.transform.wAxis={3040+3*std::sin(angle),-3.8f,-1937.0f+3*std::cos(angle),1};
    c.fov=72+float(step)*0.1f;c.effects={true,16.5f};return c;
}
int main() {
    using A=BrnDirector::Arbitrator;
    const auto camera=CameraAt(1);
    for(int state=0;state<=9;++state) {
        Bridge b;b.mDirectorModule.director.arb.state=static_cast<A::EState>(state);
        b.Update(false);b.Dispatch(&camera);
        const bool expected=state==A::E_STATE_NORMAL || state==A::E_STATE_CRASH_NAV || state==A::E_STATE_CRASH_NAV_ICE_CAMERAS;
        Check(b.mbDirectorCameraLive==expected,"exact accepted outer states include both original paused cameras");
        Check((b.mWorldModule.calls==1)==expected,"accepted valid camera reaches the original world handoff");
        for(int reason=0;reason<2;++reason) {
            Bridge active;active.mDirectorModule.director.arb.state=static_cast<A::EState>(state);
            active.mDirectorModule.director.flyby=reason==0;
            active.mDirectorModule.director.game.meJunkyardState=reason==1?BrnDirector::GameState::E_JY_ACTIVE:0;
            active.Update(false);active.Dispatch(&camera);
            Check(active.mbDirectorCameraLive && active.mWorldModule.calls==1,"existing flyby and junkyard admission remains");
        }
        for(bool previous:{false,true}) {
            Bridge post;post.mbDirectorCameraLive=previous;post.mDirectorModule.director.arb.state=static_cast<A::EState>(state);
            post.Update(true);Check(post.mbDirectorCameraLive==previous,"post-GUI pass preserves the pre-GUI gate decision");
        }
    }
    for(auto state:{A::E_STATE_NORMAL,A::E_STATE_CRASH_NAV,A::E_STATE_CRASH_NAV_ICE_CAMERAS}) {
        Bridge b;b.mDirectorModule.director.arb.state=state;b.Update(false);
        BrnDirector::Camera::Camera origin;b.Dispatch(&origin);
        Check(b.mWorldModule.calls==0,"origin guard still protects world streaming in every admitted state");
        b.Dispatch(nullptr);Check(b.mWorldModule.calls==0,"absent camera cannot publish");
    }
    Bridge paused;
    for(auto state:{A::E_STATE_NORMAL,A::E_STATE_CRASH_NAV_ICE_CAMERAS,A::E_STATE_CRASH_NAV,A::E_STATE_NORMAL}) {
        paused.mDirectorModule.director.arb.state=state;
        for(u32 step=0;step<30;++step) {
            const auto moving=CameraAt(step+u32(state)*30u);const u32 before=paused.mWorldModule.calls;
            paused.Update(false);paused.Dispatch(&moving);
            Check(paused.mWorldModule.calls==before+1,"paused/orbiting camera continues publication each render frame");
            Check(std::memcmp(&paused.mWorldModule.transform,&moving.transform,sizeof(moving.transform))==0,
                  "world receives the exact same valid director pose while paused");
            Check(paused.mWorldModule.fov==moving.fov && paused.mWorldModule.flags==moving.mState_uFlags,
                  "world retains current FOV and discrete camera flags");
        }
    }
    paused.mDirectorModule.director.arb.state=A::E_STATE_RELEASE;const u32 before=paused.mWorldModule.calls;
    paused.Update(false);paused.Dispatch(&camera);
    Check(!paused.mbDirectorCameraLive && paused.mWorldModule.calls==before,"release state keeps its existing rejection");
    std::printf("PCPausedWorldCamera: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision; use HEAD before the repair for RED")
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read("src/GameSource/Game/BrnGameModule.cpp")
    marker = source.index("const BrnDirector::MainDirector& lrMainDirector = mDirectorModule.GetMainDirector();")
    gate_start = source.rfind("if (!lbPostGui)", 0, marker)
    gate = definition(source[gate_start:], "if (!lbPostGui)")
    dispatch = definition(definition(source, "int BrnGameModule::DoDispatch("),
                          "if (lpDispatchCamera != 0 && mbDirectorCameraLive)")
    arb = tree.read("src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.h")
    enum = definition(arb, "enum EState") + ";\n"
    with tempfile.TemporaryDirectory(prefix="brn_paused_camera_") as directory:
        out = Path(directory)
        for name, text in (("fixture.cpp", FIXTURE), ("camera_gate.inc", gate),
                           ("camera_dispatch.inc", dispatch), ("arb_enum.inc", enum)):
            (out / name).write_text(text, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        script = out / "run.cmd"
        command = "cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " fixture.cpp /Fe:fixture.exe"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 90\n' + command
                          + '\nif errorlevel 1 exit /b 90\nfixture.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        environment = dict(os.environ)
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        result = subprocess.run(["cmd", "/c", str(script)], cwd=out, env=environment,
                                capture_output=True, text=True)
        print(result.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
