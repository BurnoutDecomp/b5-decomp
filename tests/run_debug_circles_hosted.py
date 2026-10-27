"""Hosted-only candidate/control execution using the existing owned Windows Job helper."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys
from online_save_route_owned import run_owned

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rev',required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args()
    assert os.name=='nt' and os.environ.get('GITHUB_ACTIONS')=='true'
    assert len(args.rev)==40 and all(c in '0123456789abcdef' for c in args.rev)
    here=Path(__file__).resolve().parent
    root=here.parent.parent
    helper=here/'online_save_route_owned.py'
    assert hashlib.sha256(helper.read_bytes().replace(b'\r\n',b'\n')).hexdigest()=='c21a018a37c094eec2c6befcd19b8862f8c33c84b66f84c33c55eac600649179'
    output=args.output_dir.resolve()
    output.mkdir(parents=True,exist_ok=False)
    control='11cbd50c824724c49d425fc91793ba2453bba320'
    normal_control='b7f8069bd986c59cefad9567fed014d1e3a7f1fe'
    git_path=Path(shutil.which('git.exe') or '')
    assert git_path.is_absolute() and git_path.is_file() and git_path.name.lower()=='git.exe'
    fetch=run_owned([str(git_path),'-C',str(here.parent),'fetch','--recurse-submodules=no','--no-tags','--depth=1','origin',control,normal_control],root,output/'control-fetch.log',90)
    assert fetch['exit_code']==0 and not fetch['timed_out'] and fetch['cleanup']['complete']
    records={'control_fetch':fetch}
    for variant,revision,extra in [('candidate',args.rev,[]),('control',control,['--expect-circle-regression']),('normal_control',normal_control,['--expect-circle-regression'])]:
        command=[sys.executable,'-B',str(here/'run_pc_prop_reflection_lod.py'),'--rev',revision,'--output-dir',str(output/variant),*extra]
        record=run_owned(command,root,output/(variant+'-owned.log'),300)
        records[variant]=record
        (output/'owned-jobs.json').write_text(json.dumps(records,indent=2)+'\n')
        assert record['exit_code']==0 and not record['timed_out']
        assert record['cleanup']['complete'] and record['cleanup']['job_active_processes_after_cleanup']==0
        numeric=json.loads((output/variant/'result.json').read_text('utf-8'))
        assert numeric['numeric']['checks']>0
        if variant=='candidate':
            assert numeric['process_exit_code']==0 and numeric['numeric']['failures']==0
        else:
            assert numeric['process_exit_code']==1 and numeric['numeric']['failures']>0
            assert 'EXPECTED_BASELINE_RED:' in (output/'control-owned.log').read_text('utf-8',errors='replace')
    files=[dict(path=str(p.relative_to(output)),bytes=p.stat().st_size,sha256=digest(p)) for p in output.rglob('*') if p.is_file()]
    receipt=dict(status='PASS',source=args.rev,control=control,normal_control=normal_control,owned_jobs=records,files=files,
                 scope='Actual extracted production render bodies with finite fixture geometry; no native/pixel or general VMX precision certificate')
    (output/'fixture.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(dict(status='PASS',source=args.rev,control=control)))

if __name__=='__main__':
    main()
