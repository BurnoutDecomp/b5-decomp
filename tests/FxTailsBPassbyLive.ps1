# FX-TAILS-B item 6 (crash parity 2026-09-24) -- a rival car passing the player is VOICED, live.
#
# The chain, console addresses:
#   AIPhysicsControl::UpdateAIPassbys @0x826B4A98 -> PlayPassBy @0x8269A4C0 posts a
#     PassbyStateManager::Passby (TrafficLarge 5, or TrafficSmall 3 for a near miss) with the car's
#     3D control and volume 2.5
#   PassbyStateManager::Prepare @0x826F9748 (PassbyAsset.bundle, the bank, PrepareStates(1, 8, 0))
#   PassbyStateManager::UpdateParams @0x826D4D00 -> a free PassbyState::Attach @0x826D4A98
#   PassbyEffect::Attach @0x826D5280 (bin, sample, Play, silent start) -> UpdateParams @0x826D5068
#     (the relative-speed pitch / volume slopes, Slope::GetValue @0x826897F0) -> ProcessUpdate
#     @0x826F99C0 (SetGain / Pitch / Azimuth while playing; the state detaches when it ends)
# Before item 6 the manager's Prepare returned true with no states and its UpdateParams was empty:
# every post was dropped, nothing was voiced.
#
# The witnesses (NOT IN THE X360 BINARY): BRN_AI_SOUND_DIAG's "[ai-sound-passby] ... accepted=1"
# (the post) and BRN_PASSBY_SOUND_DIAG's "[passby-sound]" prepare / dispatch / effect attached /
# playing / finished lines.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTailsBPassbyLive.ps1 --run-name fxtailsb_passby
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtailsb_passby'
$case.Area = 'sound'
$case.Bug = 'A pass-by a rival car posts must reach a PassbyState and be played with a non-zero gain by its PassbyEffect.'
$case.Run.MaxSeconds = 100
$case.DiagEnv = 'BRN_AI_SOUND_DIAG=1,BRN_PASSBY_SOUND_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'PassbyStateManager::Prepare finished (bank loaded, 8 states prepared)'
       Pattern = '\[passby-sound\] Prepare: FINISHED'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'a rival posted a pass-by (AIPhysicsControl::PlayPassBy accepted)'
       Pattern = '\[ai-sound-passby\] car=\d+ (passby|near-miss) relSpeed=\S+ accepted=1'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'UpdateParams handed a post to a PassbyState'
       Pattern = '\[passby-sound\] dispatch posted=\d+ attached=[1-9]'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'a PassbyEffect attached a rival pass-by (TrafficSmall/TrafficLarge, with its 3D control)'
       Pattern = '\[passby-sound\] effect attached type=(3|5) sample=\d+ .* control=1'; Expect = $true }
    @{ Kind = 'Script'; Name = 'a rival pass-by was PLAYED with a non-zero gain'; Script = {
        param($ctx)
        $inv = [System.Globalization.CultureInfo]::InvariantCulture
        $playing = 0; $audible = 0; $max = 0.0; $first = ''
        foreach ($l in $ctx.LogLines) {
            if ($l -notmatch '\[passby-sound\] playing type=(3|5) sample=\d+ gain=(\S+) pitch=(\S+)') { continue }
            $playing++
            $g = 0.0
            if ([double]::TryParse($Matches[2], [System.Globalization.NumberStyles]::Float, $inv, [ref]$g) -and $g -gt 0.0) {
                $audible++
                if ($g -gt $max) { $max = $g }
                if (-not $first) { $first = $l.Trim() }
            }
        }
        @{ Pass = ($audible -gt 0); Detail = ('{0} playing frames, {1} with gain > 0 (max {2}); first: {3}' -f $playing, $audible, $max, $first) }
    } }
    @{ Kind = 'LogMatch'; Name = 'the pass-by finished and its state let go (ProcessUpdate -> Detach)'
       Pattern = '\[passby-sound\] finished type=\d+ sample=\d+ lifetime=[12] -> state Detach'; Expect = $true }
)
$case
