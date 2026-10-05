"""Fault reports must survive a stalled normal writer without taking its locks."""
from pathlib import Path
import os
from fxgs_common import Tree, STRSTREAM_CPP, compile_and_run, definition, report
import sys

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ.pop('BRN_LOG_ASYNC', None)
os.environ['BRN_LOG_PROFILE'] = '0'
source = Tree().read('src/GameShared/GameClasses/Development/Log/CgsLog.cpp')
crash = Tree().read('src/GameShared/GameClasses/System/PC/CgsCrashHandlerPC.cpp')
entries = '\n'.join(definition(crash, name) for name in ('void PurecallProbe()', 'void TerminateProbe()'))
heap = definition(crash, 'if (lhHeap != NULL && HeapValidate(')
heap = heap.replace('CgsDev::StackUnpick', 'StackUnpickFixture')
entries += '''\nvoid HeapFailureProbe() {
HANDLE lhHeap = GetProcessHeap();
unsigned long long gsuNextHeapCheckMs = 0;
const auto HeapValidate = [](HANDLE, DWORD, const void*) { return FALSE; };
''' + heap + '\n}\n'
if '--truncate-emergency' in sys.argv:
    source = source.replace('"BrnGame.emergency.log"', '"BrnGame.log"')
if '--truncate-basename' in sys.argv:
    old = '''if (luDir + std::strlen(lpcName) >= MAX_PATH) std::strcpy(lacPath, lpcName);
            else std::strcpy(lacPath + luDir, lpcName);'''
    assert source.count(old) == 1
    source = source.replace(old, '''lacPath[luDir] = '\\0';
            std::strncat(lacPath, lpcName, MAX_PATH - luDir - 1);''')
if '--queue-fatal-prefix' in sys.argv:
    entries = entries.replace('CgsDev::Log::WriteToLogEmergency(', 'Emit(')
if '--queue-heap-report' in sys.argv:
    entries = entries.replace('CgsDev::Log::CriticalLogScopePC lCriticalLog;', '')
result = compile_and_run(Path(__file__).with_name('PCLogEmergency.cpp'),
    'pc_log_emergency_source.inc', source, 'PCLogEmergency',
    extra_flags='/Gy /Gw /EHsc user32.lib /DPC_LOG_LONG_PATH=' + str(int('--long-module-path' in sys.argv)), extra_sources=[STRSTREAM_CPP],
    extra_files={'pc_log_crash_entries.inc': entries})
raise SystemExit(report('run_pc_log_emergency', [], result, 12))
