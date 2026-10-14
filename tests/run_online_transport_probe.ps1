# Compile a small native fixture with the production online transport. Set
# BP_ONLINE_SERVER / BP_ONLINE_KEY to a running server before invoking.
param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
$workflow = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$output = Join-Path $workflow 'scratch/online-v1/probe'
New-Item -ItemType Directory -Force $output | Out-Null
$source = Join-Path $PSScriptRoot 'OnlineTransportProbe.cpp'
$transport = Join-Path $workflow 'b5-decomp/vendor/dirtysdk/src/pc/online/OnlineTransport.cpp'
$exe = Join-Path $output 'OnlineTransportProbe.exe'
$rsp = Join-Path $output 'probe.rsp'
$flags = @('/nologo', '/std:c++17', '/EHsc', '/MD', '/O2', '/D_CRT_SECURE_NO_WARNINGS',
    ('/I"{0}"' -f (Join-Path $workflow 'b5-decomp/src')),
    ('/I"{0}"' -f (Join-Path $workflow 'b5-decomp/vendor/EABase/include/Common')),
    ('"{0}"' -f $source), ('"{0}"' -f $transport),
    ('/Fe"{0}"' -f $exe), ('/Fo"{0}\\"' -f $output), '/link', 'ws2_32.lib', 'bcrypt.lib')
[IO.File]::WriteAllLines($rsp, $flags, [Text.UTF8Encoding]::new($false))
$cmdFile = Join-Path $output 'build.cmd'
$lines = @('@echo off', ('call "{0}"' -f (Join-Path $workflow 'tools/build/msvc_env.bat')),
    'if errorlevel 1 exit /b 1', ('cl @"{0}"' -f $rsp))
[IO.File]::WriteAllText($cmdFile, ($lines -join "`r`n") + "`r`n", [Text.UTF8Encoding]::new($false))
& $cmdFile
if ($LASTEXITCODE -ne 0) { throw 'OnlineTransportProbe build failed' }
if ($BuildOnly) { return }
if (-not $env:BP_ONLINE_SERVER -or -not $env:BP_ONLINE_KEY) { throw 'Set BP_ONLINE_SERVER and BP_ONLINE_KEY first' }
$hostLog = Join-Path $output 'host.log'
$guestLog = Join-Path $output 'guest.log'
$hostProcess = Start-Process -FilePath $exe -ArgumentList 'host' -WindowStyle Hidden -PassThru -RedirectStandardOutput $hostLog -RedirectStandardError ($hostLog + '.err')
$null = $hostProcess.Handle
Start-Sleep -Milliseconds 300
$guestProcess = Start-Process -FilePath $exe -ArgumentList 'guest' -WindowStyle Hidden -PassThru -RedirectStandardOutput $guestLog -RedirectStandardError ($guestLog + '.err')
$null = $guestProcess.Handle
$exitCodes = @()
foreach ($process in @($hostProcess, $guestProcess)) {
    if (-not $process.WaitForExit(20000)) { $process.Kill(); throw 'Transport probe timed out' }
    $exitCodes += $process.ExitCode
}
Get-Content $hostLog
Get-Content $guestLog
if ($exitCodes.Count -ne 2 -or @($exitCodes | Where-Object { $null -eq $_ -or $_ -ne 0 }).Count -ne 0) {
    throw ('Transport interoperability probe failed; exit codes: ' + ($exitCodes -join ','))
}
