from pathlib import Path
import sys


here = Path(__file__).resolve().parent
source = Path(sys.argv[1]) / "src/GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace(
    "namespace CgsFileSystem\n{",
    """namespace CgsFileSystem
{
    static bool ProbeBootMedia(const char* name)
    {
        return std::strstr(name, ".VP6") || std::strstr(name, ".vp6")
            || std::strstr(name, ".SNS") || std::strstr(name, ".sns");
    }""",
)
replace(
    "        GetDeviceManager()->Open(macFileName, 1, &StreamDeviceDiskRead::OpenCallback, this, miHighPriority);",
    """        if (ProbeBootMedia(macFileName))
        {
            char lacProbe[280];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R99_STREAM_OPEN this=%08X oldStatus=%d pending=%d file=%.96s\\n",
                reinterpret_cast<u32>(this), static_cast<s32>(meStatus),
                miPendingOperationCount, macFileName);
            OutputDebugStringA(lacProbe);
        }
        GetDeviceManager()->Open(macFileName, 1, &StreamDeviceDiskRead::OpenCallback, this, miHighPriority);""",
)
anchor = '        CGS_ASSERT(meStatus == E_STATUS_OPENING, "Incorrect stream state\\n");'
replace(
    anchor,
    """        if (ProbeBootMedia(macFileName) || liResult != 0)
        {
            char lacProbe[280];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R99_STREAM_OPEN_COMPLETE this=%08X status=%d result=%d pending=%d file=%.96s\\n",
                reinterpret_cast<u32>(this), static_cast<s32>(meStatus),
                liResult, miPendingOperationCount, macFileName);
            OutputDebugStringA(lacProbe);
        }
""" + anchor,
)
anchor = """    bool StreamDeviceDiskRead::StartAsyncReadInternal(void** lppData, u32* lpuAmount)
    {"""
replace(
    anchor,
    anchor + """
        if (meStatus != E_STATUS_OPEN)
        {
            char lacProbe[280];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R99_STREAM_BAD_READ this=%08X status=%d pending=%d blocks=%d close=%d seek=%d file=%.96s\\n",
                reinterpret_cast<u32>(this), static_cast<s32>(meStatus),
                miPendingOperationCount, miBlocksUsed, mbWaitingToClose, mbWaitingToSeek, macFileName);
            OutputDebugStringA(lacProbe);
        }""",
)
anchor = "        if (meStatus == E_STATUS_CLOSED || mbWaitingToClose)"
replace(
    anchor,
    """        if (ProbeBootMedia(macFileName))
        {
            char lacProbe[280];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R99_STREAM_CLOSE this=%08X status=%d pending=%d file=%.96s\\n",
                reinterpret_cast<u32>(this), static_cast<s32>(meStatus),
                miPendingOperationCount, macFileName);
            OutputDebugStringA(lacProbe);
        }
""" + anchor,
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n' + body)
