"""Freeze/capture native repair stages and combine immutable reports with input proofs."""
import argparse
from collections import defaultdict, deque
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('wave4_evidence',HERE/'wave4_evidence.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def affected(previous, current):
    changed = {p for p in set(previous['hashes']) | set(current['hashes'])
               if previous['hashes'].get(p) != current['hashes'].get(p)}
    reverse = defaultdict(set)
    for graph in (previous['dependencies'],current['dependencies']):
        for parent, children in graph.items():
            for child in children:
                reverse[child].add(parent)
    result = set(changed)
    queue = deque(changed)
    while queue:
        for parent in reverse[queue.popleft()]:
            if parent not in result:
                result.add(parent)
                queue.append(parent)
    return changed,result


def verify_current(meta):
    assert e.git('rev-parse','HEAD').decode().strip() == meta['head']
    assert not e.git('status','--porcelain=v1').strip()
    assert hashlib.sha256(e.git('diff','--binary',e.BASE)).hexdigest() == meta['sha256']
    for p,h in meta['source_snapshot']['hashes'].items():
        assert e.sha(e.WORK/p) == h,p


def freeze(args):
    assert not e.git('status','--porcelain=v1').strip()
    previous = read(HERE/args.baseline_audit)
    snapshot = e.source_snapshot()
    changed,closure = affected(previous['source_snapshot'],snapshot)
    sources = {r['source'] for r in read(e.RAW)['results']}
    patch = e.git('diff','--binary',e.BASE)
    paths = e.git('diff','--name-only',e.BASE).decode().splitlines()
    meta = {'base':e.BASE,'head':e.git('rev-parse','HEAD').decode().strip(),'workspace':str(e.WORK),
            'sha256':hashlib.sha256(patch).hexdigest(),'paths':paths,
            'source_sha256':{p:e.sha(e.WORK/p) for p in paths if (e.WORK/p).is_file()},
            'source_snapshot':snapshot,'repair_paths':sorted(changed),'affected_sources':sorted(closure & sources),
            'previous_stage_head':previous['head'],'previous_stage_audit':str(HERE/args.baseline_audit),
            'stage1_head':e.FROZEN,'stage1_raw_report_sha256':e.sha(e.RAW),
            'created_utc':datetime.now(timezone.utc).isoformat()}
    e.write_new(e.SCRATCH/(args.name+'-patch.json'),meta)
    with (e.SCRATCH/(args.name+'.diff')).open('xb') as stream:
        stream.write(patch)
    print(json.dumps({k:v for k,v in meta.items() if k not in ('source_snapshot','source_sha256','paths')},indent=2))
    print('Full changed paths:',len(paths))


def capture(args):
    path = e.SCRATCH/(args.name+'-patch.json')
    meta = read(path)
    verify_current(meta)
    report_path = e.ARTIFACTS/args.report
    report,index,artifacts = e.report_check(report_path)
    assert set(index) == set(meta['affected_sources'])
    audit = {'head':meta['head'],'patch_sha256':meta['sha256'],'metadata':str(path),
             'report':str(report_path),'report_sha256':e.sha(report_path),'requested':report['requested'],
             'passed':report['passed'],'failed':report['failed'],'artifact_manifest':artifacts,
             'source_snapshot':meta['source_snapshot'],'created_utc':datetime.now(timezone.utc).isoformat()}
    e.write_new(HERE/args.audit,audit)
    print(json.dumps({k:v for k,v in audit.items() if k not in ('artifact_manifest','source_snapshot')},indent=2))


def overlay(args):
    meta = read(e.SCRATCH/(args.name+'-patch.json'))
    verify_current(meta)
    final_snapshot = meta['source_snapshot']
    preceding = args.prior_audit or ['repair1-terminal-audit.json']
    paths = [HERE/'stage1-terminal-audit.json'] + [HERE/p for p in preceding] + [HERE/args.audit]
    index,latest_artifacts,source_stage = {},{},{}
    provenance = []
    audits = [read(p) for p in paths]
    for i,(path,audit) in enumerate(zip(paths,audits)):
        report_path = Path(audit['report'])
        assert e.sha(report_path) == audit['report_sha256']
        report = read(report_path)
        assert report['state'] == 'complete'
        assert report['requested'] == len(report['results']) == audit['requested']
        for row in report['results']:
            source = row['source']
            index[source] = dict(row,build_head=audit['head'])
            latest_artifacts[source] = audit['artifact_manifest'][source]
            source_stage[source] = i
        provenance.append({'head':audit['head'],'report':str(report_path),'report_sha256':audit['report_sha256'],
                           'requested':report['requested'],'passed':report['passed'],'failed':report['failed'],
                           'terminal_audit':str(path),'terminal_audit_sha256':e.sha(path)})
    # A reused artifact must have identical source/header inputs in the final
    # tree. Closure includes every textual/conditional/ambiguous include edge.
    for i,audit in enumerate(audits):
        changed,closure = affected(audit['source_snapshot'],final_snapshot)
        stale = sorted(s for s in index if source_stage[s] == i and s in closure)
        assert not stale,(audit['head'],stale)
    raw = read(e.RAW)
    assert len(index) == raw['requested'] == 3609
    for source,row in index.items():
        expected = latest_artifacts[source]
        assert e.sha(row['log']) == expected['log_sha256'],source
        if row['passed']:
            assert e.sha(row['object']) == expected['object_sha256'],source
            assert e.sha(row['assembly']) == expected['assembly_sha256'],source
    rows = [index[r['source']] for r in raw['results']]
    passed = sum(r['passed'] for r in rows)
    result = dict(raw,results=rows,passed=passed,failed=len(rows)-passed)
    result['provenance'] = {'kind':'full sweep plus %d actual repair overlays' % (len(paths)-1),'source_head':meta['head'],
                            'patch_sha256':meta['sha256'],'layers':provenance,
                            'input_invariance_verified':True,'not_a_new_full_sweep':True,
                            'closure_method':final_snapshot['method']}
    output = e.ARTIFACTS/('build/x360/'+args.name+'-effective.json')
    e.write_new(output,result)
    e.write_new(HERE/(args.name+'-artifact-manifest.json'),
                {'head':meta['head'],'patch_sha256':meta['sha256'],'effective_report':str(output),
                 'effective_report_sha256':e.sha(output),'artifact_manifest':latest_artifacts,
                 'layers':provenance,'input_invariance_verified':True})
    old = {r['source']:r for r in read(e.PRIOR)['results']}
    regressions = sorted(s for s in old if old[s]['passed'] and s in index and not index[s]['passed'])
    gains = sorted(s for s in index if index[s]['passed'] and(s not in old or not old[s]['passed']))
    print(json.dumps({'head':meta['head'],'patch_sha256':meta['sha256'],'effective_report':str(output),
                      'effective_report_sha256':e.sha(output),'requested':len(rows),'passed':passed,
                      'failed':len(rows)-passed,'prior_passes_retained':sum(index[s]['passed'] for s in old if old[s]['passed']),
                      'new_passes':len(gains),'regressions':regressions,
                      'timeouts':[r['source'] for r in rows if r['returncode']==-1]},indent=2))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage',choices=('freeze','capture','overlay'))
parser.add_argument('--name',required=True)
parser.add_argument('--baseline-audit',default='repair1-terminal-audit.json')
parser.add_argument('--audit',default='repair2-terminal-audit.json')
parser.add_argument('--prior-audit',action='append',default=[])
parser.add_argument('--report')
args = parser.parse_args()
{'freeze':freeze,'capture':capture,'overlay':overlay}[args.stage](args)
