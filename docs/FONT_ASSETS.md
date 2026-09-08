# Incorrect UI fonts on an existing installation

Rebuilding the executable does not refresh converted game assets. Old font bundles
can load successfully from `Language/Fonts` while containing obsolete x64 field
offsets. This is separate from selecting the SD font directory.

One affected installation loaded `WesternB5Body_35.font` with an empty typeface
family and a height of 5,469,033 instead of 36. An empty family matches every font
request, so body text can replace headings and dot-matrix text.

From the **BP-Decomp_Workflow** checkout, close the game and run:

```powershell
.\build.cmd file 'LANGUAGE/FONTS/*.FONT' --all --install
```

This uses the existing X360 font/texture converter and installs all six refreshed
HD bundles in `build/game`. No change to Windows fonts or the game's lookup code
is required. Other installed game folders need these refreshed assets too.

After restarting, check `build/game/BrnGame.log`. The three UI font registrations
should have these family/style pairs:

| Family | Style |
| --- | --- |
| `b5eacondiss` | `B5EAConDisS` |
| `b5dotmat` | `Regular` |
| `machinestd-bold` | `Bold` |

Verified on 2026-09-08 by rebuilding the six bundles, inspecting the body font's
serialized metadata, and running through junkyard selection into driving.
