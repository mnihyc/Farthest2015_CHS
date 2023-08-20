import struct
from typing import Callable

class CStruct(object):
	__slots__ = ('buf', 'pos')
	
	def __init__(self, buf=b''):
		self.set(buf)
	
	def set(self, buf: bytes):
		self.buf = buf
		self.pos = 0
	
	def get(self) -> bytes:
		return self.buf

	def append(self, buf: bytes):
		self.buf = self.buf + buf
	
	def __check(self, pos: int):
		if pos >= len(self.buf):
			raise RuntimeError('position overflow')
	
	def unpack(self, fmt: str = '', pos: int = -1):
		pos = self.pos if pos == -1 else pos
		sz = struct.calcsize(fmt)
		self.__check(pos + sz - 1)
		self.pos = pos + sz
		return struct.unpack_from(fmt, self.buf, pos)
	
	def pack(self, fmt: str = '', *args, pos: int = -1):
		pos = len(self.buf) if pos == -1 else pos
		ts = struct.pack(fmt, *args)
		self.buf = (self.buf[:pos] if pos>=0 else b'') + ts \
			+ (self.buf[pos+len(ts):] if pos+len(ts)<len(self.buf) else b'')
		self.pos = len(self.buf)
		return ts
	
	def bitwise(self, func: Callable[[bytearray], None], start: int = 0, end: int = -1):
		end = len(self.buf) if end == -1 else end + 1
		ts = bytearray(self.buf[start:end])
		func(ts)
		self.buf = self.buf[:start] + bytes(ts) + self.buf[end:]
		return bytes(ts)
	
	def calcsize(self, fmt: str = None):
		return struct.calcsize(fmt) if fmt is not None else len(self.buf)
	
	def from_file(self, filepath: str):
		with open(filepath, 'rb') as f:
			self.set(f.read())
	
	def to_file(self, filepath: str):
		with open(filepath, 'wb') as f:
			f.write(self.get())
	


