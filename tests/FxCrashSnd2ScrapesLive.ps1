# FX-CRASHSND2 item 2 (crash parity 2026-09-24) -- the scrape legs of the collision sound, live.
#
# ARTIST CollisionStateManager::UpdateScrapes @0x826D3F50 runs after the contact imports: a contact
# that continues a scrape (same entity pair and face, ScrapeInfo::operator== @0x826821F0) found in
# the 16-entry history eats its repeated impact, and hands the scrape to a collision state with the
# SCRAPE lifetime, whose ScrapeEffect (@0x826F8578) starts the AEMS_ScrapeGranulator voice 0.15 s
# (KF_TIME_DELAY_BEFORE_SCRAPES) after the state attached. Before the fix the history was never
# written: no scrape state, no scrape voice, and every frame of a grind was a fresh impact.
#
# The drive is fxcrashsnd2_builders' deterministic three-shot sweep (quay rocks at 44 m/s, the
# harbour slope, 53 m/s): the car grinds along the rocks after each impact.
#
# The witnesses (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY; owner:index, owner 0 = world,
# 1 = race car):
#   [collision-audio] scrape history A=<o:i> B=<o:i> orient=<o> stamp=<t>     a new scrape recorded
#   [collision-audio] scrape eaten A=.. B=.. orient=..                        its repeated impact eaten
#   [collision-audio] scrape start|refresh A=.. B=.. orient=.. intensity=..   a state holds it (SCRAPE)
#   [collision-audio] scrape voice start A=.. B=.. orient=.. material_b=.. attached=.. now=.. live=<0|1>
@{
  Name    = 'fxcrashsnd2_scrapes'
  Area    = 'sound'
  Bug     = 'A contact that keeps touching the same thing on the same face must become a scrape: UpdateScrapes records it, eats its repeated impacts and gives it a SCRAPE-lifetime collision state whose ScrapeEffect plays the AEMS scrape granulator.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 110
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '1790.5,-3.2,-2393.0'
    CrashSweepShots  = '1790.5/-3.2/-2393.0/260:44,1758.5/-0.8/-2399.6/262:12,1790.5/-3.2/-2393.0/260:53'
    CrashSweepSettle = 300
  }
  DiagEnv = 'BRN_COLLISION_AUDIO_DIAG=1,BRN_HUD_SOUND_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the player car''s scrape against the WORLD entered the scrape history (UpdateScrapes, new-scrape leg)'
       Pattern = '^\[collision-audio\] scrape history A=1:0 B=0:0 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a continuing scrape''s repeated impact was eaten (meFatality OFF)'
       Pattern = '^\[collision-audio\] scrape eaten '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a collision state took a scrape with the SCRAPE lifetime'
       Pattern = '^\[collision-audio\] scrape (start|refresh) A='; Min = 1 }
    @{ Kind = 'Script';     Name = 'a WALL scrape voice started live (player car vs world, AEMS_ScrapeGranulator created)'; Script = {
        param($ctx)
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] scrape voice start A=1:0 B=0:0 .* live=1') {
            return @{ Pass = $true; Detail = $l.Trim() }
          }
        }
        $any = @($ctx.LogLines | Where-Object { $_ -match '^\[collision-audio\] scrape voice start' })
        $detail = if ($any.Count) { 'voice starts, none player-vs-world live: ' + $any[0].Trim() } else { 'no [collision-audio] scrape voice start line' }
        return @{ Pass = $false; Detail = $detail }
      } }
    @{ Kind = 'Script';     Name = 'every scrape voice starts at least KF_TIME_DELAY_BEFORE_SCRAPES (0.15 s) after its state attached'; Script = {
        param($ctx)
        $n = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] scrape voice start .* attached=(?<a>-?[\d.]+) now=(?<n>-?[\d.]+)') {
            $n++
            if (([double]$Matches.n - [double]$Matches.a) -lt 0.149) { return @{ Pass = $false; Detail = $l.Trim() } }
          }
        }
        return @{ Pass = ($n -gt 0); Detail = "$n voice starts, all past the delay" }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
