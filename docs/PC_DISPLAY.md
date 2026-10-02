# PC display and fullscreen

Press **F11** to enter borderless fullscreen on the monitor containing the game.
Press it again to restore the previous window position and size. Holding F11 does
not repeatedly toggle. Resizing or maximizing the window also updates rendering.
The last mode is saved immediately in `config.ini` (`[Display] Fullscreen=1` or
`0`) and restored on the next launch. A saved fullscreen launch fills the current
monitor; F11 still returns to a framed window. With no saved preference, the game
starts windowed.

The game draws at the physical pixel resolution of the window's 16:9 content
area. For example, a 1920x1080 monitor uses 1920x1080 rendering; a 2560x1600 monitor
uses a 2560x1440 image with 80-pixel bars above and below. Ultrawide, 4:3 and
portrait displays likewise keep the complete 16:9 image centered without stretch.
Changing the game's field of view or expanding the HUD to another aspect ratio is
a separate feature.

The UI keeps its 1280x720 logical layout, so it occupies the same proportion of
the picture at every resolution. Geometry and text draw into the larger target;
existing bitmap assets retain their original detail. Windows display scaling does
not reduce the game's rendering resolution.

Graphics patch equivalents and native MSAA sample counts are documented in
[PC graphics options](PC_GRAPHICS.md).

`config.ini` `[Display] Width` and `Height` bound the initial 16:9 window size. F11 uses
the current monitor size, including after a display-mode change. If the GPU cannot
allocate a requested size, the previous render resolution remains usable and is
scaled to fit; changing the window size retries allocation.

## Developer verification

Run from the parent workflow checkout:

```powershell
build exe
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_display_resize.py
powershell -NoProfile -ExecutionPolicy Bypass -File b5-decomp/tests/PCDisplayResizeLive.ps1
```

The first two tests use real D3D9 surfaces through 3840x2160, with repeated
resizing, preserved texture contents, MSAA coverage, odd dimensions and allocation
failure checks. The fullscreen test also checks saved-mode loading and startup,
F11 return to a visible framed window, and immediate writes to a temporary INI.
The live test uses the existing flow harness, a private save slot,
bounded frame captures and byte-for-byte restoration of `config.ini`. Its requested
window sizes can be limited by the desktop's maximum window height; `events.json`
records the actual client sizes and `result.json` records actual render sizes.
