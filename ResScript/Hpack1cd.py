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

def Unpack1cd(infile: str, outfile: str):
	cs = CStruct()
	cs.from_file(infile)
	cs.bitwise(RoundFunc)
	res = []
	_, cnt, _ = cs.unpack('<4sHH')
	for i in range(cnt):
		_, idx, slen = cs.unpack('<4sHH')
		name, _ = cs.unpack('<{}sH'.format(slen))
		res.append((idx, name.decode()))
	with open(outfile, 'w', encoding='utf-8') as f:
		for i, s in res:
			f.write('Idx {}, {:5}"{}"\n'.format(hex(i), '', s))
	print('Hpack1cd: {} variables processed'.format(cnt))

def Repack1cd(infile: str, outfile: str):
	with open(infile, 'r', encoding='utf-8') as f:
		lines = f.readlines()
	res = []
	for l in lines:
		p = [k.strip() for k in l.split(',')]
		i = int(p[0].split(' ')[1].strip(), 16)
		s = p[1].partition('"')[2].rpartition('"')[0]
		res.append((i, s))
	cs = CStruct()
	cs.pack('<4sHH', b'sVAR', len(res), 0)
	for i,s in res:
		cs.pack('<4sHH', b'eVAR', i, len(s))
		cs.pack('<{}sH'.format(len(s)), s.encode(), 0)
	cs.bitwise(RoundFunc)
	cs.to_file(outfile)
	print('Hpack1cd: {} variables processed'.format(len(res)))

if __name__=='__main__':
	if len(sys.argv) != 4:
		print("Usage: Hpack1cd.py unpack/pack <in_file> <out_file>")
		sys.exit(1)
	cmd, infile, outfile = sys.argv[1:4]
	if cmd == "unpack":
		Unpack1cd(infile, outfile)
	elif cmd == "pack":
		Repack1cd(infile, outfile)
	else:
		print("Hpack1cd.py: Nothing to do")
		sys.exit(1)