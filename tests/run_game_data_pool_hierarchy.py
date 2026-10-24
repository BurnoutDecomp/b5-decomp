"""Exercise actual GameData pool preparation with controlled transport/root allocator seams.

Requires the CP12 resource chain and CP13 canonical AppendResourceQueue provider.
Run only in a later authorized validation pass. --output-dir preserves exact receipts.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from fxgs_common import definition

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = next(path for path in REPO.parents if (path / 'tools/build/msvc_env.bat').is_file())
BASE = 'src/GameShared/GameClasses/'
RESOURCE = BASE + 'System/Resource/'
GAME = 'src/GameSource/Resource/BrnGameDataModule.cpp'


def read(path):
    return (REPO / path).read_text(encoding='utf-8-sig')


def production():
    game = read(GAME)
    pool = read(RESOURCE + 'CgsResourcePool.cpp')
    driver = read(RESOURCE + 'CgsPoolModule.cpp')
    manager = read(BASE + 'Development/AssertSystem/CgsAssertManager.cpp')
    memory = read(BASE + 'Memory/CgsMemoryModule.cpp')
    resource = read(RESOURCE + 'CgsResourceModule.cpp')
    allocators = read('src/GameSource/Resource/BrnResourceAllocator.cpp')
    code = 'namespace CgsDev { namespace Assert {\n' + definition(manager, 'Manager::Manager()') + '\n} }\n'
    layout = pool[pool.index('        struct PoolManagementStorageLayout'):pool.index('    // ---- lookup')]
    code += 'namespace CgsResource { namespace {\n' + definition(pool, 'u32 GetManagementHashLength(') + '\n' + layout
    for signature in ('u32 Pool::GetOverheadMemoryRequired(', 'u32 Pool::GetOverheadMemoryAlignment('):
        code += definition(pool, signature) + '\n'
    for signature in ('void PoolModule::ConvertPoolRequestOptions(', 'void PoolModule::GetRequiredResourceDescriptor('):
        code += definition(driver, signature) + '\n'
    code += definition(resource, 'ResourceModule::InitOptions::InitOptions()') + '\n}\n'
    code += 'namespace CgsMemory {\n' + definition(memory, 'MemoryModule::InitOptions::InitOptions()') + '\n}\n'

    start = game.index('    namespace\n', game.index('namespace BrnResource\n'))
    end = game.index('    // "New module"', start)
    code += 'namespace BrnResource {\n' + game[start:end]
    for signature in ('bool GameDataModule::CreateBanks(void*, void*)',
                      'bool GameDataModule::CreatePools(void*, void*)'):
        # Only the owning class qualification changes; every production statement,
        # canonical row, counter, queue operation and host sizing call stays intact.
        code += definition(game, signature).replace('GameDataModule::', 'GameDataPoolFixture::') + '\n'
    start = game.index('        static CgsResource::ResourceModule::InitOptions s_InitOptions;')
    end = game.index('        // [PC diagnostic]', start)
    code += 'RootSnapshot CaptureRootRequests(RootAllocatorDouble* lpAllocator)\n{\n' + game[start:end]
    code += '    RootSnapshot result; result.options = lrMem; result.allocations = lpAllocator->requests; return result;\n}\n'
    # The real startup owner relocates the typed image before GameData construction.
    # Keep those statements, excluding its unrelated default-allocator/logging work.
    start = allocators.index('        CgsMemory::MemoryMap& lrMap = MemoryMapImageInitializer::GetMemoryMap();')
    end = allocators.index('\n\n        mInitialDefaultAllocator.Construct(0);', start)
    code += 'void InitialiseMapForFixture()\n{\n' + allocators[start:end] + '\n}\n}\n'
    return code


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    generated = output / 'GameDataPoolProduction.inc'
    generated.write_text(production(), encoding='utf-8')
    flags_file = WORKFLOW / 'tools/build/msvc_flags.txt'
    includes_file = WORKFLOW / 'tools/build/msvc_includes.txt'
    flags = [line.strip() for line in flags_file.read_text().splitlines() if line.strip() and not line.startswith('#')]
    includes = [line.strip() for line in includes_file.read_text().splitlines() if line.strip() and not line.startswith('#')]
    mapped = [REPO / Path(path.replace('\\', '/')).relative_to('b5-decomp') for path in includes]
    sources = [BASE + 'Core/CgsAssert.cpp', BASE + 'Core/CgsStringUtils.cpp',
        BASE + 'Development/CgsStrStream.cpp', BASE + 'Development/StackUnpick/CgsStackUnpick.cpp',
        BASE + 'Module/CgsIOBuffer.cpp', BASE + 'Module/CgsBaseEventReceiverQueue.cpp',
        BASE + 'Memory/CgsMemoryModuleIO.cpp', BASE + 'Memory/MemoryMap/CgsMemoryMap.cpp',
        RESOURCE + 'CgsResourceModuleIO.cpp', RESOURCE + 'CgsResourceModuleIO_InputBuffer_GetResourceQueue.cpp',
        'vendor/renderware/src/rw/core/debug/DebugCriticalSection.cpp',
        'vendor/renderware/src/rw/BaseResourceDescriptor.cpp']
    options = flags + ['/D_ALLOW_KEYWORD_MACROS=1', '/Dprivate=public', '/Dprotected=public', '/Gw', f'/I"{output}"']
    options += [f'/I"{path}"' for path in mapped]
    options += [f'"{Path(__file__).with_name("GameDataPoolHierarchy.cpp")}"']
    options += [f'"{REPO / path}"' for path in sources]
    options += ['/Fe:GameDataPoolHierarchy.exe']
    response = output / 'fixture.rsp'
    response.write_text('\n'.join(options) + '\n', encoding='utf-8')
    script = output / 'compile.cmd'
    script.write_text('@echo off\ncall "' + str(WORKFLOW / 'tools/build/msvc_env.bat')
        + '" >nul 2>&1\nif errorlevel 1 exit /b 200\ncl @"' + str(response)
        + '" /link /OPT:REF user32.lib\nexit /b %ERRORLEVEL%\n', encoding='utf-8', newline='\r\n')
    references = [GAME, RESOURCE + 'CgsResourcePool.cpp', RESOURCE + 'CgsPoolModule.cpp',
        RESOURCE + 'CgsResourceModule.cpp', BASE + 'Memory/CgsMemoryModule.cpp',
        BASE + 'Development/AssertSystem/CgsAssertManager.cpp',
        'src/GameSource/Resource/BrnResourceAllocator.cpp', 'src/GameSource/Resource/BrnMemoryMapData.h',
        BASE + 'Memory/MemoryMap/CgsMemoryMap.h', RESOURCE + 'CgsResourceIOEvents.h',
        BASE + 'Core/CgsAssert.h']
    inputs = [Path(__file__), Path(__file__).with_name('GameDataPoolHierarchy.cpp'), generated, flags_file, includes_file]
    inputs += [REPO / path for path in dict.fromkeys(sources + references)]
    (output / 'input_manifest.json').write_text(json.dumps([
        {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in inputs
    ], indent=2) + '\n', encoding='utf-8')
    env = os.environ.copy()
    env.pop('NoDefaultCurrentDirectoryInExePath', None)
    compiled = subprocess.run(['cmd', '/c', str(script)], cwd=output, env=env, capture_output=True, timeout=120)
    (output / 'compile.stdout.txt').write_bytes(compiled.stdout)
    (output / 'compile.stderr.txt').write_bytes(compiled.stderr)
    (output / 'compile.exit_code.txt').write_text(str(compiled.returncode) + '\n')
    if compiled.returncode:
        print(compiled.stdout.decode('utf-8', errors='replace'))
        print(compiled.stderr.decode('utf-8', errors='replace'))
        return {'compiled': False, 'compile_exit_code': compiled.returncode, 'execution': 'not attempted'}
    executed = subprocess.run([str(output / 'GameDataPoolHierarchy.exe')], cwd=output, env=env, capture_output=True, timeout=30)
    (output / 'run.stdout.txt').write_bytes(executed.stdout)
    (output / 'run.stderr.txt').write_bytes(executed.stderr)
    (output / 'run.exit_code.txt').write_text(str(executed.returncode) + '\n')
    stdout = executed.stdout.decode('utf-8', errors='replace')
    print(stdout)
    if executed.stderr: print(executed.stderr.decode('utf-8', errors='replace'))
    match = re.search(r'GameDataPoolHierarchy: (\d+) checks, (\d+) failures', stdout)
    return {'compiled': True, 'exit_code': executed.returncode,
        'checks': int(match[1]) if match else 0, 'failures': int(match[2]) if match else None}


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    temporary = None
    if args.output_dir is None:
        temporary = tempfile.TemporaryDirectory(prefix='gamedata_pool_')
        output = Path(temporary.name)
    else:
        output = args.output_dir.resolve()
        if output.exists() and any(output.iterdir()):
            raise SystemExit('Refusing to overwrite receipts: ' + str(output))
    result = run(output)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    failed = not result['compiled'] or result.get('exit_code') != 0 or not result.get('checks') or result.get('failures') != 0
    print('GameDataPoolHierarchy verdict:', 'FAIL' if failed else 'PASS')
    if temporary: temporary.cleanup()
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
