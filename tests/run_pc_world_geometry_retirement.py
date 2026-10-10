"""Exercise byte-range retirement, neighbour preservation and reused addresses."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
# This fixture registers the legacy page-key maps directly. The compact-token
# path is exercised by run_pc_geometry_lifetime through RegisterMirror.
os.environ['BRN_GEOMETRY_COMPACT_LIFETIME']='0'
p=argparse.ArgumentParser();p.add_argument('--source-page-kb',choices=['4','16','64'],default='4')
p.add_argument('--drop-last-source-page',action='store_true');a=p.parse_args()
os.environ['BRN_GEOMETRY_SOURCE_PAGE_KB']=a.source_page_kb
shadow={}
if a.drop_last_source_page:
    path='src/pc/gcm/renderengine/WorldGeometry.cpp';text=Tree().read(path)
    before='if (luPage != luHeaderPage)';assert text.count(before)==1
    shadow[path]=text.replace(before,'if (luPage != luHeaderPage && luPage != luLast)')
here=Path(__file__).resolve().parent
numeric=compile_and_run(here/'PCWorldGeometryRetirement.cpp','unused.inc','',
    'PCWorldGeometryRetirement',extra_flags='d3d9.lib user32.lib',shadow=shadow)
raise SystemExit(report('run_pc_world_geometry_retirement',[],numeric,23))
