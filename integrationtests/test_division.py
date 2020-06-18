# -*- coding: utf-8 -*-
"""
Division tests
"""
import re

import pytest


@pytest.fixture()
def divvm(vm):
    vm.cmd('exit')
    return vm


UNSIGNED_CASES = [
    # An example of even division (i.e. no remainder)
    (15, 3, 5, 0),

    # Divisor larger than dividend (result is 0, remainder is dividend)
    (12, 500, 0, 12),

    # a random number I threw in?
    (65535, 2, 32767, 1),
    (65535, 32767, 2, 1),
]


@pytest.mark.parametrize('dividend,divisor,result,remainder', [
    # Test the cases of signed division. I verified these with gcc 10.1.0 on
    # amd64 -- Python's signed integer division semantics seem to be different
    # than C's.
    (-10, -3, 3, -1),
    (-10, 3, -3, -1),
    (10, -3, -3, 1),
    (10, 3, 3, 1),
] + UNSIGNED_CASES)
def test_sdiv(divvm, dividend, divisor, result, remainder):
    res = divvm.cmd(f'sdiv {dividend} {divisor}')
    match = re.search(r'= (-?\d+) \(rem (-?\d+)\)', res)
    assert match
    assert int(match.group(1)) == result
    assert int(match.group(2)) == remainder


@pytest.mark.parametrize('dividend,divisor,result,remainder', UNSIGNED_CASES)
def test_udiv(divvm, dividend, divisor, result, remainder):
    res = divvm.cmd(f'udiv {dividend} {divisor}')
    match = re.search(r'= (\d+) \(rem (\d+)\)', res)
    assert match
    assert int(match.group(1)) == result
    assert int(match.group(2)) == remainder
