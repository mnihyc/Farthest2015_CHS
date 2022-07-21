import sys
from CStruct import CStruct

enc = ''
def RoundNameFunc(ts):
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ 0x16
  return ts
def RoundTextFunc(ts):
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ 0x53
  return ts

'''
One possible path:
ScriptInstruction_ScriptSelection -> sub_472BA0 -> sub_472BD0 -> sub_4793B0
  -> sub_46DC90 -> sub_46DA80 -> sub_4BA6B0 -> LoadCScenarioDataFromFile
File Format 0xxx.cd
struct
{
  int sign[4]; // for validation
  int id[4]; // file identification
  char name[260]; // scenario script name
  int first_cnt, second_cnt, third_cnt; // counts
  struct // first block
  {
    int a,b,off;
  }first_block[first_cnt];
  byte second_block[second_cnt]; // second block, moved by 3*sizeof(int)
  byte third_block[third_count]; // third block
  int sign[4]; // for validation
}pack;
'''


'''
  Custom structures extracted
'''
def InstWrapFunc412DD3(idf, a, cs, trd, **kwargs):
  # Assuming this from funcs_412DD3 sub_412DB0
  idfmap = {
    0: ('skip', 0),
    1: ('<2B', 2),
    2: ('', 0),
    3: ('<2H', 2),
    4: ('<H', 1),
    5: ('', 0),
    6: ('<I', 1),
    7: ('', 0),
    8: ('<3B', 3),
    9: ('', 0),
    10: ('<5I', 5),
    11: ('<2H', 2),
    12: ('<7I', 7),
    13: ('<4I', 4),
  }
  if 's' not in kwargs.keys():
    if idfmap[idf][1] == 0:
      return ('('+','.join(['{}',]*len(a))+')').format(*map(hex,a))
    else:
      f, n = idfmap[idf]
      e = trd.unpack(f)
      return ('('+'{},'*len(a)+'('+','.join(['{}',]*n)+'))').format(*map(hex,a), *map(hex,e))
  else:
    if idfmap[idf][1] == 0:
      pass
    else:
      f, n = idfmap[idf]
      trd.pack(f, *map(lambda x: int(x,16), a))
    return

def InstWrapFunc415283(idf, a, cs, trd, **kwargs):
  # Assuming this from funcs_415283 sub_4151D0 and test results
  idfmap = {
    0: ('skip', 0),
    1: ('', 0),
    2: ('<14B', 14), # Tested
    3: ('', 0),
    4: ('<41B', 41), # Tested
    5: ('<9I', 9), # Tested
    6: ('', 0),
    7: ('', 0),
    8: ('', 0),
  }
  if 's' not in kwargs.keys():
    if idfmap[idf][1] == 0:
      return ('('+','.join(['{}',]*len(a))+')').format(*map(hex,a))
    else:
      f, n = idfmap[idf]
      e = trd.unpack(f)
      return ('('+'{},'*len(a)+'('+','.join(['{}',]*n)+'))').format(*map(hex,a), *map(hex,e))
  else:
    if idfmap[idf][1] == 0:
      pass
    else:
      f, n = idfmap[idf]
      trd.pack(f, *map(lambda x: int(x,16), a))
    return

def InstWrapSub413350(a, cs, trd, **kwargs):
  # Assuming this from test results
  idfmap = {
    512: ('<4I', 4), # Tested
    64: ('<7I', 7), # Tested
    2147483649: ('<H', 1), # Tested
    320: ('<38B', 38), # Tested
  }
  if 's' not in kwargs.keys():
    idf = a[3]
    if idfmap.get(idf, ('', 0))[1] == 0:
      return '({},{},{},{})'.format(*map(hex,a))
    else:
      f, n = idfmap[idf]
      e = trd.unpack(f)
      return ('({},{},{},{},('+','.join(['{}',]*n)+'))').format(*map(hex,a), *map(hex,e))
  else:
    if idfmap.get(idf, ('', 0))[1] == 0:
      pass
    else:
      f, n = idfmap[idf]
      trd.pack(f, *map(lambda x: int(x,16), a))
    return


'''
  Instructions extracted
'''
# N N N / N
def InstNone(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    cs.unpack('<HII')
    return ''
  else:
    s = kwargs['s']
    return cs.pack('<HII', 0, 0, 0)

# W D N / N
def InstWDN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    a, b, _ = cs.unpack('<HII')
    return '{}, {}'.format(hex(a), hex(b))
  else:
    s = kwargs['s']
    p = [k.strip() for k in s.split(',')]
    a, b = int(p[0],16), int(p[1],16)
    return cs.pack('<HII', a, b, 0)
  
# W WN N / N
def InstWWNN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    a, b, _, _ = cs.unpack('<HHHI')
    return '{}, {}'.format(hex(a), hex(b))
  else:
    s = kwargs['s']
    p = [k.strip() for k in s.split(',')]
    a, b = int(p[0],16), int(p[1],16)
    return cs.pack('<HHHI', a, b, 0, 0)

# N D N / N
def InstNDN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    _, a, _ = cs.unpack('<HII')
    return '{}'.format(hex(a))
  else:
    s = kwargs['s']
    p = [k.strip() for k in s.split(',')]
    a = int(p[0],16)
    return cs.pack('<HII', 0, a, 0)

# W D N / 5H
def InstWiGVar(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction WiGVar offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<5H', off)
    return '{}, ({},{},{},{},{})'.format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a = int(p[0],16)
    off = trd.pos
    trd.pack('<5H', *map(lambda x: int(x,16), p[1:]))
    return cs.pack('<HII', a, off, 0)
  
# W N N / N
def InstWNN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    a, _, _ = cs.unpack('<HII')
    return '{}'.format(hex(a))
  else:
    s = kwargs['s']
    a = int(s.strip(), 16)
    return cs.pack('<HII', a, 0, 0)

# W D N / II
def InstWDNII(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction WDNII offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<II', off)
    return '{}, ({},{})'.format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a = int(p[0],16)
    off = trd.pos
    trd.pack('<II', *map(lambda x: int(x,16), p[1:]))
    return cs.pack('<HII', a, off, 0)

# N D N / IInI
def InstExScrVIdx(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction ExScrVIdx offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a, num = trd.unpack('<II', off)
    t = trd.unpack('<{}I'.format(num))
    return ('({},('+('{},'*num)[:-1]+'))').format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    a, num = int(p[0],16), len(p)-2
    trd.pack('<II{}I'.format(num), a, num, *map(lambda x: int(x,16), p[2:]))
    return cs.pack('<HII', 0, off, 0)

# N D N / IInII
def InstExScrVVar(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction ExScrVVar offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a, num = trd.unpack('<II', off)
    t = trd.unpack('<{}I'.format(num*2))
    return ('({},'+('({},{})'*num)[:-1]+')').format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    a, num = int(p[0],16), (len(p)-2)//2
    trd.pack('<II{}I'.format(num*2), a, num, *map(lambda x: int(x,16), p[2:]))
    return cs.pack('<HII', 0, off, 0)

# N D N / II
def InstNDNII(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction NDNII offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<II', off)
    return '({},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<II', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

# W D N / S
def InstLoadSimpStr(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction LoadSimpStr offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    s, = trd.unpack('<{}s'.format(a), off)
    return '("{}")'.format(s.decode(enc))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    s = p[0][p[0].find('"')+1 : p[0].rfind('"')].encode(enc)
    off = trd.pos
    trd.pack('<{}s'.format(len(s)), s)
    return cs.pack('<HII', len(s), off, 0)

'''
Custom Structure from sub_475E90 sub_4A2F50
struct
{
   int a; // unknown
   int off; // to below -->
   int idx; // text index (asc)
   int cnt; // number of lines
   struct // one line
   {
     int len; // length of this line
     char str[len]; // without trailing zero (encrypted)
   }line[cnt];
   // <-- off to here = begin + 2*sizeof(int) + off
   int b,c; // unknown
}custom;
'''
# W D N / Custom
def InstShowText(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a0, off0, _ = cs.unpack('<HII')
    if off0 != trd.pos:
      raise Exception('Instruction ShowText offset error, {} expected, {} got'.format(hex(off0), hex(trd.pos)))
    a, off, idx, cnt = trd.unpack('<IIII', off0)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      s = bytes(RoundTextFunc(bytearray(s))) # decryption
      res.append(s.decode(enc))
    b, c = trd.unpack('<II')
    return ('{}, ({},'+'"{}",'*cnt+'{},{},{})').format(hex(a0), hex(idx), *res, *map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a0, idx = int(p[0],16), int(p[1],16)
    c,b,a = int(p[-1],16),int(p[-2],16),int(p[-3],16)
    res =[k[k.find('"')+1 : k.rfind('"')].encode(enc) for k in p[2:-3]]
    off0 = trd.pos
    trd.pack('<IIII', a, 8+sum([len(p)+4 for p in res]), idx, len(res))
    for p in res:
      p = bytes(RoundTextFunc(bytearray(p))) # encryption
      trd.pack('<I{}s'.format(len(p)), len(p), p)
    trd.pack('<II', b, c)
    return cs.pack('<HII', a0, off0, 0)
  
'''
Custom Structure from sub_494A60 sub_494B10
struct
{
   short a,b; // unknown
   int c,d; // unknown
   int len; // length of string
   char str[len]; // (UNKNOWN) trailing zero
}custom;
'''
# N D N / Custom
def InstTextRbFS(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TextRbFS offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c,d = trd.unpack('<HHII', off)
    l, = trd.unpack('<I')
    s, = trd.unpack('<{}s'.format(l))
    s = bytes(RoundTextFunc(bytearray(s))) # decryption
    return '({},{},{},{},"{}")'.format(*map(hex,(a,b,c,d)), s.decode(enc))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c,d = map(lambda x: int(x,16), p[:4])
    s = p[4][p[4].find('"')+1 : p[4].rfind('"')].encode(enc)
    s = bytes(RoundTextFunc(bytearray(s))) # encryption
    off = trd.pos
    trd.pack('<HHIII{}s'.format(len(s)), a, b, c, d, len(s), s)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x46
struct
{
   short a,b,c; // unknown
   int var; // a variable 
   int idx; // target index in first block
   int f; // unknown
}custom;
'''
# N D N / Custom
def InstTextHypLnk(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TextHypLnk offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c,var,idx,f = trd.unpack('<HHHIII', off)
    return '({},{},{},{},{},{})'.format(*map(hex,(a,b,c,var,idx,f)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c,var,idx,f = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<HHHIII', a, b, c, var, idx, f)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x49
struct
{
   short a,b,c,d; // unknown
   int e,f; // unknown
}custom;
'''
# N D N / Custom
def InstTextFont(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TextFont offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c,d,e,f = trd.unpack('<HHHHII', off)
    return '({},{},{},{},{},{})'.format(*map(hex,(a,b,c,d,e,f)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c,d,e,f = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<HHHHII', a, b, c, d, e, f)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x4A
struct
{
   short a; // unknown
   int b,c; // unknown
}custom;
'''
# N D N / Custom
def InstGLSPTextFont(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction GLSPTextFont offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c = trd.unpack('<HII', off)
    return '({},{},{})'.format(*map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<HII', a, b, c)
    return cs.pack('<HII', 0, off, 0)

# N WW N / N
def InstNWWN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    _, a, b, _ = cs.unpack('<HHHI')
    return '{}, {}'.format(hex(a), hex(b))
  else:
    s = kwargs['s']
    p = [k.strip() for k in s.split(',')]
    a, b = int(p[0],16), int(p[1],16)
    return cs.pack('<HHHI', 0, a, b, 0)

'''
Custom Structure from ScriptInstruction_0x55
struct
{
   byte a,b; // unknown
   int c,d; // unknown
   byte e; // unknown
}custom;
'''
# N D N / Custom
def InstAOSndB(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AOSndB offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c,d,e = trd.unpack('<BBIIB', off)
    return '({},{},{},{},{})'.format(*map(hex,(a,b,c,d,e)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c,d,e = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<BBIIB', a, b, c, d, e)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x56
struct
{
   byte a,b; // unknown
   int c,d; // unknown
}custom;
'''
# N D N / Custom
def InstAOSndC(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AOSndC offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c,d = trd.unpack('<BBII', off)
    return '({},{},{},{})'.format(*map(hex,(a,b,c,d)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c,d = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<BBII', a, b, c, d)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x59
struct
{
   int a; // unknown
   short b,c; // unknown
}custom;
'''
# N D N / Custom
def InstSndObj2(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction SndObj2 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c = trd.unpack('<IHH', off)
    return '({},{},{})'.format(*map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<IHH', a, b, c)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x5A
struct
{
   int a; // unknown
   byte b; // unknown
   int c; // unknown
}custom;
'''
# N D N / Custom
def InstSndObj3(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction SndObj3 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a,b,c = trd.unpack('<IBI', off)
    return '({},{},{})'.format(*map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a,b,c = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<IBI', a, b, c)
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x61/0x62
struct
{
   // cnt not here
   struct
   {
     int len; // length
     char str[len]; // without trailing zero (encrypted)
   }line[cnt];
}custom;
'''
# W D N / Custom
def InstScrSelTxt(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    cnt, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction ScrSelTxt offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    trd.unpack('', off)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      s = bytes(RoundTextFunc(bytearray(s))) # decryption
      res.append(s.decode(enc))
    return ('('+','.join(['"{}"',]*cnt)+')').format(*res)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    cnt = len(p)
    off = trd.pos
    for s in p:
      s = s[s.find('"')+1 : s.rfind('"')].encode(enc)
      l = len(s)
      s = bytes(RoundTextFunc(bytearray(s))) # encryption
      trd.pack('<I{}s'.format(l), l, s)
    return cs.pack('<HII', cnt, off, 0)
  
'''
Custom Structure from ScriptInstruction_0x63
struct
{
   // cnt not here
   int a; // unknown
   struct
   {
     int len; // length
     char str[len]; // without trailing zero (encrypted)
   }line[cnt];
}custom;
'''
# W D N / Custom
def InstScrSelTxtC(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    cnt, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction ScrSelTxt offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a = trd.unpack('<I', off)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      s = bytes(RoundTextFunc(bytearray(s))) # decryption
      res.append(s.decode(enc))
    return ('({}, '+','.join(['"{}"',]*cnt)+')').format(hex(a), *res)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a, p = int(p[0],16), p[1:]
    cnt = len(p)
    off = trd.pos
    trd.pack('<I', a)
    for s in p:
      s = s[s.find('"')+1 : s.rfind('"')].encode(enc)
      l = len(s)
      s = bytes(RoundTextFunc(bytearray(s))) # encryption
      trd.pack('<I{}s'.format(l), l, s)
    return cs.pack('<HII', cnt, off, 0)

'''
Custom Structure from ScriptInstruction_0x64
struct
{
   // cnt not here
   int var[cnt]; // jmp variable table
   struct
   {
     int len; // length
     char str[len]; // without trailing zero (encrypted)
   }line[cnt];
}custom;
'''
# W D N / Custom
def InstSSTxtVarA(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    cnt, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction SSTxtVarA offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    trd.unpack('', off)
    var = []
    for i in range(cnt):
      v, = trd.unpack('<I')
      var.append(v)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      s = bytes(RoundTextFunc(bytearray(s))) # decryption
      res.append(s.decode(enc))
    return ('('+'{},'*cnt+','.join(['"{}"',]*cnt)+')').format(*map(lambda x: int(x,16), var), *res)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    cnt = len(p)//2
    off = trd.pos
    for i in range(cnt):
      trd.pack('<I', p[i])
    p = p[cnt:]
    for s in p:
      s = s[s.find('"')+1 : s.rfind('"')].encode(enc)
      l = len(s)
      s = bytes(RoundTextFunc(bytearray(s))) # encryption
      trd.pack('<I{}s'.format(l), l, s)
    return cs.pack('<HII', cnt, off, 0)

'''
Custom Structure from ScriptInstruction_0x65
struct
{
   // cnt not here
   int a; // unknown
   int var[cnt]; // jmp variable table
   struct
   {
     int len; // length
     char str[len]; // without trailing zero (encrypted)
   }line[cnt];
}custom;
'''
# W D N / Custom
def InstSSTxtVarB(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    cnt, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction SSTxtVarB offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    a = trd.unpack('<I', off)
    var = []
    for i in range(cnt):
      v, = trd.unpack('<I')
      var.append(v)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      s = bytes(RoundTextFunc(bytearray(s))) # decryption
      res.append(s.decode(enc))
    return ('({},'+'{},'*cnt+','.join(['"{}"',]*cnt)+')').format(hex(a), *map(lambda x: int(x,16), var), *res)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a, p = int(p[0],16), p[1:]
    cnt = len(p)//2
    off = trd.pos
    trd.pack('<I', a)
    for i in range(cnt):
      trd.pack('<I', p[i])
    p = p[cnt:]
    for s in p:
      s = s[s.find('"')+1 : s.rfind('"')].encode(enc)
      l = len(s)
      s = bytes(RoundTextFunc(bytearray(s))) # encryption
      trd.pack('<I{}s'.format(l), l, s)
    return cs.pack('<HII', cnt, off, 0)

# N D N / III
def InstNDNIII(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction NDNIII offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<III', off)
    return '({},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<III', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x6D
struct
{
   int a,b,c; // unknown
   int e[3]; // Assuming from test result, Tested
}custom;
'''
# N D N / Custom
def InstTaskVibr(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TaskVibr offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<6I', off)
    return '({},{},{},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<6I', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x70
struct
{
   int a,b,c; // unknown
   short d,e; // unknown
   // Assuming from sub_4156D0 sub_412DB0
}custom;
'''
# N D N / Custom
def InstAdvSEftFlt(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvSEftFlt offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<IIIHH', off)
    idf = t[2]
    return InstWrapFunc412DD3(idf, t, cs, trd)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<IIIHH', *map(lambda x: int(x,16), p[:5]))
    idf = int(p[2], 16)
    InstWrapFunc412DD3(idf, p[5:], cs, trd, s='')
    return cs.pack('<HII', 0, off, 0)

# W D N / III
def InstWDNIII(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction WDNIII offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<III', off)
    return '{}, ({},{},{})'.format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    a = int(p[0],16)
    off = trd.pos
    trd.pack('<III', *map(lambda x: int(x,16), p[1:]))
    return cs.pack('<HII', a, off, 0)

# N D N / 5I
def InstTskTrsMsk(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TskTrsMsk offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<5I', off)
    return '({},{},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<5I', *map(lambda x: int(x,16), p[1:]))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x79
struct
{
   int a; // unknown
   byte b; // unknown
   int c,d,e,f,g,h,i,j; // unknown
}custom;
'''
# N D N / Custom
def InstTskTrsMsk2(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction TskTrsMsk2 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<IB8I', off)
    return '({},{},{},{},{},{},{},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<IB8I', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x7A
struct
{
   byte a; // unknown
   int b; // unknown
   byte c; // unknown
   int d; // unknown
   short e,f,g,h; // unknown
   byte i; // unknown
   short j; // unknown
   byte k; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj0(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj0 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIBIHHHHBHB', off)
    return '({},{},{},{},{},{},{},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIBIHHHHBHB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x7B
struct
{
   byte a; // unknown
   int b; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj1(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj1 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BI', off)
    return '({},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BI', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x7C/0x85
struct
{
   byte a; // unknown
   int b; // unknown
   byte c; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj211(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj211 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIB', off)
    return '({},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)
  
'''
Custom Structure from ScriptInstruction_0x7D
struct
{
   byte a; // unknown
   int b; // unknown
   byte c,d; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj3(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj3 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIBB', off)
    return '({},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIBB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x7E/0x7F/0x80
struct
{
   byte a; // unknown
   int b,c,d; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj4T6(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj4T6 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIII', off)
    return '({},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIII', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x81/0x89
struct
{
   byte a; // unknown
   int b,c; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj715(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj715 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BII', off)
    return '({},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BII', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x82
struct
{
   byte a; // unknown
   int b; // unknown
   short c; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj8(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj8 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIH', off)
    return '({},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIH', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)
  
'''
Custom Structure from ScriptInstruction_0x83
struct
{
   byte a; // unknown
   int b; // unknown
   byte c; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj9(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj9 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIB', off)
    return '({},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)
  
'''
Custom Structure from ScriptInstruction_0x84
struct
{
   byte a; // unknown
   int b; // unknown
   byte c; // unknown
   int d; // unknown
   byte f; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj10(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj10 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIBIB', off)
    return '({},{},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIBIB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x86
struct
{
   byte a; // unknown
   int b,c,d; // unknown
   // UNKNOWN, assuming from test results
   // int e1,e2,e3,e4;
   // END
}custom;
'''
# N D N / Custom
def InstAdvObj12(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj12 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIII', off)
    return InstWrapSub413350(t, cs, trd)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIII', *map(lambda x: int(x,16), p[:4]))
    InstWrapSub413350(t, cs, trd, s='')
    return cs.pack('<HII', 0, off, 0)
  
'''
Custom Structure from ScriptInstruction_0x87/0x8A
struct
{
   byte a; // unknown
   int b,c; // unknown
   byte d; // unknown
}custom;
'''
# N D N / Custom
def InstAdvObj1316(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj1316 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BIIB', off)
    return '({},{},{},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BIIB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x88
struct
{
   int a,b,c,d; // unknown
   short e,f; // unknown
   // Assuming from sub_412DB0
}custom;
'''
# N D N / Custom
def InstAdvObj14(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj14 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<IIIIHH', off)
    idf = t[3]
    return InstWrapFunc412DD3(idf, t, cs, trd)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<IIIIHH', *map(lambda x: int(x,16), p[:6]))
    idf = int(p[3], 16)
    InstWrapFunc412DD3(idf, p[6:], cs, trd, s='')
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x8B sub_40AF80
struct
{
   byte a; // unknown
   int b,c; // unknown
   // UNKNOWN, assuming from test results
   int e[9]; // Tested
   // END
}custom;
'''
# N D N / Custom
def InstAdvObj17(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvObj17 offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<BII9I', off)
    return ('({},{},{},('+','.join(['{}',]*9)+'))').format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<BII9I', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x8C
struct
{
   int a,b,c; // unknown
   // Assuming from sub_4151D0
}custom;
'''
# N D N / Custom
def InstAdvScrEnv(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction AdvScrEnv offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<III', off)
    idf = t[2]
    return InstWrapFunc415283(idf, t, cs, trd)
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<III', *map(lambda x: int(x,16), p[:3]))
    idf = int(p[2], 16)
    InstWrapFunc415283(idf, p[3:], cs, trd, s='')
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x8D sub_4154B0
struct
{
   int a; // unknown
   // UNKNOWN, assuming from test results
   // byte b; // Tested
   // END
}custom;
'''
# N D N / Custom
def InstGCLspU(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    _, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction GCLspU offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    t = trd.unpack('<IB', off)
    return '({},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    off = trd.pos
    trd.pack('<IB', *map(lambda x: int(x,16), p))
    return cs.pack('<HII', 0, off, 0)

'''
Custom Structure from ScriptInstruction_0x8E sub_434530
struct
{
   // cnt not here
   struct
   {
     short a,b,c,d,e,f; // unknown
     byte s[32]; // unknown
   }block[cnt]
}custom;
'''
# W D N / Custom
def InstEffBustUp(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    cnt, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction EffBustUp offset error, {} expected, {} got'.format(hex(off), hex(trd.pos)))
    trd.unpack('', off)
    res = []
    for i in range(cnt):
      t = trd.unpack('<6H')
      s = trd.unpack('<32B')
      res.append((t, s))
    return '({})'.format(','.join(map(lambda x: '({},{})'.format(*map(lambda y: ','.join(map(hex,y)), x)), res)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()[]').strip() for k in s.split(',')]
    cnt = len(p)//38
    off = trd.pos
    for i in range(cnt):
      trd.pack('<6H', *map(lambda x: int(x,16), p[:6]))
      trd.pack('<32B', *map(lambda x: int(x,16), p[6:38]))
      p = p[38:]
    return cs.pack('<HII', cnt, off, 0)
  
# N BN N / N
def InstNBNN(**kwargs):
  cs = kwargs['cs']
  if 's' not in kwargs.keys():
    _, a, _, _, _, _ = cs.unpack('<HB3BI')
    return '{}'.format(hex(a))
  else:
    s = kwargs['s']
    p = [k.strip() for k in s.split(',')]
    a = int(p[0],16)
    return cs.pack('<HB3BI', 0, a, [0,]*3, 0)

inst = {
  0x00: ('0x00_Pass', InstNone),
  0x01: ('0x01_MovVal', InstWDN),
  0x02: ('0x02_AddVal', InstWDN),
  0x03: ('0x03_SubVar', InstWWNN),
  0x04: ('0x04_MulVal', InstWDN),
  0x05: ('0x05_DivVal', InstWDN),
  0x06: ('0x06_AndVal', InstWDN),
  0x07: ('0x07_OrVal', InstWDN),
  0x08: ('0x08_MovVar', InstWWNN),
  0x09: ('0x09_AddVar', InstWWNN),
  0x0A: ('0x0A_SubVar', InstWWNN),
  0x0B: ('0x0B_MulVar', InstWWNN),
  0x0C: ('0x0C_DivVar', InstWWNN),
  0x0D: ('0x0D_AndVar', InstWWNN),
  0x0E: ('0x0E_OrVar', InstWWNN),
  0x0F: ('0x0F_GenGlobRnd', InstNDN),
  0x10: ('0x10_LoadScen', InstNDN),
  0x11: ('0x11_LoadScen', InstNDN),
  0x12: ('0x12_SetWiGVar', InstWiGVar),
  0x13: ('0x13_AddWiGVar', InstWiGVar),
  0x14: ('0x14_OrWiGVar', InstWiGVar),
  0x15: ('0x15_JmpScrFB', InstWNN),
  0x16: ('0x16_JmpScrEqVal', InstWDNII),
  0x17: ('0x17_JmpScrNEqVal', InstWDNII),
  0x18: ('0x18_JmpScrLsVal', InstWDNII),
  0x19: ('0x19_JmpScrGrVal', InstWDNII),
  0x1A: ('0x1A_JmpScrLEqVal', InstWDNII),
  0x1B: ('0x1B_JmpScrGEqVal', InstWDNII),
  0x1C: ('0x1C_JmpScrEqVar', InstWDNII),
  0x1D: ('0x1D_JmpScrNEqVar', InstWDNII),
  0x1E: ('0x1E_JmpScrLsVar', InstWDNII),
  0x1F: ('0x1F_JmpScrGrVar', InstWDNII),
  0x20: ('0x20_JmpScrLEqVar', InstWDNII),
  0x21: ('0x21_JmpScrGEqVar', InstWDNII),
  0x22: ('0x22_EntScrFB', InstWNN),
  0x23: ('0x23_ExitScr', InstNone),
  0x24: ('0x24_EntScrEqVal', InstWDNII),
  0x25: ('0x25_EntScrNEqVal', InstWDNII),
  0x26: ('0x26_EntScrLsVal', InstWDNII),
  0x27: ('0x27_EntScrGrVal', InstWDNII),
  0x28: ('0x28_EntScrLEqVal', InstWDNII),
  0x29: ('0x29_EntScrGEqVal', InstWDNII),
  0x2A: ('0x2A_EntScrEqVar', InstWDNII),
  0x2B: ('0x2B_EntScrNEqVar', InstWDNII),
  0x2C: ('0x2C_EntScrLsVar', InstWDNII),
  0x2D: ('0x2D_EntScrGrVar', InstWDNII),
  0x2E: ('0x2E_EntScrLEqVar', InstWDNII),
  0x2F: ('0x2F_EntScrGEqVar', InstWDNII),
  0x30: ('0x30_JmpScrFVIdx', InstExScrVIdx),
  0x31: ('0x31_JmpScrFVVar', InstExScrVVar),
  0x32: ('0x32_NJmpScrFVIdx', InstExScrVIdx),
  0x33: ('0x33_NJmpScrFVVar', InstExScrVVar),
  0x34: ('0x34_Set2GlobVar', InstWNN),
  0x35: ('0x35_Set1GlobVar', InstWNN),
  0x36: ('0x36_U_EntChapGVGr', InstNDNII),
  0x37: ('0x37_U_EntChapGVLs', InstNDNII),
  0x38: ('0x38_U_ChgChapName', InstNDNII),
  0x39: ('0x39_U_ClrChapHis', InstNone),
  0x3A: ('0x3A_Pass', InstNone),
  0x3B: ('0x3B_LoadStriGLSP', InstLoadSimpStr),
  0x3C: ('0x3C_IncStrCGLSP', InstNone),
  0x3D: ('0x3D_DecStrCGLSP', InstNone),
  0x3E: ('0x3E_ClrStrCGLSP', InstNone),
  0x3F: ('0x3E_SetStrCGLSP', InstNDN),
  0x40: ('0x40_ShowText', InstShowText),
  0x41: ('0x41_ShowText', InstShowText),
  0x42: ('0x42_U_CrtTextWin', InstNDN),
  0x43: ('0x43_U_ClsTextWin', InstNDN),
  0x44: ('0x44_U_TextRbSwFS', InstTextRbFS),
  0x45: ('0x45_U_TextRbDelFS', InstNDN),
  0x46: ('0x46_TextHypLnkFB', InstTextHypLnk),
  0x47: ('0x47_U_MenuSound', InstNone),
  0x48: ('0x48_U_ChangeScene', InstNone),
  0x49: ('0x49_U_TextFont', InstTextFont),
  0x4A: ('0x4A_U_GLSPTextFont', InstGLSPTextFont),
  0x4B: ('0x4B_U_Set0GCLSP', InstNDN),
  0x4C: ('0x4C_U_SetVGCLSP', InstNDN),
  0x4D: ('0x4D_U_SetVGCLSPF', InstNDN),
  0x4E: ('0x4E_U_TextIndent', InstNWWN),
  0x4F: ('0x4F_U_AdvSkinMsgWin', InstNDN),
  0x50: ('0x50_U_ResizeTxtWin', InstNWWN),
  0x51: ('0x51_U_SkipTxtTrsA', InstWDN),
  0x52: ('0x52_U_SkipTxtTrsB', InstWDN),
  0x53: ('0x53_U_SkipTxtTrsC', InstWDN),
  0x54: ('0x54_U_AdvObjSndA', InstWDN),
  0x55: ('0x55_U_AdvObjSndB', InstAOSndB),
  0x56: ('0x56_U_AdvObjSndC', InstAOSndC),
  0x57: ('0x57_U_SndObj1', InstWDN),
  0x58: ('0x58_U_SndObj0', InstWDN),
  0x59: ('0x59_U_SndObj2', InstSndObj2),
  0x5A: ('0x5A_U_SndObj3', InstSndObj3),
  0x5B: ('0x5B_U_SndObjA', InstWDN),
  0x5C: ('0x5C_U_GlobSave', InstNDN),
  0x5D: ('0x5D_U_SndObj', InstNone),
  0x5E: ('0x5E_U_LSavSndManA', InstNone),
  0x5F: ('0x5F_U_LSavSndManB', InstWDN),
  0x60: ('0x60_U_GLSPSndMan', InstNone),
  0x61: ('0x61_ScrSelA', InstScrSelTxt),
  0x62: ('0x62_ScrSelDstB', InstScrSelTxt),
  0x63: ('0x63_ScrSelDstC', InstScrSelTxtC),
  0x64: ('0x64_ScrSelDstVarA', InstSSTxtVarA),
  0x65: ('0x65_ScrSelDstVarB', InstSSTxtVarB),
  0x66: ('0x66_U_WinWaitIcon', InstNWWN),
  0x67: ('0x67_Pass', InstNone),
  0x68: ('0x68_U_WIconTray0', InstNDN),
  0x69: ('0x69_U_WIconTray1', InstNone),
  0x6A: ('0x6A_U_SndObjC', InstNone),
  0x6B: ('0x6B_U_SwScenario', InstNDN),
  0x6C: ('0x6C_U_SAGlobVar', InstNone),
  0x6D: ('0x6D_TaskVibrate', InstTaskVibr),
  0x6E: ('0x6E_TaskFlash', InstNDN),
  0x6F: ('0x6F_EftSurprise', InstNDNII),
  0x70: ('0x70_AdvSEftFlt', InstAdvSEftFlt),
  0x71: ('0x71_TaskTransZ', InstNone),
  0x72: ('0x72_TaskFade0', InstNWWN),
  0x73: False,
  0x74: ('0x74_TaskFade1', InstNWWN),
  0x75: False,
  0x76: ('0x76_TskTrsZMore', InstWDNIII),
  0x77: ('0x77_TskTrsMsk0', InstTskTrsMsk),
  0x78: ('0x78_TskTrsMsk1', InstTskTrsMsk),
  0x79: ('0x79_TskTrsMsk2', InstTskTrsMsk2),
  0x7A: ('0x7A_U_AdvObj0', InstAdvObj0),
  0x7B: ('0x7B_U_AdvObj1', InstAdvObj1),
  0x7C: ('0x7C_U_AdvObj2', InstAdvObj211),
  0x7D: ('0x7D_U_AdvObj3', InstAdvObj3),
  0x7E: ('0x7E_U_AdvObj4', InstAdvObj4T6),
  0x7F: ('0x7F_U_AdvObj5', InstAdvObj4T6),
  0x80: ('0x80_U_AdvObj6', InstAdvObj4T6),
  0x81: ('0x81_U_AdvObj7', InstAdvObj715),
  0x82: ('0x82_U_AdvObj8', InstAdvObj8),
  0x83: ('0x83_U_AdvObj9', InstAdvObj9),
  0x84: ('0x84_U_AdvObj10', InstAdvObj10),
  0x85: ('0x85_U_AdvObj11', InstAdvObj211),
  0x86: ('0x86_U_AdvObj12', InstAdvObj12),
  0x87: ('0x87_U_AdvObj13', InstAdvObj1316),
  0x88: ('0x88_U_AdvObj14', InstAdvObj14),
  0x89: ('0x89_U_AdvObj15', InstAdvObj715),
  0x8A: ('0x8A_U_AdvObj1316', InstAdvObj1316),
  0x8B: ('0x8B_U_AdvObj17', InstAdvObj17),
  0x8C: ('0x8C_U_AdvScrEnv', InstAdvScrEnv),
  0x8D: ('0x8D_U_GCLspU', InstGCLspU),
  0x8E: ('0x8E_U_EffBustUp', InstEffBustUp),
  0x8F: ('0x8F_U_CAdvObj', InstNone),
  0x90: ('0x90_U_MAdvObj', InstNDNIII),
  0x91: ('0x91_ShowStaff', InstNDN),
  0x92: ('0x92_ShowMenu', InstNone),
  0x93: ('0x93_U_StyMemCpy', InstNDN),
  0x94: ('0x94_U_ExtraMode', InstNone),
  0x95: ('0x95_U_GlobByteNE', InstNone),
  0x96: ('0x96_U_Menu', InstNone),
  0x97: ('0x97_PlayMovie', InstNWWN),
  0x98: False,
  0x99: ('0x97_U_SGlobByte', InstNBNN),
  0x9A: ('0x9A_U_ScrTPRecv', InstNone),
  0x9B: ('0x9B_U_SelGirlVar', InstWWNN),
  0x9C: ('0x9C_U_SetRndGVar', InstNone),
  0x9D: ('0x9D_U_SetGlobVar', InstNDN),
}

def UnpackScd(filepath, outpath):
  cs = CStruct()
  cs.from_file(filepath)
  cs.bitwise(RoundNameFunc, 0x20, 0x124-1)
  sign = cs.unpack('<4I')
  idt = cs.unpack('<4I')
  name, = cs.unpack('<260s')
  name = name.strip(b'\x00').decode(enc).strip()
  first_cnt, second_cnt, third_cnt = cs.unpack('<3I')
  first_block = []
  for i in range(first_cnt):
    first_block.append(cs.unpack('<3I'))
  trd = CStruct()
  eb2 = cs.pos+second_cnt; eb3 = eb2+third_cnt
  trd.set(cs.get()[eb2 : eb3])
  s = []
  while cs.pos<eb2:
    cmd, = cs.unpack('<H')
    #print('cmd {} ipos {} tbpos {}'.format(inst[cmd][0],hex(cs.pos),hex(trd.pos)))
    t = inst[cmd][0] + ': ' + inst[cmd][1](cs=cs, trd=trd)
    s.append(t + '\n')
  with open(outpath, 'w', encoding=enc) as f:
    f.write('Idt: {}, {}, {}, {}\n'.format(*map(hex, idt)))
    f.write('Name: "{}"\n'.format(name))
    for i in range(first_cnt):
      f.write('FstBlk: Idx {}, {}, {}, {}\n'.format(hex(i), *map(hex, first_block[i])))
    for i in s:
      f.write(i)
  filename = filepath.split('\\')[-1].split('/')[-1].strip()
  print('HpackScd {}: {} commands processed'.format(filename, len(s)))
  
def RepackScd(filepath, outpath):
  pass

if __name__=='__main__':
  if len(sys.argv) != 5:
    print("Usage: HpackScd.py unpack/pack <in_file> <out_file> <encoding>")
    sys.exit(1)
  enc = sys.argv[4]
  if sys.argv[1] == "unpack":
    UnpackScd(sys.argv[2], sys.argv[3])
  elif sys.argv[1] == "pack":
    RepackScd(sys.argv[2], sys.argv[3])
  else:
    print("HpackScd.py: Nothing to do")
    sys.exit(1)