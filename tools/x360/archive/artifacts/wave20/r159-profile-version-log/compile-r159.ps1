$ErrorActionPreference = 'Stop'
$repo = 'C:\Users\adri1\.codex\worktrees\bp360\ui_cpp_luna'
$out = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r159-profile-version-log'
$baseReport = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-composed-preflight\compile-results.json'
$baseReportSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $baseReport).Hash.ToLowerInvariant()
$base = Get-Content -LiteralPath $baseReport -Raw | ConvertFrom-Json
$guiProfile = $base.results | Where-Object { $_.source -eq 'src/GameSource/Gui/BrnGuiProfile.cpp' } | Select-Object -First 1
if (-not $guiProfile -or -not $guiProfile.passed) { throw 'R157 sealed XDK command template unavailable or failed.' }
New-Item -ItemType Directory -Force -Path (Join-Path $out 'objects') | Out-Null
$compiler = $guiProfile.command[0]
$compilerSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $compiler).Hash.ToLowerInvariant()
$srcRootOld = 'C:\Users\adri1\.codex\worktrees\bp360\debug_format_luna'
$sources = @(
  'src\GameSource\Gui\BrnGuiModule.cpp',
  'src\GameSource\Gui\BrnGuiProfile.cpp',
  'src\GameSource\Network\Parameters\BrnNetworkGameSearchParams.cpp',
  'src\GameSource\Network\Managers\BrnNetworkStateManager_wN1_03.cpp',
  'src\GameSource\Network\Managers\BrnNetworkPlayerStatsManager.cpp',
  'src\GameSource\Network\Managers\BrnNetworkLiveRevengeManager.cpp',
  'src\GameSource\Network\Debug Components\BrnNetworkLiveRevengeDebugComponent.cpp'
)
$results = @()
Push-Location $repo
try {
  foreach ($source in $sources) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($source)
    $obj = Join-Path $out ("objects\" + $stem + '.obj')
    $asm = Join-Path $out ("objects\" + $stem + '.asm')
    $log = Join-Path $out ("objects\" + $stem + '.log')
    $callArgs = @()
    foreach ($arg in $guiProfile.command[1..($guiProfile.command.Count - 1)]) {
      if ($arg.StartsWith('/Fo', [StringComparison]::OrdinalIgnoreCase)) {
        $callArgs += '/Fo' + $obj
      } elseif ($arg.StartsWith('/Fa', [StringComparison]::OrdinalIgnoreCase)) {
        $callArgs += '/Fa' + $asm
      } elseif ($arg.StartsWith('/I', [StringComparison]::OrdinalIgnoreCase) -and $arg.Contains($srcRootOld)) {
        $callArgs += $arg.Replace($srcRootOld, $repo)
      } elseif ($arg -eq $guiProfile.command[-1]) {
        $callArgs += $source
      } else {
        $callArgs += $arg
      }
    }
    & $compiler @callArgs *> $log
    $exit = $LASTEXITCODE
    $objHash = if (Test-Path -LiteralPath $obj) { (Get-FileHash -Algorithm SHA256 -LiteralPath $obj).Hash.ToLowerInvariant() } else { $null }
    $asmHash = if (Test-Path -LiteralPath $asm) { (Get-FileHash -Algorithm SHA256 -LiteralPath $asm).Hash.ToLowerInvariant() } else { $null }
    $logHash = if (Test-Path -LiteralPath $log) { (Get-FileHash -Algorithm SHA256 -LiteralPath $log).Hash.ToLowerInvariant() } else { $null }
    $results += [pscustomobject]@{
      source = $source.Replace('\', '/')
      passed = ($exit -eq 0 -and $objHash -and $asmHash)
      returncode = $exit
      machine = 498
      object = $obj
      object_sha256 = $objHash
      assembly = $asm
      assembly_sha256 = $asmHash
      log = $log
      log_sha256 = $logHash
      command = @($compiler) + $callArgs
    }
  }
} finally {
  Pop-Location
}
$sourcePath = Join-Path $repo 'src\GameSource\Network\Managers\BrnNetworkLiveRevengeManager.h'
$proof = [pscustomobject]@{
  stage = 'focused-XDK-compile-only'
  target = 'xbox360-ppc-be'
  source_base_commit = '49484358f743078512f861ab783ee194c998744b'
  source_base_tree = '7794ea26f68bd5170fcf9a64f06cb9f82304271a'
  precommit_source_blob = (git -C $repo hash-object -- 'src/GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h')
  precommit_source_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash.ToLowerInvariant()
  compiler = $compiler
  compiler_sha256 = $compilerSha
  sdk = $base.sdk
  command_template = $guiProfile.command
  command_template_report = $baseReport
  command_template_report_sha256 = $baseReportSha
  source_root_rebased_from = $srcRootOld
  source_root = $repo
  requested = $sources.Count
  results = $results
}
$proofPath = Join-Path $out 'compile-results.json'
$proof | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $proofPath -Encoding utf8
$passed = @($results | Where-Object { -not $_.passed }).Count -eq 0
"compile passed=$passed count=$($results.Count) compiler_sha256=$compilerSha template_report_sha256=$baseReportSha"
if (-not $passed) {
  $results | Where-Object { -not $_.passed } | ForEach-Object { "$($_.source): rc=$($_.returncode) log=$($_.log)" }
  exit 1
}

