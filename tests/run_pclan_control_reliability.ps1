param([switch]$EpochTests)
$ErrorActionPreference = 'Stop'
$workflow = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$output = Join-Path $workflow $(if ($EpochTests) { 'scratch/online-v1/epoch-fences' } else { 'scratch/online-v1/control-reliability' })
New-Item -ItemType Directory -Force $output | Out-Null
$testName = if ($EpochTests) { 'OnlineEpochTests' } else { 'PcLanControlReliabilityTests' }
$source = Join-Path $PSScriptRoot ($testName + '.cpp')
$exe = Join-Path $output ($testName + '.exe')
$rsp = Join-Path $output 'test.rsp'
$flags = @('/nologo', '/std:c++17', '/EHsc', '/MD', '/O2', '/D_CRT_SECURE_NO_WARNINGS',
    ('/I"{0}"' -f (Join-Path $workflow 'b5-decomp/src')),
    ('/I"{0}"' -f (Join-Path $workflow 'b5-decomp/vendor/EABase/include/Common')),
    ('"{0}"' -f $source), ('/Fe"{0}"' -f $exe), ('/Fo"{0}\\"' -f $output), '/link', 'ws2_32.lib')
[IO.File]::WriteAllLines($rsp, $flags, [Text.UTF8Encoding]::new($false))
$cmdFile = Join-Path $output 'build.cmd'
$lines = @('@echo off', ('call "{0}"' -f (Join-Path $workflow 'tools/build/msvc_env.bat')),
    'if errorlevel 1 exit /b 1', ('cl @"{0}"' -f $rsp))
[IO.File]::WriteAllText($cmdFile, ($lines -join "`r`n") + "`r`n", [Text.UTF8Encoding]::new($false))
& $cmdFile
if ($LASTEXITCODE -ne 0) { throw 'Control reliability test build failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Control reliability test failed' }
