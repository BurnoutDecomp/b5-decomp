"""Exercise production paused-trail getters and their cap without a D3D device."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import definition, settings, REPO, WORKFLOW

FIXTURE = r'''
#include <Windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include "pc/gcm/renderengine/TrailPausedDiagPC.h"
struct Matrix44 { f32 ma[16] = {}; };
u32 gPresent=100,gCalls=0,gReleases=0;
namespace renderengine { u32 GetDispatchPresentCountPC() { return gPresent; } }
std::string gLog;
namespace CgsDev { namespace Log { void WriteToLog(const char* p) { gLog+=p; } } }
struct IDirect3DVertexShader9 {
    HRESULT result=S_OK;UINT bytes=496;
    HRESULT GetFunction(void*,UINT* p) { ++gCalls;*p=bytes;return result; }
    void Release() { ++gReleases; }
};
struct IDirect3DDevice9 {
    IDirect3DVertexShader9 vs;
    f32 native[16]={};HRESULT constantsResult=S_OK,shaderResult=S_OK;
    HRESULT GetVertexShaderConstantF(UINT base,f32* p,UINT count) {
        ++gCalls;if(base!=0 || count!=4)return E_INVALIDARG;
        if(SUCCEEDED(constantsResult))std::memcpy(p,native,sizeof(native));return constantsResult;
    }
    HRESULT GetVertexShader(IDirect3DVertexShader9** p) { ++gCalls;
        if(SUCCEEDED(shaderResult))*p=&vs;return shaderResult; }
};
struct WorldFrame { Matrix44 matrix;
    Matrix44 GetViewProjectionMatrix() const { return matrix; } } gBrnWorldShaderConstantsFrameBringUp;
bool gbBrnWorldShaderConstantsFrameBringUpValid=true;
u32 suImVertsStride=28,suImVertsPrimCount=4;
u64 guWorldDrawCalls=123;
f32 sauImVertsScratch[7]={3041.5f,-4.2f,-1973.8f,0,0,0.3f,0.77f};
#include "paused_trail.inc"
int main(int argc,char** argv) {
    if(argc!=2)return 90;const std::string mode=argv[1];
    _putenv_s("BRN_PAUSED_TRAIL_DIAG",mode=="off"?"":"1");
    IDirect3DDevice9 d;f32 expected[16]={};
    for(u32 i=0;i<16;++i)expected[i]=d.native[i]=gBrnWorldShaderConstantsFrameBringUp.matrix.ma[i]=float(i)+0.25f;
    if(mode=="source_stale")gBrnWorldShaderConstantsFrameBringUp.matrix.ma[0]+=1;
    if(mode=="native_overwrite")d.native[0]+=2;
    if(mode=="world_invalid")gbBrnWorldShaderConstantsFrameBringUpValid=false;
    if(mode=="read_fail")d.constantsResult=E_FAIL;
    if(mode=="vs_fail")d.shaderResult=E_FAIL;
    if(mode=="code_fail")d.vs.result=E_FAIL;
    if(mode=="code_zero")d.vs.bytes=0;
    if(mode=="invalid_matrix")expected[0]=NAN;
    if(mode=="wrong_stride")suImVertsStride=20;
    auto expected_at=[&](float now,u32 present){gPresent=present;renderengine::TrailPausedDiag_ExpectedPC(now,expected);};
    auto draw=[&](){TrailPausedDiag_AtDrawPC(&d);};
    if(mode=="moving"){
        for(u32 i=0;i<2000;++i){expected_at(float(i)/60.0f,100+i);draw();}
    }else if(mode=="invalid_clock"){
        expected_at(NAN,100);expected_at(NAN,190);draw();
    }else{
        expected_at(10,100);draw();
        if(mode!="stale_expected")expected_at(10,189);else gPresent=189;
        draw();
        if(mode!="stale_expected")expected_at(10,190);else gPresent=190;
        draw();
        if(mode=="same_present")for(u32 i=0;i<100;++i)draw();
        if(mode=="cap")for(u32 i=1;i<20;++i){expected_at(10,190+i*90);draw();}
        if(mode=="resume"){
            expected_at(11,191);draw();expected_at(11,280);draw();expected_at(11,281);draw();
        }
    }
    std::printf("calls=%u releases=%u\n%s",gCalls,gReleases,gLog.c_str());return 0;
}
'''


def main():
    source = (REPO / "src/pc/gcm/renderengine/XenonD3D9Shims.cpp").read_text(encoding="utf-8-sig")
    start = source.index("    renderengine::TrailPausedClockPC sTrailPausedClock;")
    end = source.index("    // One flag per CAUSE", start)
    methods = source[start:end] + definition(source, "void renderengine::TrailPausedDiag_ExpectedPC(")
    assert not re.search(r"->(?:Set|Draw|Lock|Create)\w*\s*\(", methods)
    draw = definition(source, "void D3DDevice_EndVertices(")
    assert draw.index("TrailPausedDiag_AtDrawPC(lpDevice)") < draw.index("GeometryBindingsPC::DrawPrimitiveUP(")
    begin = definition((REPO / "src/GameSource/Effects/Particles/Native/BrnTrailRender.cpp").read_text(encoding="utf-8-sig"), "void TrailRenderer::BeginRender(")
    assert "TrailPausedDiag_ExpectedPC(mfCurrentTime, &mViewProjectionMatrix.xAxis.x)" in begin
    modes = ("off", "moving", "invalid_clock", "wrong_stride", "stale_expected", "held",
             "source_stale", "native_overwrite", "same_present", "cap", "resume",
             "world_invalid", "read_fail", "vs_fail", "code_fail", "code_zero", "invalid_matrix")
    with tempfile.TemporaryDirectory(prefix="brn_paused_trail_") as directory:
        out = Path(directory)
        (out / "paused_trail.inc").write_text(methods, encoding="utf-8")
        (out / "fixture.cpp").write_text(FIXTURE, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        command = "cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " fixture.cpp /Fe:fixture.exe"
        script = out / "compile.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=out, check=True)
        environment = dict(os.environ)
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        results = {}
        for mode in modes:
            result = subprocess.run([str(out / "fixture.exe"), mode], cwd=out, env=environment,
                                    capture_output=True, text=True, check=True)
            results[mode] = result.stdout
        for mode in ("off", "moving", "invalid_clock", "wrong_stride", "stale_expected"):
            assert "calls=0 releases=0" in results[mode] and "record=" not in results[mode], (mode, results[mode])
        for mode in ("held", "source_stale", "native_overwrite", "same_present"):
            assert results[mode].count("status=observed") == 1, (mode, results[mode])
            assert "heldPresents=90" in results[mode] and "releases=1" in results[mode]
        assert "nativeSourceMaxError=0 sourceWorldMaxError=1" in results["source_stale"]
        assert "nativeSourceMaxError=2 sourceWorldMaxError=0" in results["native_overwrite"]
        for mode in ("world_invalid", "read_fail", "vs_fail", "code_fail", "code_zero", "invalid_matrix"):
            assert "status=INCONCLUSIVE" in results[mode], (mode, results[mode])
        assert "nativeReadable=0" in results["read_fail"]
        assert results["cap"].count("status=observed") == 12
        assert "record=13" not in results["cap"] and "cap=12 reached" in results["cap"]
        assert "calls=36 releases=12" in results["cap"]
        assert results["resume"].count("status=observed") == 2
        assert "present=191 status=" not in results["resume"]
        for mode in modes:
            print(f"PASS {mode}: {results[mode].splitlines()[0]}")
        print(f"{len(modes)} scenarios passed; production getter bodies; no native D3D device")


if __name__ == "__main__":
    main()
