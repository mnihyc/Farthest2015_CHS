import sys

if __name__=='__main__':
  if len(sys.argv) < 5:
    print("Usage: EncConv.py [shift-jis] <in_file> [utf-8] <out_file>")
    sys.exit(1)
  with open(sys.argv[2], 'rb') as f:
    t = f.read()
    s = t.decode(sys.argv[1]).encode(sys.argv[3])
    print('EncConv: in {}bytes, out {}bytes'.format(len(t), len(s)))
  with open(sys.argv[4], 'wb') as g:
    g.write(s)