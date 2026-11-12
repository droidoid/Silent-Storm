import sys,struct,collections
fn=sys.argv[1]
d=open(fn,'rb').read()
n=len(d)//8192
cnt=collections.Counter()
for p in range(n):
    h=d[p*8192:p*8192+96]
    hv,typ,tfb,lvl,flag,indid=struct.unpack_from('<BBBBHH',h,0)
    objid=struct.unpack_from('<i',h,24)[0]
    slot=struct.unpack_from('<H',h,22)[0]
    tp=struct.unpack_from('<IH',h,32)
    if tp[0]!=p: cnt[('badpage',)]+=1; continue
    cnt[(objid,indid,typ)]+=1
for k,v in sorted(cnt.items(),key=lambda x:str(x[0])):
    print(k,v)
