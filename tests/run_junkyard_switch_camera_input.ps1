param([string]$GameLog,[int]$Slot)
$ErrorActionPreference = 'Stop'
if ($Slot -le 0) { throw 'Use a test slot.' }
$began = Get-Date
$handles = @{}
foreach ($name in @('Accept','OptionNext','OptionPrev')) {
  $handles[$name] = [Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::AutoReset,"Local\BurnoutPC_Input_${name}_$Slot")
}
try {
  $deadline = $began.AddSeconds(65)
  while ((Get-Date) -lt $deadline) {
    if ((Test-Path $GameLog) -and (Get-Item $GameLog).LastWriteTime -ge $began) {
      $log = Get-Content $GameLog -Raw
      if ($log -match 'CSV : Entering Car Select') { break }
    }
    Start-Sleep -Milliseconds 250
  }
  if ((Get-Date) -ge $deadline) { throw 'No vehicle menu.' }
  Start-Sleep -Seconds 4
  $handles.Accept.Set() | Out-Null
  Write-Output 'dismissed title-update popup'
  Start-Sleep -Seconds 3
  foreach ($name in @('OptionNext','OptionNext','OptionPrev')) {
    $handles[$name].Set() | Out-Null
    Write-Output ("{0:o}: {1}" -f (Get-Date),$name)
    Start-Sleep -Seconds 7
  }
  $handles.Accept.Set() | Out-Null
  Start-Sleep -Seconds 4
  $handles.Accept.Set() | Out-Null
} finally {
  foreach ($handle in $handles.Values) { $handle.Dispose() }
}
