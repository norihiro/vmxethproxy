#! /bin/env python3

import json
import sys

with open(sys.argv[-1]) as f:
	j = json.load(f)

print('| | Exec | Total | Coverage |')
print('| --- | ---: | ---: | ---: |')
for t1 in ('line', 'function', 'branch'):
	x = [t1.capitalize()]
	for t2 in ('covered', 'total', 'percent'):
		v = j[t1 + '_' + t2]
		if t2 == 'percent':
			v = f'{v} %'
		else:
			v = str(v)
		x.append(v)
	print('| ' + ' | '.join(x) + ' |')
