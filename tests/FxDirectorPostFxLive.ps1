# FX-DIRECTOR (crash parity 2026-09-24) -- an ICE shot's authored post-FX hook reaches the GUI as an EDGE, live.
#
# KeyAnimController publishes the playing take's E_ICE_POSTFX_HOOK id in the camera's
# mEffects.muRequestedPostFxId every frame, and BridgeDirectorToGui posts GUI event 495 (StartHook by
# GUID) for any non-zero id; EffectsArbitrator::StartHook re-initialises the blender each time.
# MainDirector::Update's post-FX bookkeeping (0x82275084, 0x82275094..0x822750E4) publishes the id only
# on the frame it changes (or on a camera's first frame) and registers it on the director's
# EffectInterface. The shot: FxDirectorStuntJumpLive.ps1's signature jump (4030, cut 305 ->
# World_Signature_305), whose moment compares the published id with the 2dFlash hook 575791 (the jump
# photo flash, MomentPlayerStunt @0x82272750).
# Witness: BRN_PFX_DIAG's `[pfx] StartHook '<name>' ...` lines (EffectsArbitrator::StartHook) -- the
# case REPORTS how many start requests each hook got; the before/after pair is the evidence (one per
# change after the fix, one per frame of the shot before it). No gate on the count itself.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirectorPostFxLive.ps1
@{
  Name    = 'fxdirector_postfx'
  Area    = 'director'
  Bug     = 'An ICE take''s authored post-FX hook must reach the GUI once per change (MainDirector::Update post-FX bookkeeping), not on every frame of the shot.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 60
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '3026.55,-8.9,-294.9'
    CrashSweepShots  = '3026.55/-8.9/-294.9/1.45:55.8'
    CrashSweepSettle = 600
  }
  DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_PFX_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was seated on the approach)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the jump posted its CUSTOM camera request (OnJumpStart, type 1)'
       Pattern = '\[jump-ladder\] action=56 OnJumpStart posted cut=\d+ type=1'; Min = 1 }
    @{ Kind = 'Script';     Name = 'post-FX start requests per hook (evidence: StartHook count by name)'; Script = {
        param($ctx)
        $counts = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match "\[pfx\] StartHook '([^']*)'") { $counts[$Matches[1]] = 1 + [int]$counts[$Matches[1]] }
        }
        $summary = ($counts.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name) x$($_.Value)" }) -join ', '
        @{ Pass = $true; Detail = ("StartHook: {0}" -f $(if ($summary) { $summary } else { 'none' })) }
    } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
