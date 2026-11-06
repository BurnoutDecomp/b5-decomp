"""Inspect native SDK CodeView type records without modifying the archive."""
import json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/xgraphics-inspection'
path=Path(sys.argv[1]) if len(sys.argv)>1 else OUT/'full-types-0x1e1a576.obj'
b=path.read_bytes();ns=struct.unpack_from('<H',b,2)[0];op=struct.unpack_from('<H',b,16)[0]
s=next(struct.unpack_from('<8sIIIIIIHHI',b,20+op+i*40) for i in range(ns) if b[20+op+i*40:28+op+i*40]==b'.debug$T')
d=b[s[4]:s[4]+s[3]];assert d[:4]==b'\x04\0\0\0'
at=4;ti=0x1000;records={}
while at<len(d):
    n,leaf=struct.unpack_from('<HH',d,at);z=d[at+4:at+2+n]
    if leaf==0x1509:
        start,count=struct.unpack_from('<II',z);ti=start+count
    else:records[ti]=(leaf,z);ti+=1
    at+=2+n
assert at==len(d)
def number(z,at):
    value=struct.unpack_from('<H',z,at)[0]
    if value<0x8000:return value,at+2
    formats={0x8000:'b',0x8001:'h',0x8002:'H',0x8003:'i',0x8004:'I',0x8009:'q',0x800a:'Q'}
    fmt=formats[value];return struct.unpack_from('<'+fmt,z,at+2)[0],at+2+struct.calcsize(fmt)
def string(z,at):
    end=z.index(b'\0',at);return z[at:end].decode('ascii'),end+1
def typename(ti):
    if ti not in records:return hex(ti)
    leaf,z=records[ti]
    if leaf in [0x1504,0x1505]:_,at=number(z,16);return string(z,at)[0]
    if leaf==0x1507:return string(z,12)[0]
    if leaf==0x1002:return typename(struct.unpack_from('<I',z)[0])+'*'
    if leaf==0x1001:return 'qualified '+typename(struct.unpack_from('<I',z)[0])
    if leaf==0x1503:
        elem,index=struct.unpack_from('<II',z);size,at=number(z,8);return f'{typename(elem)}[bytes={size}]'
    return f'leaf{leaf:x}:{ti:x}'
def fields(ti):
    leaf,z=records[ti];assert leaf==0x1203;at=0;result=[]
    while at<len(z):
        if z[at]>=0xf0:at+=z[at]&15;continue
        kind=struct.unpack_from('<H',z,at)[0];at+=2
        if kind==0x150d:
            attr,typ=struct.unpack_from('<HI',z,at);offset,at=number(z,at+6);name,at=string(z,at)
            result.append({'name':name,'type':typename(typ),'index':hex(typ),'offset':hex(offset)})
        elif kind in [0x150e,0x1510]:
            attr,typ=struct.unpack_from('<HI',z,at);name,at=string(z,at+6)
        elif kind==0x150f:
            count,typ=struct.unpack_from('<HI',z,at);name,at=string(z,at+6)
        elif kind==0x1511:
            attr,typ=struct.unpack_from('<HI',z,at);at+=6
            if (attr>>2)&7 in [4,6]:at+=4
            name,at=string(z,at)
        elif kind==0x1400:
            attr,typ=struct.unpack_from('<HI',z,at);offset,at=number(z,at+6)
            result.append({'base':typename(typ),'index':hex(typ),'offset':hex(offset)})
        elif kind==0x1409:at+=6
        elif kind==0x1502:
            attr=struct.unpack_from('<H',z,at)[0];value,at=number(z,at+2);name,at=string(z,at)
            result.append({'name':name,'value':value})
        elif kind==0x1404:
            _,typ=struct.unpack_from('<HI',z,at);at+=6;result.extend(fields(typ))
        else:raise ValueError((hex(kind),hex(ti),hex(at-2),z[at-2:at+32].hex()))
    return result
classes=[]
for ti,(leaf,z) in records.items():
    if leaf==0x1507:
        count,props,underlying,field=struct.unpack_from('<HHII',z);name,_=string(z,12)
        if '_E_SSM_RETURNCODE' in name and not props&0x80:
            classes.append({'enum':name,'index':hex(ti),'underlying':typename(underlying),'fields':fields(field)})
        continue
    if leaf not in [0x1504,0x1505]:continue
    count,props,field,derived,vshape=struct.unpack_from('<HHIII',z);size,at=number(z,16);name,_=string(z,at)
    if (name in ['XGRAPHICS::Compiler','XGRAPHICS::Arena','XGRAPHICS::Chunk','SETJMP_VECTOR4'] or name.startswith('XGRAPHICS::Compiler::')) and not props&0x80:
        classes.append({'name':name,'index':hex(ti),'size':hex(size),'fields':fields(field)})
out=OUT/(path.stem+'-classes.json');out.write_text(json.dumps(classes,indent=2)+'\n')
print(json.dumps(classes,indent=2))
