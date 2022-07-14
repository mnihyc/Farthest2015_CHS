import sys
from CStruct import CStruct

enc = ''
def RoundNameFunc(ts):
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ 0x16
def RoundTextFunc(ts):
  for i in range(0, len(ts)):
    ts[i] = ts[i] ^ 0x53

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
  byte second_block[second_cnt]; // second block
  byte third_block[third_count]; // third block
  int sign[4]; // for validation
}pack;
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
def InstCalcVarVal(**kwargs):
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
def InstCalcVarVar(**kwargs):
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
      raise Exception('Instruction WiGVar offset error')
    t = trd.unpack('<5H', off)
    return '{}, ({},{},{},{},{})'.format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
def InstExScrCond(**kwargs):
  cs = kwargs['cs']
  trd = kwargs['trd']
  if 's' not in kwargs.keys():
    a, off, _ = cs.unpack('<HII')
    if off != trd.pos:
      raise Exception('Instruction ExScrCond offset error')
    t = trd.unpack('<II', off)
    return '{}, ({},{})'.format(hex(a), *map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction ExScrVIdx offset error')
    a, num = trd.unpack('<II', off)
    t = trd.unpack('<{}I'.format(num))
    return ('({},('+('{},'*num)[:-1]+'))').format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction ExScrVVar offset error')
    a, num = trd.unpack('<II', off)
    t = trd.unpack('<{}I'.format(num*2))
    return ('({},'+('({},{})'*num)[:-1]+')').format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction NDNII offset error')
    t = trd.unpack('<II', off)
    return '({},{})'.format(*map(hex,t))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction LoadSimpStr offset error')
    s = trd.unpack('<{}s'.format(a), off)
    return '("{}")'.format(s.decode(enc))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
     char str[len]; // without trailing zero
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
      raise Exception('Instruction ShowText offset error')
    a, off, idx, cnt = trd.unpack('<IIII', off0)
    res = []
    for i in range(cnt):
      l, = trd.unpack('<I')
      s, = trd.unpack('<{}s'.format(l))
      res.append(s.decode(enc))
    b, c = trd.unpack('<II')
    return '{}, ({},'+'"{}",'*cnt+'{},{},{})'.format(hex(a0), hex(idx), *res, *map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
    a0, idx = int(p[0],16), int(p[1],16)
    c,b,a = int(p[-1],16),int(p[-2],16),int(p[-3],16)
    res =[k[k.find('"')+1 : k.rfind('"')].encode(enc) for k in p[2:-3]]
    off0 = trd.pos
    trd.pack('<IIII', a, 8+sum([len(p)+4 for p in res]), idx, len(res))
    for p in res:
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
      raise Exception('Instruction TextRbFS offset error')
    a,b,c,d = trd.unpack('<HHII', off)
    l, = trd.unpack('<I')
    s, = trd.unpack('<{}s'.format(l))
    return '({},{},{},{},"{}")'.format(*map(hex,(a,b,c,d)), s.decode(enc))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
    a,b,c,d = map(lambda x: int(x,16), p[:4])
    s = p[4][p[4].find('"')+1 : p[4].rfind('"')].encode(enc)
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
      raise Exception('Instruction TextHypLnk offset error')
    a,b,c,var,idx,f = trd.unpack('<HHHIII', off)
    return '({},{},{},{},{},{})'.format(*map(hex,(a,b,c,var,idx,f)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction TextFont offset error')
    a,b,c,d,e,f = trd.unpack('<HHHHII', off)
    return '({},{},{},{},{},{})'.format(*map(hex,(a,b,c,d,e,f)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
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
      raise Exception('Instruction GLSPTextFont offset error')
    a,b,c = trd.unpack('<HII', off)
    return '({},{},{})'.format(*map(hex,(a,b,c)))
  else:
    s = kwargs['s']
    p = [k.strip().strip('()').strip() for k in s.split(',')]
    a,b,c = map(lambda x: int(x,16), p)
    off = trd.pos
    trd.pack('<HII', a, b, c)
    return cs.pack('<HII', 0, off, 0)

inst = {
  0x00: ('0x00_Pass', InstNone),
  0x01: ('0x01_MovVal', InstCalcVarVal),
  0x02: ('0x02_AddVal', InstCalcVarVal),
  0x03: ('0x03_SubVar', InstCalcVarVar),
  0x04: ('0x04_MulVal', InstCalcVarVal),
  0x05: ('0x05_DivVal', InstCalcVarVal),
  0x06: ('0x06_AndVal', InstCalcVarVal),
  0x07: ('0x07_OrVal', InstCalcVarVal),
  0x08: ('0x08_MovVar', InstCalcVarVar),
  0x09: ('0x09_AddVar', InstCalcVarVar),
  0x0A: ('0x0A_SubVar', InstCalcVarVar),
  0x0B: ('0x0B_MulVar', InstCalcVarVar),
  0x0C: ('0x0C_DivVar', InstCalcVarVar),
  0x0D: ('0x0D_AndVar', InstCalcVarVar),
  0x0E: ('0x0E_OrVar', InstCalcVarVar),
  0x0F: ('0x0F_GenGlobRnd', InstNDN),
  0x10: ('0x10_LoadScen', InstNDN),
  0x11: ('0x11_LoadScen', InstNDN),
  0x12: ('0x12_SetWiGVar', InstWiGVar),
  0x13: ('0x13_AddWiGVar', InstWiGVar),
  0x14: ('0x14_OrWiGVar', InstWiGVar),
  0x15: ('0x15_JmpScrFB', InstWNN),
  0x16: ('0x16_JmpScrEqVal', InstExScrCond),
  0x17: ('0x17_JmpScrNEqVal', InstExScrCond),
  0x18: ('0x18_JmpScrLsVal', InstExScrCond),
  0x19: ('0x19_JmpScrGrVal', InstExScrCond),
  0x1A: ('0x1A_JmpScrLEqVal', InstExScrCond),
  0x1B: ('0x1B_JmpScrGEqVal', InstExScrCond),
  0x1C: ('0x1C_JmpScrEqVar', InstExScrCond),
  0x1D: ('0x1D_JmpScrNEqVar', InstExScrCond),
  0x1E: ('0x1E_JmpScrLsVar', InstExScrCond),
  0x1F: ('0x1F_JmpScrGrVar', InstExScrCond),
  0x20: ('0x20_JmpScrLEqVar', InstExScrCond),
  0x21: ('0x21_JmpScrGEqVar', InstExScrCond),
  0x22: ('0x22_EntScrFB', InstWNN),
  0x23: ('0x23_ExitScr', InstNone),
  0x24: ('0x24_EntScrEqVal', InstExScrCond),
  0x25: ('0x25_EntScrNEqVal', InstExScrCond),
  0x26: ('0x26_EntScrLsVal', InstExScrCond),
  0x27: ('0x27_EntScrGrVal', InstExScrCond),
  0x28: ('0x28_EntScrLEqVal', InstExScrCond),
  0x29: ('0x29_EntScrGEqVal', InstExScrCond),
  0x2A: ('0x2A_EntScrEqVar', InstExScrCond),
  0x2B: ('0x2B_EntScrNEqVar', InstExScrCond),
  0x2C: ('0x2C_EntScrLsVar', InstExScrCond),
  0x2D: ('0x2D_EntScrGrVar', InstExScrCond),
  0x2E: ('0x2E_EntScrLEqVar', InstExScrCond),
  0x2F: ('0x2F_EntScrGEqVar', InstExScrCond),
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

  
  
  
}

def UnpackScd(filepath, outpath, enc):
  cs = CStruct()
  cs.from_file(filepath)
  cs.bitwise(RoundNameFunc, 0x20, 0x124)

def RepackScd(filepath, outpath, enc):
  pass

if __name__=='__main__':
  if len(sys.argv) != 5:
    print("Usage: HpackScd.py unpack/pack <in_file> <out_file> <encoding>")
    sys.exit(1)
  if sys.argv[1] == "unpack":
    UnpackScd(sys.argv[2], sys.argv[3], sys.argv[4])
  elif sys.argv[1] == "pack":
    RepackScd(sys.argv[2], sys.argv[3], sys.argv[4])
  else:
    print("HpackScd.py: Nothing to do")
    sys.exit(1)