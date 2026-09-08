# Junkyard paint selection

The finish selector requests the original car model for each unlocked livery.
For the starting Hunter Cavalry, the three normal finishes load `PUSMC1A1`,
`PUSMC1A2`, and `PUSMC1A3`. Special finishes retain the progression gates.
Paint type selects the model's palette; colour selects an entry in that palette.
Both values are stored in the profile and applied to the active car.

The restored path includes GUI requests/replies, vehicle unload and attribute
cleanup, streaming completion, and the GUI notification that unlocks navigation
after a model change. The model-change camera event carries the full spawn pose.

From the workflow checkout, run:

```powershell
python b5-decomp/tests/run_collection_hash_removal.py
pwsh -NoProfile -File b5-decomp/tests/run_junkyard_customization.ps1
```

The first test requires MSVC and exercises collision chains using production map
methods. The second requires installed game assets and a built executable. It uses
the existing `tools/diagnostics/flow_run.ps1` harness and its window lock, captures
frames/logs, cycles all three starting-car finishes, changes type and colour, and
exits the junkyard. It fails on game assertions or missing completion/paint events.
The optional `-OutDir` selects the capture directory.

The harness's `OptionPrev` and `OptionNext` input events supply GUI left/right;
`Prev` and `Next` supply up/down. All use the `Local\BurnoutPC_Input_` prefix.
