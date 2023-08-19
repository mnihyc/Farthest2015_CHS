import sys

if __name__=='__main__':
	if len(sys.argv) < 5:
		print("Usage: EncConv.py [shift-jis] <in_file> [utf-8] <out_file>")
		sys.exit(1)
	inenc, infile, outenc, outfile = sys.argv[1], sys.argv[2].lower(), sys.argv[3], sys.argv[4].lower()
	with open(infile, 'rb') as f:
		t = f.read()
		s = t.decode(inenc)
	# convertion from shift-jis to gbk; replace invalid characters
	if outenc == 'gbk':
		rep = {
			'\u266a': '\u2605', # ♪ -> ★
			'\u30fb': '\u00b7', # ・ -> ·
			'\uff65': '\u00b7', # ･ -> ·
			'\uff6c': '\u30e3', # ｬ -> ャ
			'\uff70': '\u002d', # ｰ -> -
			'\uff72': '\u30a3', # ｲ -> ィ
			'\uff73': '\u30a5', # ｳ -> ゥ
			'\uff74': '\u30a7', # ｴ -> ェ
			'\uff75': '\u30a9', # ｵ -> ォ
			'\uff7c': '\u30b7', # ｼ -> シ
			'\uff7d': '\u30b9', # ｽ -> ス
			'\uff7e': '\u30bb', # ｾ -> セ
			'\uff83': '\u30c6', # ﾃ -> テ
			'\uff84': '\u30c8', # ﾄ -> ト
			'\uff87': '\u30cc', # ﾇ -> ヌ
			'\uff8c': '\u30d5', # ﾌ -> フ
			'\uff8f': '\u30de', # ﾏ -> マ
			'\uff99': '\u30eb', # ﾙ -> ル
			'\uff9c': '\u30ee', # ﾜ -> ヮ
			'\uff9d': '\u30f3', # ﾝ -> ン
			'\uff9e': '\u309b', # ﾞ -> ゛
			'\uff9f': '\u309c', # ﾟ -> ﾟ
		}
		for rf, rt in rep.items():
			if rf in s:
				s = s.replace(rf, rt)
	with open(outfile, 'wb') as g:
		s = s.encode(outenc)
		g.write(s)
	print('EncConv: in {}bytes, out {}bytes'.format(len(t), len(s)))