import struct

class CStruct(object):
  def __init__(self, str=b''):
    self.set(str)
  def set(self, str):
    if type(str) is not bytes:
      raise Exception('type mismatch')
    self.str = str
    self.pos = 0
  def get(self):
    return self.str
  def __check(self, pos):
    if pos >= len(self.str):
      raise Exception('position overflow')
  def unpack(self, format='', pos=-1):
    pos = self.pos if pos == -1 else pos
    sz = struct.calcsize(format)
    self.__check(pos + sz - 1)
    self.pos = pos + sz
    return struct.unpack_from(format, self.str, pos)
  def pack(self, format='', *args, pos=-1):
    self.pos = 0
    pos = len(self.str) if pos == -1 else pos
    ts = struct.pack(format, *args)
    self.str = (self.str[:pos] if pos>=0 else b'') + ts \
      + (self.str[pos+len(ts):] if pos+len(ts)<len(self.str) else b'')
    return ts
  def bitwise(self, func, start=0, end=-1):
    end = len(self.str) if end == -1 else end + 1
    ts = bytearray(self.str[start:end])
    func(ts)
    self.str = self.str[:start] + bytes(ts) + self.str[end:]
    return bytes(ts)
  def calcsize(self, format=''):
    return struct.calcsize(format) if format!='' else len(self.str)
  def from_file(self, filepath):
    with open(filepath, 'rb') as f:
      self.set(f.read())
  def to_file(self, filepath):
    with open(filepath, 'wb') as f:
      f.write(self.get())
