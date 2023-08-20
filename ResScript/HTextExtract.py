import os, sys
from HCommon import LoadFCS, SplitParam
from typing import Dict, List, Tuple

# NOTE: character "," can not be in texts
# NOTE: text can not be too long; otherwise stack overflow

def TextExp(dp: str, enc: str, outfile: str):
	fcs = LoadFCS(dp, enc)
	ln = 0
	with open(outfile, 'w', encoding=enc) as f:
		for fn, fc in fcs:
			text = [line for line in fc if line.startswith('0x40_') or line.startswith('0x41_')]
			for line in text:
				p = SplitParam(line, sfrom = ':')
				assert(len(p) >= 6)
				f.write('Text {:04d}.cd, {}, ({}, {}, {})\n'.format(int(fn[:4]), p[1], *p[-3:]))
				assert(int(p[0], 16) in [0x12, 0x13])
				chna = (int(p[0], 16) == 0x13)
				p = p[2 : -3]
				if chna:
					f.write('C: {}\n'.format(p[0]))
					p = p[1:]
				for i in p:
					f.write(i + '\n')
				f.write('\n')
				ln = ln + 1
	print('HTextExtract: {} files, {} lines processed'.format(len(fcs), ln))

def TextImp(dp: str, enc: str, outfile: str):
	fcs = LoadFCS(dp, enc)
	with open(outfile, 'r', encoding=enc) as f:
		lines = [line.strip() for line in f.readlines()]
	blks = '\n'.join(lines).split('\n\n')
	txts: Dict[int, Dict[int, Tuple[int, List[str], Tuple[int, int, int]]]] = [{} for _ in range(len(fcs))]
	for blk in blks:
		line = [i for i in blk.split('\n') if not i.startswith('//')]
		if line[0][:4] == 'Text':
			p = SplitParam(line[0], sfrom = 'Text')
			assert(len(p) == 5)
			t = []; cn = 0x12
			if line[1].startswith('C:'):
				t.append(line[1].partition(':')[2].strip())
				cn = 0x13
				t.extend(line[2 : -3])
			else:
				t.extend(line[1 : -3])
			txts[int(p[0].partition('.')[0].strip())][int(p[1], 16)] = [cn, t, p[-3:]]
		else:
			raise RuntimeError(f'unimplemented {line}')
	for i, fi in enumerate(fcs):
		lns = fi[1]
		for k in range(len(lns)):
			if lns[k].startswith('0x40_') or lns[k].startswith('0x41_'):
				p = SplitParam(lns[k], sfrom = ':')
				assert(len(p) >= 6)
				idx = int(p[1], 16)
				if idx not in txts[i]:
					print(f'HTextExtract: text idx {hex(idx)} not found, is this expected?')
					continue
				lns[k] = lns[k].partition(':')[0] + ': {}, ({},'+'{},'*len(txts[i][idx])+'{},{},{})'.format(
					hex(txts[i][idx][0]), p[1], *txts[i][idx][1], *txts[i][idx][2])
	print('HTextExtract: {} files, {} lines processed'.format(len(fcs), len(blks)))

if __name__=='__main__':
	if len(sys.argv) != 5:
		print("Usage: HTextExtract.py unpack/pack <dir> <text> <encoding>")
		sys.exit(1)
	cmd, dp, outfile, enc = sys.argv[1:5]
	if cmd == "unpack":
		TextExp(dp, enc, outfile)
	elif cmd == "pack":
		TextImp(dp, enc, outfile)
	else:
		print("HTextExtract.py: Nothing to do")
		sys.exit(1)
