import os, sys
from HCommon import LoadFCS, SplitParam, parseInt
from HpackScd import _FST_BLK_LINE
from typing import Dict, List, Tuple

# Lines: Idt, Name, FstBlk ...

def JTableFromFB(dp: str, enc: str):
	fcs = LoadFCS(dp, enc)
	tbl: Dict[int, List] = [[] for _ in range(len(fcs))] # rebuild jump table to label
	for fi in fcs:
		if not fi:
			raise RuntimeError('unable to unpack JTable for isolated files')
		lns = fi[1]
		for k in range(_FST_BLK_LINE, len(lns)):
			if lns[k].startswith('FstBlk:'):
				ints = SplitParam(lns[k], sfrom = 'Idx', func = parseInt)
				assert(len(ints) == 4)
				tbl[ints[1]].append(ints[3] // 12)
			else:
				break
	tbl = [sorted(set(t)) for t in tbl]
	blks: Dict[int, Dict[int, str]] = [{} for _ in range(len(fcs))] # metainfo from 0.cd
	with open(os.path.join(dp, '0.cd.txt'), 'r', encoding=enc) as f:
		for line in [line.strip() for line in filter(lambda x: x, f.readlines())]:
			assert(line.startswith('Blk '))
			p = SplitParam(line, sfrom = 'Blk')
			assert(len(p) == 6)
			assert(tbl[int(p[0])].index(int(p[1], 16) // 12) == int(p[4], 16))
			blks[int(p[0])][int(p[2], 16)] = p[5]
	for i, fi in enumerate(fcs):
		lns = fi[1]
		for k in range(_FST_BLK_LINE, len(lns)):
			if lns[k].startswith('FstBlk:'):
				ints = SplitParam(lns[k], sfrom = 'Idx', func = parseInt)
				lns[k] = 'FstBlkTable: Idx {}, {:04d}.cd, {}, JTable_{}'.format(hex(ints[0]), ints[1], hex(ints[2]), tbl[ints[1]].index(ints[3] // 12))
				if i == ints[1] and ints[0] in blks[i]:
					lns[k] += ', ' + blks[i][ints[0]]
			else:
				break
	for i, fi in enumerate(fcs):
		lns = fi[1]
		with open(os.path.join(dp, fi[0]), 'w', encoding=enc) as f:
			f.write('\n'.join(lns[:_FST_BLK_LINE]) + '\n')
			for k in range(_FST_BLK_LINE, len(lns)):
				if not lns[k].startswith('FstBlkTable:'):
					break
				else:
					f.write(lns[k] + '\n')
			for p in range(k, len(lns)):
				if p - k in tbl[i]:
					f.write('_JTable_{}:\n'.format(tbl[i].index(p - k)))
				f.write(lns[p] + '\n')
	print('HJFstBlk: {} files, {} jtables processed'.format(len(fcs), sum([len(ti) for ti in tbl])))

def JTableToFB(dp: str, enc: str):
	fcs = LoadFCS(dp, enc)
	tbl: Dict[int, Dict[int, int]] = [{} for _ in range(len(fcs))] # rebuild label to jump table
	for i, fi in enumerate(fcs):
		if not fi:
			raise Exception('unable to pack JTable for isolated files')
		lns = fi[1]
		for k in range(_FST_BLK_LINE, len(lns)):
			if not lns[k].startswith('FstBlkTable:'):
				break
		for p in range(k, len(lns)):
			if lns[p].startswith('_JTable_'):
				tbl[i][int(lns[p].rpartition('_')[2].partition(':')[0].strip())] = p - k
				k += 1
			elif lns[p].startswith('#'):
				k += 1
	blks: Dict[int, List[Tuple[int, int, int, int, str]]] = [[] for _ in range(len(fcs))] # metainfo in 0.cd
	for i, fi in enumerate(fcs):
		lns = fi[1]
		for k in range(_FST_BLK_LINE, len(lns)):
			if lns[k].startswith('FstBlkTable:'):
				ints = SplitParam(lns[k], sfrom = 'Idx')
				assert(len(ints) in [4, 5])
				ints[0], ints[2] = int(ints[0], 16), int(ints[2], 16)
				ints[1] = int(ints[1].partition('.')[0].strip())
				ints[3] = tbl[ints[1]][jid := int(ints[3].rpartition('_')[2])] * 12
				lns[k] = 'FstBlk: Idx {}, {}, {}, {}'.format(*map(hex, ints[:4]))
				if i == ints[1] and len(ints) == 5:
					blks[i].append((ints[3], ints[0], ints[2], jid, ints[4]))
			else:
				break
	with open(os.path.join(dp, '0.cd.txt'), 'w', encoding=enc) as f:
		# rebuild whole 0.cd
		for i, blk in enumerate(blks):
			for b in blk:
				f.write('Blk {}, {}, {}, {}, {}, {}\n'.format(i, *map(hex, b[:4]), b[4]))
	for fi in fcs:
		lns = fi[1]
		with open(os.path.join(dp, fi[0]), 'w', encoding=enc) as f:
			for k in range(len(lns)):
				if not lns[k].startswith('_JTable_'):
					f.write(lns[k] + '\n')
	print('HJFstBlk: {} files, {} jtables processed'.format(len(fcs), sum([len(ti) for ti in tbl])))

if __name__=='__main__':
	if len(sys.argv) != 4:
		print("Usage: HJFstBlk.py unpack/pack <dir> <encoding>")
		sys.exit(1)
	cmd, dp, enc = sys.argv[1:4]
	if cmd == "unpack":
		JTableFromFB(dp, enc)
	elif cmd == "pack":
		JTableToFB(dp, enc)
	else:
		print("HJFstBlk.py: Nothing to do")
		sys.exit(1)