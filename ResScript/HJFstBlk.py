import os
import sys

enc = ''

def JTableFromFB(dp):
  fcs = [0,] * 200
  for file in [f for f in os.listdir(dp) if f.endswith('.txt')]:
    if file[:4].isdecimal():
      with open(os.path.join(dp, file), 'r', encoding=enc) as f:
        fcs[int(file[:4])] = [file, [line.strip() for line in f.readlines()]]
  while not fcs[-1]:
    fcs = fcs[:-1]
  tbl = [[] for _ in range(len(fcs))]
  for i in range(len(fcs)):
    if not fcs[i]:
      raise Exception('unable to unpack JTable for isolated files')
    for k in range(2, len(fcs[i][1])):
      if fcs[i][1][k].startswith('FstBlk:'):
        ints = list(map(lambda x: int(x.strip(), 16), fcs[i][1][k][fcs[i][1][k].find('Idx')+3 : ].strip().split(',')))
        tbl[ints[1]].append(ints[3]//12)
      else:
        break
  for i in range(len(fcs)):
    tbl[i] = sorted(set(tbl[i]))
  for i in range(len(fcs)):
    for k in range(2, len(fcs[i][1])):
      if fcs[i][1][k].startswith('FstBlk:'):
        ints = list(map(lambda x: int(x.strip(), 16), fcs[i][1][k][fcs[i][1][k].find('Idx')+3 : ].strip().split(',')))
        fcs[i][1][k] = 'FstBlkTable: Idx {0}, {1:04d}.cd, {2}, JTable_{3}'.format(hex(ints[0]), ints[1], hex(ints[2]), tbl[ints[1]].index(ints[3]//12))
      else:
        break
  for i in range(len(fcs)):
    with open(os.path.join(dp, fcs[i][0]), 'w', encoding=enc) as f:
      f.write('\n'.join(fcs[i][1][:2]) + '\n')
      for k in range(2, len(fcs[i][1])):
        if not fcs[i][1][k].startswith('FstBlkTable:'):
          break
        else:
          f.write(fcs[i][1][k] + '\n')
      for l in range(k, len(fcs[i][1])):
        if l-k in tbl[i]:
          f.write('_JTable_{}:\n'.format(tbl[i].index(l-k)))
        f.write(fcs[i][1][l] + '\n')
  print('HJFstBlk: {} files, {} jtables processed'.format(len(fcs), sum([len(ti) for ti in tbl])))

def JTableToFB(dp):
  fcs = [0,] * 200
  for file in [f for f in os.listdir(dp) if f.endswith('.txt')]:
    if file[:4].isdecimal():
      with open(os.path.join(dp, file), 'r', encoding=enc) as f:
        fcs[int(file[:4])] = [file, [line.strip() for line in f.readlines()]]
  while not fcs[-1]:
    fcs = fcs[:-1]
  tbl = [{} for _ in range(len(fcs))]
  for i in range(len(fcs)):
    if not fcs[i]:
      raise Exception('unable to pack JTable for isolated files')
    for k in range(2, len(fcs[i][1])):
      if not fcs[i][1][k].startswith('FstBlkTable:'):
        break
      for l in range(k, len(fcs[i][1])):
        if fcs[i][1][l].startswith('_JTable_'):
          tbl[i][int(fcs[i][1][l].split('_')[-1].split(':')[0].strip())] = l-k
        else:
          k = k + 1
  for i in range(len(fcs)):
    for k in range(2, len(fcs[i][1])):
      if fcs[i][1][k].startswith('FstBlkTable:'):
        ints = list(map(lambda x: x.strip(), fcs[i][1][k][fcs[i][1][k].find('Idx')+3 : ].strip().split(',')))
        ints[0],ints[2] = int(ints[0], 16),int(ints[2], 16)
        ints[1] = int(ints[1].split('.')[0].strip())
        ints[3] = tbl[ints[1]][int(ints[3].split('_')[-1])]*12
        fcs[i][1][k] = 'FstBlk: Idx {}, {}, {}, {}'.format(*map(hex, ints))
      else:
        break
  for i in range(len(fcs)):
    with open(os.path.join(dp, fcs[i][0]), 'w', encoding=enc) as f:
      for k in range(len(fcs[i][1])):
        if not fcs[i][1][k].startswith('_JTable_'):
          f.write(fcs[i][1][k] + '\n')
  print('HJFstBlk: {} files, {} jtables processed'.format(len(fcs), sum([len(ti) for ti in tbl])))

if __name__=='__main__':
  if len(sys.argv) != 4:
    print("Usage: HJFstBlk.py unpack/pack <dir> <encoding>")
    sys.exit(1)
  enc = sys.argv[3]
  if sys.argv[1] == "unpack":
    JTableFromFB(sys.argv[2])
  elif sys.argv[1] == "pack":
    JTableToFB(sys.argv[2])
  else:
    print("HJFstBlk.py: Nothing to do")
    sys.exit(1)