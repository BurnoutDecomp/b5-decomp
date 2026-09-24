# FX-TAILS-B item 3 (crash parity 2026-09-24) -- a stopped stream fades out on the console's curve, live.
#
# ARTIST StreamingEffect::Detach @0x826EEA68 writes, on every fading frame, the send gain
#   GetOutput(1 - f, E_ONE_MINUS_EQPWR) * mfGainPreFade   (inlined at 0x826EEB38..0x826EEBC8)
# = (1 - gafArraySinTable[trunc(511 f)]) * pre, f = clamp(t / fade) -- the equal-power 1 - sin(f * pi/2).
# The PC faded linearly, (1 - f) * pre. The table (0x82F2D920) is entry i = the 5-decimal literal of
# sin(i * 3.14159 / 1024), entries 511 and 512 both 1.0 (FX-VOICEPOOL dfac07b2).
#
# The witness (BRN_STREAM_DIAG, NOT IN THE X360 BINARY, capped at 96 lines):
#   [sndstream] fade spec=<content spec> t=<time through fade> fade=<fade length> f=<clamped position>
#               pre=<pre-fade gain> gain=<gain written to the send>
# The check re-derives every line's gain from its f and pre with the table formula above (accepting
# the neighbouring entry where the 6-decimal print of f sits on an entry boundary), and requires at
# least one mid-fade line whose gain is NOT the linear (1 - f) * pre.
@{
  Name    = 'fxtailsb_stream_fade'
  Area    = 'sound'
  Bug     = 'A stopped stream must fade out on the equal-power curve StreamingEffect::Detach inlines (GetOutput(1 - f, E_ONE_MINUS_EQPWR) * the pre-fade gain), not linearly.'
  Frames  = $false
  Run     = @{
    Drive       = $true
    MotionProbe = $true
    MaxSeconds  = 90
    SkipIntro   = $true
    AcceptGap   = 1.0
  }
  DiagEnv = 'BRN_STREAM_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'a stream was stopped with a fade (the stream ring posted STOP)'
       Pattern = '^\[sndstream\] post STOP .* fade=0*\.?[0-9]*[1-9]'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'StreamingEffect::Detach faded it (fading frames witnessed)'
       Pattern = '^\[sndstream\] fade spec=0x[0-9A-F]{8} '; Min = 2 }
    @{ Kind = 'Script';     Name = 'every fading frame writes the console gain (1 - table[trunc(511 f)]) * pre, and the fade is not linear'; Script = {
        param($ctx)
        $lines = 0; $bad = 0; $midNonLinear = 0; $firstBad = ''
        $inv = [System.Globalization.CultureInfo]::InvariantCulture
        foreach ($l in $ctx.LogLines) {
          if ($l -notmatch '^\[sndstream\] fade spec=0x[0-9A-F]{8} t=(\S+) fade=(\S+) f=(\S+) pre=(\S+) gain=(\S+)') { continue }
          $lines++
          $f = [double]::Parse($Matches[3], $inv); $pre = [double]::Parse($Matches[4], $inv); $gain = [double]::Parse($Matches[5], $inv)
          $ok = $false
          $e0 = [math]::Floor(511.0 * $f)
          foreach ($e in @(($e0 - 1), $e0, ($e0 + 1))) {
            if ($e -lt 0 -or $e -gt 512) { continue }
            $t = if ($e -ge 511) { 1.0 } else { [math]::Round([math]::Sin($e * 3.14159 / 1024.0), 5) }
            if ([math]::Abs($gain - (1.0 - $t) * $pre) -le (2e-6 + 1e-5 * [math]::Abs($pre))) { $ok = $true }
          }
          if (-not $ok) { $bad++; if (-not $firstBad) { $firstBad = $l } }
          if ($f -gt 0.1 -and $f -lt 0.9 -and [math]::Abs($gain - (1.0 - $f) * $pre) -gt 0.01 * [math]::Abs($pre) -and [math]::Abs($pre) -gt 1e-4) { $midNonLinear++ }
        }
        return @{ Pass = ($lines -gt 0 -and $bad -eq 0 -and $midNonLinear -gt 0)
                  Detail = ('{0} fading frames, {1} off the console curve, {2} mid-fade frames off the linear ramp{3}' -f $lines, $bad, $midNonLinear, $(if ($firstBad) { '; first off: ' + $firstBad } else { '' })) }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
