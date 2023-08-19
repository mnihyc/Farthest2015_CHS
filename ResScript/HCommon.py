import os
from typing import Dict, Tuple, List, Callable, TypeVar

def LoadFCS(dp: str, enc: str) -> List[Tuple[str, List[str]]]:
	# file format: 0XYZ.txt
	fcs = [0,] * 200
	for file in [f for f in os.listdir(dp) if f.endswith('.txt')]:
		if file[:4].isdecimal():
			with open(os.path.join(dp, file), 'r', encoding=enc) as f:
				fcs[int(file[:4])] = [file, [line.strip() for line in f.readlines()]]
	while not fcs[-1]:
		fcs = fcs[:-1]
	return fcs

def parseInt(s: str) -> int:
    return int(s, 16) if s.startswith('0x') else int(s)

T = TypeVar('T')
def SplitParam(line: str, sfrom: str = ':', func: Callable[[str], T] = lambda k: k, split: str = ',') -> List[T]:
    return [func(k.strip().strip('()[]').strip()) for k in line.partition(sfrom)[2].strip().split(split)]


