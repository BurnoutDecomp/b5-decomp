import argparse
import pathlib
import sys

path = pathlib.Path(__file__).resolve()
template = path.parent.parent/'r24-batch-final/freeze.py'
body = template.read_text(encoding='utf-8-sig')
parser = argparse.ArgumentParser()
parser.add_argument('--hlb', required=True)
parser.add_argument('--hlb-receipts', nargs='+', required=True)
parser.add_argument('--connapi', required=True)
parser.add_argument('--connapi-receipts', nargs='+', required=True)
args = parser.parse_args()
for pinned, actual in [('13906f42',args.hlb),('765ed52',args.connapi)]:
    token = repr(pinned)
    assert body.count(token)==1
    body = body.replace(token,repr(actual))
old_hlb = "['wave19/hlb-finish-terra/RECEIPT.md']"
old_conn = "['wave17/network-platform-terra/connapi-control-receipt.json',\n                                        'wave17/network-platform-terra/connapi-lifecycle-receipt.json']"
assert body.count(old_hlb)==body.count(old_conn)==1
body=body.replace(old_hlb,old_hlb[:-1]+', '+', '.join(map(repr,args.hlb_receipts))+']')
body=body.replace(old_conn,old_conn[:-1]+', '+', '.join(map(repr,args.connapi_receipts))+']')
ns={'__file__':str(path),'__name__':'__main__'}
exec(compile(body,str(template),'exec'),ns)
