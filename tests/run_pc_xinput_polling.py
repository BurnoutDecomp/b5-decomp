"""Compile and run the actual PC pad source; no real controller is queried."""
from pathlib import Path
import subprocess
import tempfile
from fxgs_common import WORKFLOW, REPO, STRSTREAM_CPP, settings

with tempfile.TemporaryDirectory(prefix='brn_xinput_poll_') as directory:
    output = Path(directory)
    source = REPO / 'src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp'
    (output / 'pc_xinput_polling.inc').write_bytes(source.read_bytes())
    includes = '/I"' + str(output) + '" ' + ' '.join(
        '/I"' + str(WORKFLOW / path) + '"' for path in settings('msvc_includes.txt'))
    executable = output / 'regression.exe'
    command = ('cl ' + ' '.join(settings('msvc_flags.txt')) +
               ' /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public ' + includes +
               ' "' + str(Path(__file__).with_name('PCXInputPolling.cpp')) + '" "' +
               str(STRSTREAM_CPP) + '" /Fe:"' + str(executable) + '" /link /OPT:REF')
    script = output / 'build.cmd'
    script.write_text('@echo off\ncall "' + str(WORKFLOW / 'tools/build/msvc_env.bat') +
                      '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command +
                      '\nexit /b %ERRORLEVEL%\n', newline='\r\n')
    built = subprocess.run(['cmd','/c',str(script)],cwd=output,capture_output=True,text=True)
    if built.returncode:
        print(built.stdout,built.stderr)
        raise SystemExit(built.returncode)
    for args in ([],['--control']):
        run = subprocess.run([str(executable),*args],cwd=output,capture_output=True,text=True,timeout=30)
        print(run.stdout,run.stderr)
        if run.returncode:
            raise SystemExit(run.returncode)
