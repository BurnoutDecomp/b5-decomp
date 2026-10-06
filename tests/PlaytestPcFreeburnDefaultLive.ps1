# Two real Easy Drive create/quick-match flows using the normal PC LAN default.
# The existing pair verifies both cars spawn and receive remote transform updates.
param([string]$Role = '')
$lWorkflowRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lCase = & (Join-Path $lWorkflowRoot 'tools/tests/cases/net_ui_join.ps1') -Role $Role
$lCase.Name = if ($Role) { "playtest_pc_freeburn_default_$Role" } else { 'playtest_pc_freeburn_default' }
if ($Role) {
    $lOriginalSetup = $lCase.Setup
    $lCase.Setup = {
        param($ctx)
        & $lOriginalSetup $ctx | Out-Null
        Remove-Item Env:BP_LAN -ErrorAction SilentlyContinue
        Write-Host '[case] PC Freeburn default: BP_LAN is unset; no forced service sign-in.'
    }.GetNewClosure()
}
return $lCase
