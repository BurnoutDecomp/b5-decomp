import sys,json
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
sys.path.insert(0,'D:\\Reverse\\programbuffer-terra-build\\final2')
_r5_objects={'programbuffer': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\selected-stage1\\programbuffer-1503d257.obj'}
_r5_output_dir=Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\program-initialize')
def _r5_output(p):
    return _r5_output_dir/Path(p).name
def _r5_object(p):
    p=Path(p)
    stem=p.stem
    found=[v for k,v in _r5_objects.items() if stem==k or stem.startswith(k+'-')]
    assert len(found)==1,(str(p),found)
    return Path(found[0])
__file__='D:\\Reverse\\programbuffer-terra-build\\final2\\initialize-witness.py'
sys.argv=['D:\\Reverse\\programbuffer-terra-build\\final2\\initialize-witness.py']
exec(compile(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\program-initialize\\adapted_model.py').read_text(encoding="utf-8"),__file__,"exec"),globals())
