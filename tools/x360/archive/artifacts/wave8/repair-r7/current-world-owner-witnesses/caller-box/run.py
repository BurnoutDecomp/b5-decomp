import sys,json
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
sys.path.insert(0,'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\caller-repair-terra')
_r5_objects={'CgsAABBoxBuilder': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsAABBoxBuilder-b9511122.obj', 'CgsPrimitivePairListBuilder': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsPrimitivePairListBuilder-cd9376ef.obj'}
_r5_output_dir=Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-world-owner-witnesses\\caller-box')
def _r5_output(p):
    return _r5_output_dir/Path(p).name
def _r5_object(p):
    p=Path(p)
    stem=p.stem
    found=[v for k,v in _r5_objects.items() if stem==k or stem.startswith(k+'-')]
    assert len(found)==1,(str(p),found)
    return Path(found[0])
__file__='D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\caller-repair-terra\\validate_box.py'
sys.argv=['D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\caller-repair-terra\\validate_box.py']
_r7_previous_object = _r5_object
_r7_aliases={'CgsFine_wSQ1': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsFineIntersectionTestModule_wSQ1-253da880.obj', 'CgsFine-hit-v2': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsFineIntersectionTestModule_wSQ1-253da880.obj'}
def _r5_object(p):
 p=Path(p)
 return Path(_r7_aliases[p.stem]) if p.stem in _r7_aliases else _r7_previous_object(p)
exec(compile(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-world-owner-witnesses\\caller-box\\adapted_model.py').read_text(encoding="utf-8"),__file__,"exec"),globals())
