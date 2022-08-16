import os
import sys

enc = ''

def TextExp(dp, outpath):
  fcs = [0,] * 200
  for file in [f for f in os.listdir(dp) if f.endswith('.txt')]:
    if file[:4].isdecimal():
      with open(os.path.join(dp, file), 'r', encoding=enc) as f:
        fcs[int(file[:4])] = [file, [line.strip() for line in f.readlines()]]
  while not fcs[-1]:
    fcs = fcs[:-1]
  ln = 0
  with open(outpath, 'w', encoding=enc) as f:
    for fn,fc in fcs:
      text = [line for line in fc if line.startswith('0x40_') or line.startswith('0x41_')]
      for line in text:
        p = [k.strip().strip('()[]').strip() for k in line[line.find(':')+1 :].split(',')]
        f.write('Text {:04d}.cd, {}, ({}, {}, {})\n'.format(int(fn[:4]), p[1], *p[-3:]))
        assert(int(p[0], 16) in [0x12, 0x13])
        chna = int(p[0], 16) == 0x13
        p = p[2 : -3]
        if chna:
          f.write('CName: {}\n'.format(p[0]))
          p = p[1:]
        for i in p:
          f.write(i + '\n')
        f.write('\n')
        ln = ln + 1
  print('HTextExtract: {} files, {} lines processed'.format(len(fcs), ln))

def TextImp(dp, outpath):
  fcs = [0,] * 200
  for file in [f for f in os.listdir(dp) if f.endswith('.txt')]:
    if file[:4].isdecimal():
      with open(os.path.join(dp, file), 'r', encoding=enc) as f:
        fcs[int(file[:4])] = [file, [line.strip() for line in f.readlines()]]
  while not fcs[-1]:
    fcs = fcs[:-1]
  with open(outpath, 'r', encoding=enc) as f:
    lines = [line.strip() for line in f.readlines()]
  blks = '\n'.join(lines).split('\n\n')
  txts = [{} for i in range(len(fcs))]
  for blk in blks:
    line = [i for i in blk.split('\n') if not i.startswith('//')]
    if line[0][:4] == 'Text':
      p = [k.strip().strip('()[]').strip() for k in line[0][5:].split(',')]
      t = []; cn = 0x12
      if line[1].startswith('CName:'):
        t.append(line[1][line[1].find(':')+1 :].strip())
        cn = 0x13
        t.extend(line[2 : -3])
      else:
        t.extend(line[1 : -3])
      txts[int(p[0].split('.')[0].strip())][int(p[1], 16)] = [cn, t, p[-3:]]
    else:
      raise Exception('unimplemented')
  for i in range(len(fcs)):
    for k in range(len(fcs[i][1])):
      if fcs[i][1][k].startswith('0x40_') or fcs[i][1][k].startswith('0x41_'):
        p = [k.strip().strip('()[]').strip() for k in fcs[i][1][k][fcs[i][1][k].find(':') :].split(',')]
        idx = int(p[1], 16)
        if idx not in txts[i]:
          continue
        fcs[i][1][k] = fcs[i][1][k][: fcs[i][1][k].find(':')+1] + ' {}, ({},'+'{},'*len(txts[i][idx])+'{},{},{})'.format(
          hex(txts[i][idx][0]), p[1], *txts[i][idx][1], *txts[i][idx][2])
  print('HTextExtract: {} files, {} lines processed'.format(len(fcs), len(blks)))

if __name__=='__main__':
  if len(sys.argv) != 5:
    print("Usage: HTextExtract.py unpack/pack <dir> <text> <encoding>")
    sys.exit(1)
  enc = sys.argv[4]
  if sys.argv[1] == "unpack":
    TextExp(sys.argv[2], sys.argv[3])
  elif sys.argv[1] == "pack":
    TextImp(sys.argv[2], sys.argv[3])
  else:
    print("HTextExtract.py: Nothing to do")
    sys.exit(1)
