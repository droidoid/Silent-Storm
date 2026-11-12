import sys,struct
fn=sys.argv[1]; obj=int(sys.argv[2]); maxr=int(sys.argv[3])
d=open(fn,'rb').read()
n=len(d)//8192; k=0
for p in range(n):
    pg=d[p*8192:(p+1)*8192]
    if struct.unpack_from('<I',pg,32)[0]!=p: continue
    if struct.unpack_from('<i',pg,24)[0]!=obj or pg[1]!=1: continue
    sc=struct.unpack_from('<H',pg,22)[0]
    for s in range(sc):
        off=struct.unpack_from('<H',pg,8192-2-2*s)[0]
        r=pg[off:off+200]
        print(p,s,off, r.hex(' '))
        k+=1
        if k>=maxr: sys.exit()
