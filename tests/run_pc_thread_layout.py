"""Exercise the original frame handshake with native EAThread and a Win32 window."""
from pathlib import Path
import os
import sys
import tempfile
from fxgs_common import REPO, STRSTREAM_CPP, compile_and_run, definition, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
ea = REPO / "vendor/EAThread/source"
sources = [REPO / "src/GameShared/GameClasses/System/Threads/CgsThreadLayout.cpp",
           REPO / "src/GameShared/GameClasses/System/Timer/CgsTimeUtils.cpp", STRSTREAM_CPP]
sources += [REPO / "src/GameShared/GameClasses/Core/CgsAssert.cpp",
            REPO / "src/GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.cpp",
            REPO / "vendor/renderware/src/rw/core/debug/DebugCriticalSection.cpp"]
sources += [ea / path for path in ("eathread.cpp", "eathread_mutex.cpp", "eathread_barrier.cpp",
            "eathread_condition.cpp", "pc/eathread_thread_pc.cpp", "pc/eathread_semaphore_pc.cpp",
            "pc/eathread_x360align.cpp", "pc/eathread_callstack_win64.cpp")]
with tempfile.TemporaryDirectory(prefix="brn_threadlayout_control_") as directory:
    if "--blocking-wait-service" in sys.argv:
        source = sources[3].read_text(encoding="utf-8")
        needle = "        if (gAssertMutex.miInitialised && !TryEnterCriticalSection(&gAssertMutex.mCriticalSection))\n            return;"
        assert needle in source
        source = source.replace(needle, "        gAssertMutex.Enter(); // negative: block the window owner")
        sources[3] = Path(directory) / "CgsAssert.cpp"
        sources[3].write_text(source, encoding="utf-8")
    if "--no-wait-service" in sys.argv:
        for index in (0, 3):
            source = sources[index].read_text(encoding="utf-8")
            needle = "            CgsDev::Assert::ServiceWorkerAssertsWhileWaitingPC();" if index == 0 else "            ServiceWorkerAssertsWhileWaitingPC();"
            assert needle in source
            source = source.replace(needle, "            ; // negative: block without consuming the dependent worker's assertion")
            copy = Path(directory) / sources[index].name
            copy.write_text(source, encoding="utf-8")
            sources[index] = copy
    if "--lock-before-fence" in sys.argv:
        source = sources[3].read_text(encoding="utf-8")
        source = source.replace("            // Join BEFORE taking gAssertMutex:",
            "            if (!gbDeferredAssertPC) gAssertMutex.Enter();\n            // Join BEFORE taking gAssertMutex:", 1)
        source = source.replace("        if (!gbDeferredAssertPC) gAssertMutex.Enter();\n        return 0;",
            "        else if (!gbDeferredAssertPC) gAssertMutex.Enter();\n        return 0;", 1)
        sources[3] = Path(directory) / "CgsAssert.cpp"
        sources[3].write_text(source, encoding="utf-8")
    if "--no-message-pump" in sys.argv:
        source = sources[0].read_text(encoding="utf-8")
        before = "MsgWaitForMultipleObjectsEx(1, &lhHandle, INFINITE,\n                QS_ALLINPUT, MWMO_INPUTAVAILABLE)"
        assert before in source
        source = source.replace(before, "WaitForSingleObject(lhHandle, INFINITE)", 1)
        sources[0] = Path(directory) / "CgsThreadLayout.cpp"
        sources[0].write_text(source, encoding="utf-8")
    file_source = (REPO / "src/GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.cpp").read_text(encoding="utf-8")
    file_wait = "\n".join(definition(file_source, signature) for signature in (
        "AsyncOp::AsyncOp()", "void AsyncOp::Wait()", "void AsyncOp::CompletionCallback("))
    result = compile_and_run(Path(__file__).with_name("PCThreadLayout.cpp"), "file_wait.inc", file_wait,
                             "PCThreadLayout", extra_sources=sources,
                             extra_flags="/Gy /Gw user32.lib advapi32.lib dbghelp.lib")
# The foreign request may need more than one frame to reach the owner; each
# frame also checks the dispatch handshake. The fixed checks total at least 82.
raise SystemExit(report("run_pc_thread_layout", [], result, 82))
