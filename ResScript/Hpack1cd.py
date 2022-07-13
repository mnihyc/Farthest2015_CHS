import sys
from CStruct import CStruct

idt_str = bytes.fromhex('8A 71 37 F7 FE D0 11 FA 92 60 15 BE 1F 4B AC 6D')
def RoundFunc(ts):
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ idt_str[i % len(idt_str)]

'''
WinMain -> InitializeGame -> sub_46DE00 -> sub_4BBB50
File Format 1.cd
struct
{
  char head[4] = {"sVAR"}; // file header
  short cnt; // total variables
  short sign; // for validation
  struct // one variable
  {
    char head[4] = {"eVAR"}; // block header
    short idx; // variable index
    short len; // string length
    char name[len]; // variable name without trailing zero
    short sign; // for validation
  }var[cnt];
}pack;
'''

def Unpack1cd(filepath, outpath):
  cs = CStruct()
  cs.from_file(filepath)
  cs.bitwise(RoundFunc)
  res = []
  _,cnt,_ = cs.unpack('<4sHH')
  for i in range(cnt):
    _,idx,sl = cs.unpack('<4sHH')
    name,_ = cs.unpack('<{}sH'.format(sl))
    res.append((idx, name.decode()))
  with open(outpath, 'w') as f: # defaults to utf-8
    for i,s in res:
      f.write('Idx {}, {:5}"{}"\n'.format(i, '', s))
  print('Hpack1cd: {} variables processed'.format(cnt))

def Repack1cd(filepath, outpath):
  with open(filepath, 'r') as f: # defaults to utf-8
    lines = f.readlines()
  res = []
  for l in lines:
    p = [k.strip() for k in l.split(',')]
    i = int(p[0].split(' ')[1])
    s = p[1][p[1].find('"')+1 : p[1].rfind('"')]
    res.append((i, s))
  cs = CStruct()
  cs.pack('<4sHH', b'sVAR', len(res), 0)
  for i,s in res:
    cs.pack('<4sHH', b'eVAR', i, len(s))
    cs.pack('<{}sH'.format(len(s)), s.encode(), 0)
  cs.bitwise(RoundFunc)
  cs.to_file(outpath)
  print('Hpack1cd: {} variables processed'.format(len(res)))

if __name__=='__main__':
  if len(sys.argv) != 4:
    print("Usage: Hpack1cd.py unpack/pack <in_file> <out_file>")
    sys.exit(1)
  if sys.argv[1] == "unpack":
    Unpack1cd(sys.argv[2], sys.argv[3])
  elif sys.argv[1] == "pack":
    Repack1cd(sys.argv[2], sys.argv[3])
  else:
    print("Hpack1cd.py: Nothing to do")
    sys.exit(1)