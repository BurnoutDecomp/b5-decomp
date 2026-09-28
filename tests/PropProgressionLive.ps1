# L4 WORLDVFX (owner's list 2026-09-27) -- "The already broken smashes/billboards are not visually broken or fallen to
# the ground". A smash gate / billboard the SAVE records as broken must stream in broken after a reboot.
#
# THE DEFECT. The console hands the profile's hit-prop bits back to the prop world in a handshake inside
# GameStateModule::ProcessGameEvents @0x823A0A18: OnProfileLoaded @0x82397310 -- case 8 at the boot (the MemoryCard
# exit; the GUI's 352 -> 109 is dropped in that video state), case 109 on an in-game load -- posts action 194; the
# world drops its props and asks back with game event 112; case 112 sets mbPropSystemNeedsProgression (+292288); the
# dispatcher's tail posts action 199 carrying &profile.mabHitPropBitArray (gsm+107224). The world copies the 300000
# bits into PropZoneManager::maPreviouslyHitProps, and PropZoneManager::LoadProp leaves a hit E_DONT_RESPAWN prop out
# and swaps a hit E_RESPAWN_CHANGED prop for its broken alternative type. On the PC nothing ran OnProfileLoaded, no
# arm consumed 112 and nothing posted 194 or 199: the world only ever knew the props THIS session broke.
# Unit tests: tests/run_prop_progression.py, tests/run_profile_delivery.py.
#
# THE DRIVE. A returning-player boot on a PROGRESSION save (the case seeds build\game\Memcard_<slot>\Profile.sav from a
# COPY -- default scratch\OWNERLIST_0927\L4\profile\Profile.owner_converted.sav, or $env:BRN_PROPPROG_PROFILE -- and
# refuses slot 0, whose Memcard\Profile.sav is the owner's). Then the car drives out of the junkyard, so zones stream in.
# The witnesses: [GameStateModule::OnProfileLoaded] (always on), and (NOT IN THE X360 BINARY, BRN_PROPPROG_DIAG=1)
# [propprog] lines from the GameState arm (112 -> flag, 199 posted with the profile's hit-prop count), from
# PropEntityModule::PreSceneUpdate (194 received, the
# bits installed) and from PropZoneManager::LoadProp (every don't-respawn / respawn-changed prop that streams in: zone,
# index, respawn type, hit, loaded, type, position). NO FRAME DUMP (the frame pair is scratch/OWNERLIST_0927/evidence/L4).
#
# EXPECTED ON THE PRE-FIX EXE: FAIL -- no OnProfileLoaded line and no [propprog] line at all (no arm, no witness), and
# its world never receives a hit prop. ON THE FIXED EXE: every check passes.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/PropProgressionLive.ps1 -Slot <n>

$lProfileSource = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'scratch\OWNERLIST_0927\L4\profile\Profile.owner_converted.sav'
if ($env:BRN_PROPPROG_PROFILE) { $lProfileSource = $env:BRN_PROPPROG_PROFILE }

$lSetup = {
  param($ctx)
  if ($ctx.Slot -le 0) {
    Write-Host "[case] Setup: REFUSED -- slot 0's Memcard\Profile.sav is the owner's save. Run this case with -Slot <n>, n > 0."
    return
  }
  if (-not (Test-Path $lProfileSource)) {
    Write-Host "[case] Setup: no progression save at $lProfileSource -- the slot boots whatever Memcard_$($ctx.Slot) holds."
    return
  }
  $lDst = Join-Path $ctx.Root ("build\game\Memcard_" + $ctx.Slot)
  New-Item -ItemType Directory -Force $lDst | Out-Null
  Copy-Item $lProfileSource (Join-Path $lDst 'Profile.sav') -Force
  Write-Host "[case] Setup: seeded $lDst\Profile.sav from a copy of $lProfileSource"
}.GetNewClosure()

@{
  Name    = 'prop_progression'
  Area    = 'props'
  Bug     = 'A smash gate / billboard the save records as broken must stream in broken: the profile''s hit-prop bits have to reach PropZoneManager::maPreviouslyHitProps (actions 194 / 199).'
  Frames  = $false
  Setup   = $lSetup
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 110
  }
  DiagEnv = 'BRN_PROPPROG_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the boot profile reaches OnProfileLoaded @0x82397310 (ProcessGameEvents case 8), which posts action 194'
       Pattern = '^\[GameStateModule::OnProfileLoaded\] '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the prop world takes 194: SendingPropProgression -> E_RESET_UNLOADING_FOR_PROFILE'
       Pattern = '^\[propprog\] world: SendingPropProgression \(action 194\)'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the prop world asks back (game event 112) and case 112 raises mbPropSystemNeedsProgression'
       Pattern = '^\[propprog\] event 112 \(the prop world asks\)'; Min = 1 }
    @{ Kind = 'LogValue'; Name = 'the tail posts action 199 with the profile''s hit props (a progression save holds hundreds)'
       Pattern = '^\[propprog\] action 199 \(prop smash progression\) posted: the profile holds (?<n>\d+) hit props'; Group = 'n'; Agg = 'max'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the prop world installs exactly the bits the profile posted (PreSceneUpdate -> maPreviouslyHitProps)'; Script = {
        param($ctx)
        $posted = $null; $installed = $null
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[propprog\] action 199 .* holds (\d+) hit props') { $posted = [int]$Matches[1] }
          if ($l -match '^\[propprog\] world: the profile''s hit props installed \(action 199\): (\d+) props') { $installed = [int]$Matches[1] }
        }
        if ($null -eq $posted -or $null -eq $installed) { return @{ Pass = $false; Detail = "posted=$posted installed=$installed (a witness line is missing)" } }
        return @{ Pass = ($posted -gt 0 -and $posted -eq $installed); Detail = "posted $posted, installed $installed" }
      } }
    @{ Kind = 'LogCount'; Name = 'a prop the save records as hit streamed in AS HIT ([propprog] load ... hit=1)'
       Pattern = '^\[propprog\] load zone=\d+ prop=\d+ respawn=[12] hit=1 '; Min = 1 }
    @{ Kind = 'Script';   Name = 'LoadProp treats every hit prop the console''s way: a hit E_DONT_RESPAWN prop is not loaded, a hit E_RESPAWN_CHANGED prop loads (as its alternative)'; Script = {
        param($ctx)
        $n = 0; $bad = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[propprog\] load zone=(\d+) prop=(\d+) respawn=([12]) hit=1 loaded=([01])') {
            $n++
            $want = if ($Matches[3] -eq '1') { '0' } else { '1' }
            if ($Matches[4] -ne $want) { $bad += "zone $($Matches[1]) prop $($Matches[2]) respawn $($Matches[3]) loaded $($Matches[4])" }
          }
        }
        return @{ Pass = ($n -gt 0 -and $bad.Count -eq 0); Detail = ("{0} hit prop(s) streamed in; {1} handled against the console's arms{2}" -f $n, $bad.Count, $(if ($bad.Count) { ': ' + ($bad -join '; ') } else { '' })) }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
