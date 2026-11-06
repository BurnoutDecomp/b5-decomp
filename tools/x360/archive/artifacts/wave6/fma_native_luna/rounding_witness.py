import decimal, struct
from decimal import Decimal as D
ctx=decimal.getcontext(); ctx.prec=200
def F(x): return struct.unpack('<f',struct.pack('<f',float(x)))[0]
def B(x): return f'0x{struct.unpack("<I",struct.pack("<f",float(x)))[0]:08X}'
def witness(label,a,b,c,neg=False):
 a,b,c=map(F,(a,b,c)); da,db,dc=map(D.from_float,(a,b,c))
 exact=dc-da*db if neg else da*db+dc
 fused=F(exact); rounded_product=F(-da*db if neg else da*db); separate=F(D.from_float(rounded_product)+dc)
 print(f'{label}: a={a}({B(a)}) b={b}({B(b)}) c={c}({B(c)}) fused={fused}({B(fused)}) separate={separate}({B(separate)})')
d=D(2)**-23
d24=D(2)**-24
# UpdateJoltEnvelope attack branch: time=1-d, decay=1, level=d, peak=-1.
witness('Input attack fmadds',(D(1)-d),(D(1)+d),D(-1))
# UpdateJoltEnvelope release branch: time=1-d24, release=1, level=1.5.
witness('Input release fmadds',(D(1)-d24),D('-1.5'),D('1.5'))
# Perf table: source expression c-a*b, compiled/original fnmsubs.
witness('Perf table fnmsubs',D(1)+d,D('1.5'),D('1.5'),neg=True)
# Perf graph positive fmadds.
witness('Perf graph fmadds',D(1)+d,D(1)-d,D(-1))
