import os, sys, re
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
	ntxts: Dict[int, Dict[int, Tuple[str]]] = [{} for _ in range(len(fcs))]
	nhyptxt: Dict[int, Dict[int, Tuple[int, int, int]]] = [{} for _ in range(len(fcs))]
	for blk in blks:
		line = [i for i in blk.split('\n') if not i.startswith('//') and i]
		if not line: continue
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
		elif line[0][:4] == '□□□□' or line[0][:4] == '■■■■':
			line = list(filter(lambda t: not t.startswith('□□□□') and t, line))
			assert(all(t.startswith('■■■■') for t in line))
			t = []; cdnum = idx = None
			for li in line:
				li = li[4:].partition('■■■■')
				if cdnum or idx:
					assert(cdnum == int(li[0][:4]) and idx == int(li[0][4:], 16))
				cdnum, idx = int(li[0][:4]), int(li[0][4:], 16)
				t.append(li[2])
			ntxts[cdnum][idx] = t
		elif line[0][:4] == '△△△△' or line[0][:4] == '▲▲▲▲':
			assert(len(line) == 2)
			assert(line[0].startswith('△△△△'))
			assert(line[1].startswith('▲▲▲▲'))
			li = line[1][4:].partition('▲▲▲▲')
			cdnum, idx = int(li[0][:4]), int(li[0][4:], 16)
			li = li[2].strip().split(' ')
			assert(li[0].startswith('Line=') and li[1].startswith('Char=') and li[2].startswith('Len='))
			nhyptxt[cdnum][idx] = (int(li[0][5:]), int(li[1][5:]), int(li[2][4:])) # line, char, len
		else:
			raise RuntimeError(f'unimplemented {line}')
	for i, fi in enumerate(fcs):
		lns = fi[1]
		saved_hyptxt = None
		for k in range(len(lns)):
			if lns[k].startswith('0x46_'):
				p = SplitParam(lns[k].partition('#')[0].partition('//')[0], sfrom = ':')
				assert(len(p) == 6)
				saved_hyptxt = (k, p)
			if lns[k].startswith('0x40_') or lns[k].startswith('0x41_'):
				p = SplitParam(lns[k].partition('#')[0].partition('//')[0], sfrom = ':')
				assert(len(p) >= 6)
				idx = int(p[1], 16)
				res = [k[k.find('"')+1 : k.rfind('"')].encode(enc) for k in p[2:-3]]
				if idx not in txts[i] and idx not in ntxts[i]:
					print(f'HTextExtract: text idx {i}/{hex(idx)} not found, is this expected?')
					continue
				if saved_hyptxt:
					if idx not in nhyptxt[i]:
						print(f'HTextExtract: text idx {i}/{hex(idx)} not found in hyptxt, is this expected?')
					else:
						lns[saved_hyptxt[0]] = lns[saved_hyptxt[0]].partition(':')[0] + ': ({},{},{},{},{},{})'.format(
							*nhyptxt[i][idx], *saved_hyptxt[1][3:])
					saved_hyptxt = None
				if idx in txts[i]:
					lns[k] = lns[k].partition(':')[0] + (': {}, ({},'+'{},'*len(txts[i][idx])+'{},{},{})').format(
						hex(txts[i][idx][0]), p[1], *txts[i][idx][1], *txts[i][idx][2])
				elif idx in ntxts[i]:
					texts = ntxts[i][idx]
					assert(len(texts) == len(res))
					lns[k] = lns[k].partition(':')[0] + (': {}, ({},'+'"{}",'*len(texts)+'{},{},{})').format(
						p[0], p[1], *texts, p[-3], p[-2], p[-1])
		assert(saved_hyptxt is None)
		# dry run? or replace
		with open(os.path.join(dp, fi[0]), 'w', encoding=enc) as f:
			f.write('\n'.join(lns))
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
