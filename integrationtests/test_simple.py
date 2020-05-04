# -*- coding: utf-8 -*-
"""
Basic tests of functionality for SOS
"""
import pytest


def test_main_runs(raw_vm):
    raw_vm.read_until("SOS: Startup")


def test_boots_to_user_shell(raw_vm):
    raw_vm.read_until(r"Stephen's OS \(user shell")


def test_help(vm):
    """
    Tests that the user shell can respond to input and return a help output.
    """
    output = vm.cmd('help')
    # one line for echoed input, one line for new prompt, and then need some
    # help output
    assert len(output.split('\n')) > 2
