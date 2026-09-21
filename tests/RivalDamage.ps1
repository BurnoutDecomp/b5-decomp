$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'rival_damage'
$case.Bug = 'A player-credited rival takedown must show a moving, physically crashing, deformed victim.'
$case.Frames = $true
$case.Run.FrameEvery = 12
$case.Run.MinFreeGB = 2
# BRN_CRASH_RESPONSE_DIAG arms the [absorb] witness (DeformableObject::UpdateContacts), which
# prints the absorption SET every crashing car runs on. It doubles the log (mostly
# [crash-response] lines) but it is the only witness of the set.
$case.DiagEnv += ',BRN_RIVAL_DAMAGE_DIAG=1,BRN_CAMERA_RIG_DIAG=1,BRN_CRASH_RESPONSE_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'credited rival physically crashes with damage rendering enabled'; Script = {
        param($ctx)
        $victims = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { [int]$Matches[1] }
        } | Select-Object -Unique)
        $samples = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] slot=(\d+) crashing=1 damaged=1 displacement=([^ ]+) offsets=([^ ]+) pos=([^ ]+) angular=([^ ]+)') {
                $slot = [int]$Matches[1]
                $displacement = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $offsets = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                if ($victims -contains $slot -and $displacement -gt 0.0001 -and $offsets -gt 0.001) {
                    "$slot $($Matches[4]) $($Matches[5])"
                }
            }
        } | Select-Object -Unique)
        @{ Pass = $samples.Count -gt 1; Detail = "$($samples.Count) distinct moving/deformed samples for player-credited victims" }
    } }
    @{ Kind = 'Script'; Name = 'credited victim crashes on the AI_CRASHING or SHUTDOWN absorption set, never NORMAL'; Script = {
        param($ctx)
        # DeformableObject::UpdateAbsorptionSet @0x825DF9A0: an AI-driven race car that is crashing
        # runs on set 1 (AI_CRASHING, in a mode) or set 3 (SHUTDOWN, free burn) -- full absorption
        # at 30 mph closing instead of NORMAL's 80. Before 2026-09-21 the crash read was pinned
        # false and every victim absorbed on set 0, which is "deforms less, flies more".
        # The [absorb] line is only printed while the car is crashing, so every sample of a
        # credited victim must be 1 or 3. ent = 0x1000000 | (slot << 10) for a race car.
        $victims = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { [int]$Matches[1] }
        } | Select-Object -Unique)
        $seen = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[absorb\] owner 1 ent (\d+) set (\d+)') {
                $slot = ([int]$Matches[1] -band 0xFFFFFF) -shr 10
                if ($victims -contains $slot) { $seen["$slot/set$($Matches[2])"] += 1 }
            }
        }
        $crash  = @($seen.Keys | Where-Object { $_ -match '/set[13]$' })
        $normal = @($seen.Keys | Where-Object { $_ -match '/set0$' })
        $detail = ($seen.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)x$($_.Value)" }) -join ' '
        @{ Pass = ($crash.Count -gt 0 -and $normal.Count -eq 0); Detail = "victim absorb samples: $detail (need set1/set3 only)" }
    } }
)
$case
