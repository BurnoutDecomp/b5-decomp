from pathlib import Path
import re
import strip_host as s


def read(name):
    return (s.ROOT / name).read_bytes().decode("utf-8-sig").replace("\r\n", "\n")


def write(name, text):
    (s.ROOT / name).write_bytes(text.replace("\n", "\r\n").encode("utf-8"))


name = "src/CMakeLists.txt"
text = read(name)
text = re.sub(r"if \(WIN32\)\n    add_executable.*?endif\(\)\n", 'add_executable(${PROJECT_NAME} "")\n\ntarget_compile_definitions(${PROJECT_NAME} PRIVATE D_PLATFORM_X360=1 CGS_PLATFORM_X360=1 _XBOX=1)\n', text, count=1, flags=re.S)
text = re.sub(r"if \(WIN32\)\n.*?endif\(\)\n", "", text, flags=re.S)
write(name, text)

name = "src/GameShared/GameClasses/Core/CgsAssert.cpp"
text = read(name)
text = re.sub(r"    void\* EndAssert\(\)\n    \{.*?\n    \}", "    void* EndAssert()\n    {\n        return gAssertMutex.Leave();\n    }", text, count=1, flags=re.S)
for line in ['#include <atomic>\n','#include <cstring>\n','#include <exception>\n','#include "eathread/eathread.h"\n','#include "eathread/eathread_semaphore.h"\n']:
    text = text.replace(line, "")
write(name, text)

name = "src/GameShared/GameClasses/System/Timer/CgsFrameRate.cpp"
text = read(name)
start = text.index("// ============================================================================")
end = text.index("namespace CgsSystem", start)
text = text[:start] + "// ARTIST FrameRateManager: Prepare 0x828D7730, StartUpdateFrame 0x828D7760,\n// UpdatePostRenderWait 0x828D7770. One simulation step is required per display frame.\n" + text[end:]
text = text.replace("HostPerformanceCounter", "PerformanceCounter").replace("HostPerformanceFrequency", "PerformanceFrequency")
text = re.sub(r"        // A zero step would divide.*?mi64SimulationTimeStepCycles = li64Frequency / 60;\n", "", text, count=1, flags=re.S)
needle = "        const s64 li64Time = PerformanceCounter();   // asm 0x828D778C\n"
assert needle in text
text = text.replace(needle, needle + "\n        if (!mbPaused && miNumSimulationSteps + miPrevNumSimulationStepsRequired <= 0)\n        {\n            miPrevNumSimulationStepsRequired = liMinSimulationSteps;\n            mi64PrevFrameTime = li64Time;\n            return liMinSimulationSteps;\n        }\n", 1)
needle = "        s32 liNumSimulationStepsRequired = liMinSimulationSteps;\n"
assert needle in text
text = text.replace(needle, needle + "        if (li64TimeBehind > mi64FramerateToleranceCycles)\n        {\n            const s64 li64Numerator = li64TimeBehind + mi64SimulationTimeStepCycles\n                                      - mi64FramerateToleranceCycles - 1;\n            liNumSimulationStepsRequired = liMinSimulationSteps\n                + static_cast<s32>(li64Numerator / mi64SimulationTimeStepCycles);\n        }\n", 1)
write(name, text)

name = "src/GameShared/GameClasses/Graphics/CgsCamera.cpp"
text = read(name)
start = text.index("    // ================================================================================\n    // [FLAG PC-platform leaf] THE DEPTH ROW")
end = text.index("    void Camera::UpdatePerspectiveProjectionMatrix()", start)
text = text[:start] + text[end:]
text = text.replace("lfFar / (lfFar - lfNear), 1.0f };            // v17, D3D row", "(lfNear + lfFar) / (lfFar - lfNear), 1.0f }; // v17")
text = text.replace("-lfNearTimesFar / (lfFar - lfNear), 0.0f };  // v16, D3D row", "(-2.0f * lfNearTimesFar) / (lfFar - lfNear), 0.0f }; // v16")
write(name, text)

name = "src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp"
text = read(name)
start = text.index("    void Device::ResetProgramShadows()")
end = text.index("\n    }", start)
body = text[start:end]
body = re.sub(r"        mp(?:BlendState|DepthStencilState|RasterizerState)\s*= nullptr;\n", "", body)
text = text[:start] + body + text[end:]
write(name, text)

name = "src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
text = read(name)
text = re.sub(r"        // FLAG PC-platform: the movie files.*?static u16 ByteSwap\(u16 luValue\) \{ return luValue; \}\n", """        // Movie chunk headers are little-endian; Xenon swaps them before use.
        static u32 ByteSwap(u32 luValue)
        {
            return (luValue >> 24) | ((luValue >> 8) & 0x0000FF00u)
                 | ((luValue << 8) & 0x00FF0000u) | (luValue << 24);
        }
        static s32 ByteSwap(s32 liValue) { return static_cast<s32>(ByteSwap(static_cast<u32>(liValue))); }
        static u16 ByteSwap(u16 luValue) { return static_cast<u16>((luValue >> 8) | (luValue << 8)); }
        static s16 ByteSwap(s16 lsValue) { return static_cast<s16>(ByteSwap(static_cast<u16>(lsValue))); }
""", text, count=1, flags=re.S)
write(name, text)

name = "src/GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.cpp"
text = read(name).replace('"FSM\\\\%s.BUNDLE"', '"Fsm\\\\%s.dat"')
write(name, text)

name = "src/SDKs/EATech/include/Apt/AptCIH.h"
text = read(name)
if "static const unsigned int KU_AptEmbeddedMovieOff" not in text:
    text = text.replace("struct AptNativeHash;", "struct AptNativeHash;\n\n// ARTIST embeds the timeline after the 16-byte AptCharacter header.\nstatic const unsigned int KU_AptEmbeddedMovieOff = 0x10u;")
write(name, text)

write("src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h", """#pragma once

#include "types.hpp"
#include <cstddef>

namespace CgsGui
{
    // ARTIST AptDataHeaderType::FixUp 0x828559D8 / FixDown 0x8285D980.
    struct AptDataHeader
    {
        enum EAptDataState
        {
            E_APTDATASTATE_LOADING = 0,
            E_APTDATASTATE_LOADED = 1,
            E_APTDATASTATE_ACTIVE = 2,
        };

        u32 mpacMovieName;
        u32 mpAptData;
        u32 mpConstData;
        u32 mpGeomStruct;
        u32 muSizeOfHeader;
        u32 meCurrentState;

        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta, bool lbEndianSwap);
    };
    static_assert(offsetof(AptDataHeader, mpAptData) == 4, "ARTIST AptDataHeader.data");
    static_assert(offsetof(AptDataHeader, mpConstData) == 8, "ARTIST AptDataHeader.const");
    static_assert(offsetof(AptDataHeader, mpGeomStruct) == 12, "ARTIST AptDataHeader.geometry");
}
""")
write("src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.cpp", """#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

namespace CgsGui
{
    // ARTIST 0x828559D8: relocate the four offsets, then descend into geometry.
    void AptDataHeader::FixUp(u32 luDelta)
    {
        mpConstData += luDelta;
        mpAptData += luDelta;
        mpacMovieName += luDelta;
        mpGeomStruct += luDelta;
        reinterpret_cast<CgsResource::GuiGeometryObject*>(mpGeomStruct)->FixUp(luDelta);
    }

    // ARTIST 0x8285D980: release geometry relocation before restoring file offsets.
    void AptDataHeader::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        reinterpret_cast<CgsResource::GuiGeometryObject*>(mpGeomStruct)->FixDown(luDelta, lbEndianSwap);
        mpGeomStruct -= luDelta;
        mpConstData -= luDelta;
        mpAptData -= luDelta;
        mpacMovieName -= luDelta;
    }
}
""")

# These are complete host replacement implementations, with declarations and
# separate recovered console TUs remaining in the source tree.
for name in [
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp",
]:
    (s.ROOT / name).unlink(missing_ok=True)

print("Applied native camera, timing, endian, assert, and Apt relocation fixes")
