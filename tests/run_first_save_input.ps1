param([Parameter(Mandatory)][string]$GameLog, [Parameter(Mandatory)][int]$Slot)
$ErrorActionPreference = 'Stop'
if ($Slot -le 0) { throw 'Use an isolated test slot.' }
$began = Get-Date
$start = [Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::AutoReset,"Local\BurnoutPC_Input_Start_$Slot")
$accept = [Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::AutoReset,"Local\BurnoutPC_Input_Accept_$Slot")
$started = $false
$lastAccept = $began
try {
  while (((Get-Date)-$began).TotalSeconds -lt 245) {
    if ((Test-Path -LiteralPath $GameLog) -and (Get-Item -LiteralPath $GameLog).LastWriteTime -ge $began) {
      $log = Get-Content -LiteralPath $GameLog -Raw
      if (!$started -and $log -match '\[BootLegal\] stage -?\d+ -> 7') {
        [void]$start.Set()
        $started = $true
        $lastAccept = Get-Date
        Write-Output 'Started and signed in at the title screen.'
      }
      $movie = $log.IndexOf("QueueNextMovie: file 'VIDEOS\intro.vp6'")
      $playing = $movie -ge 0 -and $log.IndexOf('[GuiModule] video finished', $movie) -lt 0
      if ($started -and !$playing -and ((Get-Date)-$lastAccept).TotalSeconds -gt 2) {
        [void]$accept.Set()
        $lastAccept = Get-Date
      }
      if ($log -match 'CarSelectManager: Exit state is finished') { break }
    }
    Start-Sleep -Milliseconds 200
  }
} finally {
  $start.Dispose()
  $accept.Dispose()
}
