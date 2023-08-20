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
			int off; // offset in second_block
			int idx; // FstBlkTable Idx
			int id; // id in first_block
			int jid; // FstBlkTable JTable_id
			short sign; // for validation
		}string[num];
	}block[cnt];
}pack;
'''

def Unpack0cd(infile: str, outfile: str, enc: str):
	cs = CStruct()
	cs.from_file(infile)
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
		for _ in range(num):
			slen, = cs.unpack('<I')
			s, = cs.unpack('<{}s'.format(slen - 1))
			cs.unpack('<b') # trailing zero
			a, b, c, d	= cs.unpack('<4I')
			cs.unpack('<H')
			tres.append((a, b, c, d, s))
		res.append(tres)
	with open(outfile, 'w', encoding=enc) as f:
		for i, r in enumerate(res):
			for a, b, c, d, s in r:
				f.write('Blk {:04d} {:5}, {:10}, {:10}, {:10}, {:10}, {:5}"{}"\n'.format(
									i, '', hex(a), hex(b), hex(c), hex(d), '', s.decode(enc)))
	print('Hpack0cd: {}/{} blocks processed'.format(cnt, sum([len(i) for i in res])))

def Repack0cd(infile: str, outfile: str, enc: str):
	with open(infile, 'r', encoding=enc) as f:
		lines = f.readlines()
	cs = CStruct()
	res = []; tres = []; idx = 0
	pos = 0; tpos = 0
	for l in lines:
		p = [k.strip() for k in l.split(',')]
		nidx = int(p[0].split(' ')[1])
		if nidx != idx:
			res.append((tres, pos))
			pos = pos + cs.calcsize('<I{}IH'.format(len(tres))) + tpos # offset
			tres = []
			idx = nidx
			tpos = 0
		a, b, c, d = map(lambda x: int(x.strip(), 16), p[1:5])
		s = p[5].partition('"')[2].rpartition('"')[0].encode(enc)
		tres.append((a, b, c, d, s, tpos))
		tpos = tpos + cs.calcsize('<I{}sb4IH'.format(len(s))) # block pos
	res.append((tres, pos))
	num = len(res)
	cs.pack('<I', num)
	pin = cs.calcsize('<I{}IH'.format(num))
	cs.pack('<{}I'.format(num), *map(lambda x: pin + x, [p for _, p in res]))
	cs.pack('<H', 0)
	for i, _ in res:
		num = len(i)
		cs.pack('<I', num)
		pin = cs.calcsize('<I{}IH'.format(num))
		cs.pack('<{}I'.format(num), *map(lambda x: pin + x, [p for _, _, _, _, _, p in i]))
		cs.pack('<H', 0)
		for a, b, c, d, s, _ in i:
			cs.pack('<I{}sb4IH'.format(len(s)), len(s) + 1, s, 0, a, b, c, d, 0) # block
	cs.bitwise(RoundFunc)
	cs.to_file(outfile)
	print('Hpack0cd: {}/{} blocks processed'.format(len(res), sum([len(i) for i, _ in res])))

if __name__=='__main__':
	if len(sys.argv) != 5:
		print("Usage: Hpack0cd.py unpack/pack <in_file> <out_file> <encoding>")
		sys.exit(1)
	cmd, infile, outfile, enc = sys.argv[1:5]
	if cmd == "unpack":
		Unpack0cd(infile, outfile, enc)
	elif cmd == "pack":
		Repack0cd(infile, outfile, enc)
	else:
		print("Hpack0cd.py: Nothing to do")
		sys.exit(1)