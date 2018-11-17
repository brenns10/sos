#!/usr/bin/env python3
"""
List bits set in an integer
"""

import sys

cpsr = int(sys.argv[1])

bits = reversed(bin(cpsr)[2:])

for bit_index, value in enumerate(bits):
    if value == '1':
        print('bit {} set'.format(bit_index))
