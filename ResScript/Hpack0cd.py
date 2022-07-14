import sys
from CStruct import CStruct

def RoundFunc(ts):
  x = 27
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ x
    x = (i + ((x ^ 0xA1) >> 1)) % 256

'''
WinMain -> InitializeGame -> CLinkerLabelDatabase::readFile
File Format 0.cd
struct
{
  int cnt; // total number of blocks
  int offset[cnt]; // offsets of each block
  short sign; // for validation
  struct // one block
  {
    int num; // total number of strings
    int offset[num]; // offsets in this block
    short sign; // for validation
    struct // one string
    {
      int len; // size of this string
      char str[len]; // string with trailing zero
      int abcd[4]; // unknown
      short sign; // for validation
    }string[num];
  }block[cnt];
}pack;
'''

def Unpack0cd(filepath, outpath, enc):
  cs = CStruct()
  cs.from_file(filepath)
  cs.bitwise(RoundFunc)
  cnt, = cs.unpack('<I')
  cs.unpack('<{}I'.format(cnt))
  cs.unpack('<H')
  res = []
  for i in range(cnt):
    num, = cs.unpack('<I')
    cs.unpack('<{}I'.format(num))
    cs.unpack('<H')
    tres = []
    for j in range(num):
      sl, = cs.unpack('<I')
      s, = cs.unpack('<{}s'.format(sl-1))
      cs.unpack('<b') # trailing zero
      a,b,c,d  = cs.unpack('<4I')
      cs.unpack('<H')
      tres.append((a,b,c,d,s))
    res.append(tres)
  with open(outpath, 'w', encoding=enc) as f:
    for i in range(len(res)):
      for a,b,c,d,s in res[i]:
        f.write('Blk {}, {:10}, {:10}, {:10}, {:10}, {:5}"{}"\n'.format(
                  i, hex(a), hex(b), hex(c), hex(d), '', s.decode(enc)))
  print('Hpack0cd: {}/{} blocks processed'.format(cnt, len([1 for i in res for j in i])))

def Repack0cd(filepath, outpath, enc):
  with open(filepath, 'r', encoding=enc) as f:
    lines = f.readlines()
  cs = CStruct()
  res = []; tres = []; idx = 0
  pos = 0; tpos = 0
  for l in lines:
    p = [k.strip() for k in l.split(',')]
    nidx = int(p[0].split(' ')[1])
    if nidx != idx:
      res.append((tres,pos))
      pos = pos + cs.calcsize('<I{}IH'.format(len(tres))) + tpos # offset
      tres = []
      idx = nidx
      tpos = 0
    a,b,c,d = int(p[1],16),int(p[2],16),int(p[3],16),int(p[4],16)
    s = p[5][p[5].find('"')+1 : p[5].rfind('"')].encode(enc)
    tres.append((a,b,c,d,s,tpos))
    tpos = tpos + cs.calcsize('<I{}sb4IH'.format(len(s))) # block pos
  res.append((tres,pos))
  num = len(res)
  cs.pack('<I', num)
  pin = cs.calcsize('<I{}IH'.format(num))
  cs.pack('<{}I'.format(num), *map(lambda x: pin + x, [p for _,p in res]))
  cs.pack('<H', 0)
  for i,_ in res:
    num = len(i)
    cs.pack('<I', num)
    pin = cs.calcsize('<I{}IH'.format(num))
    cs.pack('<{}I'.format(num), *map(lambda x: pin + x, [p for _,_,_,_,_,p in i]))
    cs.pack('<H', 0)
    for a,b,c,d,s,_ in i:
      cs.pack('<I{}sb4IH'.format(len(s)),len(s)+1,s,0,a,b,c,d,0) # block
  cs.bitwise(RoundFunc)
  cs.to_file(outpath)
  print('Hpack0cd: {}/{} blocks processed'.format(len(res), len([1 for i,_ in res for j in i])))

if __name__=='__main__':
  if len(sys.argv) != 5:
    print("Usage: Hpack0cd.py unpack/pack <in_file> <out_file> <encoding>")
    sys.exit(1)
  if sys.argv[1] == "unpack":
    Unpack0cd(sys.argv[2], sys.argv[3], sys.argv[4])
  elif sys.argv[1] == "pack":
    Repack0cd(sys.argv[2], sys.argv[3], sys.argv[4])
  else:
    print("Hpack0cd.py: Nothing to do")
    sys.exit(1)