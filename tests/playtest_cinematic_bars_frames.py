"""Measure the original 0.15 letterbox band on captured gameplay frames."""
from pathlib import Path
import json
import sys
import numpy as np
from PIL import Image

frames = sorted(Path(sys.argv[1]).glob('bb_*.bmp'))
bars = []
uncovered_after = 0
for path in frames:
    pixels = np.asarray(Image.open(path).convert('RGB'))
    height, width = pixels.shape[:2]
    # The Super Jump wings/title intentionally overlap the lower band's left
    # half, and native diagnostics sit around y=height-60. Measure the right
    # half above them; retain the full-width upper-band check and visible city.
    inset = 10
    bar_edge = int(height * 0.15)
    top = pixels[10:bar_edge-2, inset:width-inset]
    bottom = pixels[height-bar_edge+max(1, height//100):height-int(height*0.11),
                    width//2:width-inset]
    middle = pixels[bar_edge+15:height-bar_edge-15, width//4:3*width//4]
    black_top = float((top.max(axis=2) <= 3).mean())
    black_bottom = float((bottom.max(axis=2) <= 3).mean())
    visible_middle = float(middle.mean()) > 15
    if black_top >= 0.995 and black_bottom >= 0.995 and visible_middle:
        bars.append(path.name)
    elif bars and black_top < 0.5 and visible_middle:
        uncovered_after += 1
print(json.dumps({'captured_frames': len(frames), 'bar_frames': len(bars),
                  'uncovered_after': uncovered_after,
                  'first_bar': bars[0] if bars else None,
                  'last_bar': bars[-1] if bars else None}))
