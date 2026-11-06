"""Private assembly postmortem for Arena, without touching the production ledger."""
from pathlib import Path
from types import SimpleNamespace
import json,sqlite3,sys
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).parent
sys.path.insert(0,str(ROOT/'tools/work'))
import work,dossier,gen_skeleton
for module in [dossier,gen_skeleton]:module.X360_EXPORTS='D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX'
dossier.DECFIGS_DWARFDUMP='D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump'
dossier.FEB2007='D:/Reverse/BP-Decomp_Workflow/references/Feb-2007/BrnEntityModuleUnity'
ident=json.loads((ROOT/'progress/identity.json').read_text());index=json.loads((ROOT/'progress/tu_index.json').read_text())
names=['XGRAPHICS::Arena::'+n for n in ['Malloc','Free','Grow']]
tus={t for t,v in index.items() if any(n in v['functions'] for n in names)};assert len(tus)==1;t=next(iter(tus))
database=OUT/'native-xgraphics-arena-postmortem.sqlite';assert not database.exists()
con=sqlite3.connect(database);con.row_factory=sqlite3.Row;source=sqlite3.connect(OUT/'native-thread-join-postmortem.sqlite')
for sql, in source.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name!='sqlite_sequence'"):con.execute(sql)
con.execute('INSERT INTO tu(id,source,n_funcs,n_decfigs) VALUES(?,?,?,?)',(t,index[t]['source'],3,0))
for name in names:con.execute('INSERT INTO func(name,tu_id,x360_addr) VALUES(?,?,?)',(name,t,ident[name]['x360_addrs'][0]))
con.commit();work.connect=lambda:con;p=OUT/'POSTMORTEM_NATIVE_XGRAPHICS_ARENA.md'
work.cmd_postmortem(SimpleNamespace(tu=t,out=str(p)))
(OUT/'native-xgraphics-arena-postmortems.json').write_text(json.dumps([{'tu':t,'functions':names,'path':str(p)}],indent=2)+'\n')
print('One private packet;three original functions; no production-ledger mutation')
