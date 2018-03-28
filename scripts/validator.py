#!/usr/bin/env python
#
#  Copyright (c) 2018 Red Hat Inc
#
# Author:
#  Eduardo Habkost <ehabkost@redhat.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""
QEMU validator script
=====================

This script will get test YAML test case specifications or Python
modules as input, and generate/run test cases based on them.

USAGE
-----

validator.py <specification-file>... -V VAR1=value1 VAR1=value2 VAR2=value3

specification-file is a YAML file containing the test specification.

Example::

    # this test specification is equivalent to the
    # "device/introspect/list" test case in device-introspect-test.c
    command-line: '$QEMU -nodefaults -machine none'
    monitor-commands:
    - qmp:
      - execute: qom-list-types
        arguments:
          implements: 'device'
          abstract: true
    - hmp: 'device_add help'


VARIABLE EXPANSION
------------------

The test runner will try to run the test cases with all possible values
for variables appearing in the test specification.

Some built-in variables are automatically expanded:

* `$MACHINE` - Expands to a machine-type name supported by $QEMU
* `$ACCEL` - Expands to an accelerator name supported by $QEMU
* `$DEVICE` - Expands to a (user-creatable) device type name supported by $QEMU
* `$CPU` - Expands to a CPU model name supported by $QEMU

Note that the $QEMU variable must be specified in th

TEST SPECIFICATION FIELDS
-------------------------

command-line
~~~~~~~~~~~~

List or string, containing the QEMU command-line to be run.

Default: '$QEMU'


monitor-commands
~~~~~~~~~~~~~~~~

Mapping or list-of-mappings containing monitor commands to run.  The key on each
item can be ``hmp`` or ``qmp``.  The value on each entry can be a string,
mapping, or list.

Default: None.

TODO: not implemented yet.


qmp
~~~

Boolean.  If true (the default), a QMP monitor is configured on the command-line
automatically.

If true, the test runner will issue a ``quit`` command automatically when
testing is finished.  If false, the test runner will wait until QEMU exits by
itself.

Example::

    # just run $QEMU -help and ensure it won't crash
    command-line: ['$QEMU', '-help']
    qmp: false


TODO: whitelist
TODO: validate output against reference output
TODO: configure defaults for variables
TODO: compatibility with Avocado multiplexer?
"""

import sys
import os
import string
import argparse
import pprint
import yaml
import logging
import shlex
import pipes
import re
import itertools
import traceback
import socket
import unittest
import doctest
from collections import OrderedDict, Iterable
from functools import wraps

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'scripts'))
from qemu import QEMUMachine
from qmp.qmp import QMPError

logger = logging.getLogger('qemu.tests.validator')
dbg = logger.debug

# Python 2.7 compatibility:
shquote = getattr(shlex, 'quote', pipes.quote)

def cmdquote(cmd):
    """Quote command-line arg list

    >>> cmdquote(['ls', '-l', 'arg with spaces'])
    "ls -l 'arg with spaces'"
    """
    return ' '.join(map(shquote, cmd))

class InvalidSpecification(Exception):
    pass

class VariableNotSet(Exception):
    pass

def qom_type_names(vm, **kwargs):
    """Run qom-list-types QMP command, return type names"""
    types = vm.command('qom-list-types', **kwargs)
    return [t['name'] for t in types]


def info_qdm(vm):
    """Parse 'info qdm' output"""
    args = {'command-line': 'info qdm'}
    devhelp = vm.command('human-monitor-command', **args)
    for l in devhelp.split('\n'):
        l = l.strip()
        if l == '' or l.endswith(':'):
            continue
        d = {'name': re.search(r'name "([^"]+)"', l).group(1),
             'no-user': (re.search(', no-user', l) is not None)}
        yield d


class QemuBinaryInfo(object):
    """Information for a specific QEMU binary"""
    def __init__(self, binary):
        """Don't instantiate this directly, use get_binary_info()"""
        self.binary = binary

        args = ['-S', '-machine', 'none,accel=kvm:tcg']
        dbg("querying info for QEMU binary: %s", binary)
        vm = QEMUMachine(binary=binary, args=args)
        vm.launch()
        try:
            self.alldevs = qom_type_names(vm, implements='device', abstract=False)
            # there's no way to query DeviceClass::user_creatable using QMP,
            # so use 'info qdm':
            self.no_user_devs = [d['name'] for d in info_qdm(vm, ) if d['no-user']]
            self.machines = [m['name'] for m in vm.command('query-machines')]
            self.user_devs = [dev for dev in self.alldevs if dev not in self.no_user_devs]
            self.kvm_available = vm.command('query-kvm')['enabled']
            self.cpu_models = [c['name'] for c in vm.command('query-cpu-definitions')]
        finally:
            vm.shutdown()

    def available_accels(self):
        if self.kvm_available:
            return ['kvm', 'tcg']
        else:
            return ['tcg']

BINARY_INFO = {}

def get_binary_info(binary):
    """Lookup info for QEMU binary, caching data"""
    if binary not in BINARY_INFO:
        BINARY_INFO[binary] = QemuBinaryInfo(binary)
    return BINARY_INFO[binary]


# HELPER FUNCTIONS FOR TEMPLATE STRINGS:

def apply_template(templ, values):
    """Apply variables to a template, supporting strings and lists

    >>> apply_template('$QEMU -machine X', {'QEMU':'qemu-system-x86_64'})
    'qemu-system-x86_64 -machine X'
    >>> apply_template({"$TEST": ["$FOO", "is $BAR"]}, \
                       {'FOO':'XX', 'BAR':'YY', 'TEST':'TT'})
    {'$TEST': ['XX', 'is YY']}
    """
    if isinstance(templ, str):
        return string.Template(templ).substitute(values)
    elif isinstance(templ, list):
        return [apply_template(s, values) for s in templ]
    elif isinstance(templ, dict):
        return dict( (k, apply_template(v, values)) for (k, v) in templ.items())
    else:
        return templ

def vars_for_template(templ):
    """Return list of variables used by s when used as template string

    >>> vars_for_template('abcde fgh')
    []
    >>> vars_for_template('$A is ${A}, not ${B} or $C')
    ['A', 'B', 'C']
    >>> vars_for_template(['$QEMU', '-machine' , '$MACHINE$MACHINE_OPT'])
    ['QEMU', 'MACHINE', 'MACHINE_OPT']
    """
    usedKeys = OrderedDict()
    class LoggingDict(object):
        def __getitem__(self, k):
            usedKeys[k] = 1
            return 'X'
    apply_template(templ, LoggingDict())
    return list(usedKeys.keys())


# TEST CASE GENERATION LOGIC
#
# To simplify debugging, all functions in the test case generation logic
# don't have any side-effects.  No lists or dictionaries are changed
# while generating test cases.  This allows the code to use generators
# everywhere to avoid building huge in-memory lists.

# Helpers for functional-style code

def updatedict(d1, d2):
    """Like dict.update(), but return new dictionary"""
    d = d1.copy()
    d.update(d2)
    return d

def newdict(d, k, v):
    """Like `d[k] = v`, but return new dictionary

    >>> a = {'a':1, 'b':2}
    >>> b = newdict(a, 'a', 100)
    >>> sorted(a.items())
    [('a', 1), ('b', 2)]
    >>> sorted(b.items())
    [('a', 100), ('b', 2)]
    """
    d = d.copy()
    d[k] = v
    return d

def mapchain(fn, l, *args, **kwargs):
    """map(fn, l) and then chain the results together

    Extra arguments are passed to `fn`.

    Useful for chaining results of functions that generate lists of items.

    >>> multiples = lambda i, m: [i*m, i*m*2, i*m*3]
    >>> list(mapchain(multiples, [2, 3, 5], 1))
    [2, 4, 6, 3, 6, 9, 5, 10, 15]
    >>> list(mapchain(multiples, [2, 3, 5], 10))
    [20, 40, 60, 30, 60, 90, 50, 100, 150]
    """
    for v in l:
        for i in fn(v, *args, **kwargs):
            yield i

# Test case list generastion functions:
#
# The functions below all get a single test case dictionary as argument,
# and generates a list of test cases containing additional information.
# They are all meant to be used with the mapchain() function to generate
# lists of test cases.

def enum_one_var(tc, var):
    """Call enumeration function for `var` if necessary

    An enumeration function takes two arguments: a dictionary and the variable
    name.  It must return an updated list of dictionaries containing the
    possible values for the variable.

    >>> retlist = lambda tc, var: [updatedict(tc, {var:[1,2,3], 'c':['hi']})]
    >>> tc = {'a':retlist, 'b':[100]}
    >>> tc = enum_one_var(tc, 'a')
    >>> list(tc) == [{'a':[1,2,3], 'b':[100], 'c':['hi']}]
    True
    """
    if var not in tc:
        raise VariableNotSet(var)
    func = tc[var]
    if callable(func):
        return func(tc, var)
    else:
        return [tc]

def enum_vars(tc, vars):
    """Call enum_one_var() for all variables in `vars`"""
    tcs = [tc]
    for var in vars:
        tcs = mapchain(enum_one_var, tcs, var)
    return tcs

def split_var(tc, var):
    """Split all values for `var` in separate dictionaries

    >>> tc = {'a':[1,2,3], 'b':[100, 200]}
    >>> list(split_var(tc, 'a'))
    [{'a': [1], 'b': [100, 200]}, {'a': [2], 'b': [100, 200]}, {'a': [3], 'b': [100, 200]}]
    """
    return (newdict(tc, var, [v]) for v in tc[var])

def split_vars(tc, vars):
    """Call split_vars() for all variables in `vars`

    >>> list1 = [1,2]
    >>> list2 = [10, 20]
    >>> tc = {'a':list1, 'b':list2, 'c':[100, 200, 300]}
    >>> tc = split_vars(tc, ['a', 'b'])
    >>> list(tc) == [{'a':[i], 'b':[j], 'c':[100, 200, 300]} for i in [1,2] for j in [10, 20]]
    True
    """
    tcs = [tc]
    for var in vars:
        tcs = mapchain(split_var, tcs, var)
    return tcs

def remove_unset_vars(tc):
    """Remove variables that were not enumerated from test case dict

    >>> d = {'a': lambda tc: [1], 'b':[1, 2, 3]}
    >>> remove_unset_vars(d)
    [{'b': [1, 2, 3]}]
    """
    return [dict( (k,v) for k,v in tc.items() if not callable(v) )]

def simple_enum(values):
    """Generate a simple enumeration function that return a list of values

    `values` can be a list or a callable that will return the list.

    >>> fn = simple_enum(lambda: [1,2,3])
    >>> tc = {'a':fn}
    >>> tc = list(enum_one_var(tc, 'a'))
    >>> len(tc)
    1
    >>> tc[0]['a']
    [1, 2, 3]

    >>> fn = simple_enum([1,2,3])
    >>> tc = {'a':fn}
    >>> tc = list(enum_one_var(tc, 'a'))
    >>> len(tc)
    1
    >>> tc[0]['a']
    [1, 2, 3]
    """
    if callable(values):
        valuefn = values
    else:
        valuefn = lambda: values

    def enum_func(tc, var):
        return [newdict(tc, var, valuefn())]

    return enum_func

def require_vars(*args):
    """Decorator that will first split list of test cases for single values in `vars`

    When a given enumeration function needs a variable to be set first,
    it can be wrapped using `require_vars` so it will be called
    with test cases that have all variables enumerated and set to a single
    value first.

    The wrapped function will be called as:
      fn(tc, var, varvalues)

    >>> fn = lambda tc, var, values: [{'x':values['a']+values['b']}]
    >>> fn = require_vars('a', 'b')(fn)
    >>> tc = {'x':fn, 'a':['a', 'b'], 'b':['x', 'y']}
    >>> [tc['x'] for tc in enum_one_var(tc, 'x')]
    ['ax', 'ay', 'bx', 'by']
    """
    vars = args

    def wrap(fn):
        @wraps(fn)
        def wrapper(tc, var):
            tcs = enum_vars(tc, vars)
            tcs = mapchain(split_vars, tcs, vars)

            def call_fn(tc, var):
                assert all(len(tc[v]) == 1 for v in vars)
                varvalues = dict( (v, tc[v][0]) for v in vars )
                return fn(tc, var, varvalues)
            return mapchain(call_fn, tcs, var)
        return wrapper
    return wrap

def var_enum(values):
    """Generate enumeration function that expand variables in `values`

    `values` can be a list, or a callbable that returns the list of values.

    >>> tc = {'a':var_enum(['$b']), 'b':[10]}
    >>> tcs = list(enum_one_var(tc, 'a'))
    >>> len(tcs)
    1
    >>> tcs[0]['a']
    ['10']

    >>> tc = {'a':var_enum(['$b-1', '$b-2']), 'b':[10, 20]}
    >>> tcs = enum_one_var(tc, 'a')
    >>> tcs = mapchain(split_var, tcs, 'a')
    >>> [tc['a'][0] for tc in tcs]
    ['10-1', '20-1', '10-2', '20-2']
    """
    valuefn = simple_enum(values)

    def enum_var(tc, var):
        """The actual enumeration function for the variable"""
        # to enumerate possible values, we first call the original enum function,
        # and split it in single-value test-cases:
        tcs = valuefn(tc, var)
        tcs = mapchain(split_var, tcs, var)

        def expand_one_value(tc):
            """Expand a single value for the variable"""
            assert len(tc[var]) == 1
            value = tc[var][0]
            vars = vars_for_template(value)

            @require_vars(*vars)
            def expand_one_case(tc, var, varvalues):
                expandedvalue = apply_template(value, varvalues)
                return [newdict(tc, var, [expandedvalue])]
            return expand_one_case(tc, var)
        return mapchain(expand_one_value, tcs)

    return enum_var

class VarExpansionTest(unittest.TestCase):
    def testComplexExpansion(self):
        machines = ['pc', 'q35', 'none']
        qemus = ['qemu-system-x86_64', 'qemu-system-i386']
        machine_opt = ['', '-machine $MACHINE']
        accels = ['kvm', 'tcg']

        vars = {'QEMU': var_enum(qemus), 'MACHINE_OPT': var_enum(machine_opt),
                'MACHINE':var_enum(machines), 'ACCEL':var_enum(accels)}
        tcs = enum_one_var(vars, 'QEMU')
        tcs = mapchain(split_var, tcs, 'QEMU')
        tcs = list(tcs)
        self.assertEquals(len(tcs), 2)
        self.assertEquals(tcs[0]['QEMU'], [qemus[0]])
        self.assertEquals(tcs[1]['QEMU'], [qemus[1]])
        self.assertFalse(isinstance(tcs[0]['MACHINE'], list))
        self.assertFalse(isinstance(tcs[0]['MACHINE_OPT'], list))

        vars = {'QEMU': var_enum(qemus), 'MACHINE_OPT': var_enum(machine_opt),
                'MACHINE':var_enum(machines), 'ACCEL':var_enum(accels),
                'command-line':var_enum(['$QEMU $MACHINE_OPT'])}
        expansions = enum_one_var(vars, 'command-line')
        expansions = mapchain(remove_unset_vars, expansions)
        expansions = mapchain(split_var, expansions, 'command-line')
        expansions = list(expansions)

        self.assertEquals(len(expansions), 8)
        self.assertTrue({'command-line':['qemu-system-x86_64 '], 'MACHINE_OPT':[''], 'QEMU':['qemu-system-x86_64']} in expansions)
        self.assertTrue({'command-line':['qemu-system-x86_64 -machine none'], 'MACHINE_OPT':['-machine none'], 'MACHINE':['none'], 'QEMU':['qemu-system-x86_64']} in expansions)
        self.assertTrue({'command-line':['qemu-system-x86_64 -machine pc'], 'MACHINE_OPT':['-machine pc'], 'MACHINE':['pc'], 'QEMU':['qemu-system-x86_64']} in expansions)
        self.assertTrue({'command-line':['qemu-system-x86_64 -machine q35'], 'MACHINE_OPT':['-machine q35'], 'MACHINE':['q35'], 'QEMU':['qemu-system-x86_64']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 '], 'MACHINE_OPT':[''], 'QEMU':['qemu-system-i386']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 -machine none'], 'MACHINE_OPT':['-machine none'], 'MACHINE':['none'], 'QEMU':['qemu-system-i386']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 -machine pc'], 'MACHINE_OPT':['-machine pc'], 'MACHINE':['pc'], 'QEMU':['qemu-system-i386']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 -machine q35'], 'MACHINE_OPT':['-machine q35'], 'MACHINE':['q35'], 'QEMU':['qemu-system-i386']} in expansions)

        vars = {'QEMU': var_enum(qemus), 'MACHINE_OPT': var_enum(machine_opt), 'MACHINE':['none'],
                'command-line':var_enum(['$QEMU $MACHINE_OPT'])}
        expansions = enum_one_var(vars, 'command-line')
        expansions = mapchain(remove_unset_vars, expansions)
        expansions = mapchain(split_var, expansions, 'command-line')
        expansions = list(expansions)

        self.assertEquals(len(expansions), 4)
        self.assertTrue({'command-line':['qemu-system-x86_64 '], 'MACHINE_OPT':[''], 'QEMU':['qemu-system-x86_64'], 'MACHINE':['none']} in expansions)
        self.assertTrue({'command-line':['qemu-system-x86_64 -machine none'], 'MACHINE_OPT':['-machine none'], 'MACHINE':['none'], 'QEMU':['qemu-system-x86_64']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 '], 'MACHINE_OPT':[''], 'QEMU':['qemu-system-i386'], 'MACHINE':['none']} in expansions)
        self.assertTrue({'command-line':['qemu-system-i386 -machine none'], 'MACHINE_OPT':['-machine none'], 'MACHINE':['none'], 'QEMU':['qemu-system-i386']} in expansions)

# enumeration functions for built-in vars:

@require_vars('QEMU')
def get_MACHINE(tc, var, values):
    """Machine-type name.  Don't use with $MACHINE_OPT"""
    return [newdict(tc, var, get_binary_info(values['QEMU']).machines)]

@require_vars('QEMU')
def get_ACCEL(tc, var, values):
    return [newdict(tc, var, get_binary_info(values['QEMU']).available_accels())]

@require_vars('QEMU')
def get_DEVICE(tc, var, values):
    return [newdict(tc, var, get_binary_info(values['QEMU']).user_devs)]

@require_vars('QEMU')
def get_CPU(tc, var, values):
    return [newdict(tc, var, get_binary_info(values['QEMU']).cpu_models)]

BUILTIN_VARS = {'MACHINE': get_MACHINE,
                'ACCEL': get_ACCEL,
                'DEVICE': get_DEVICE,
                'CPU': get_CPU }

class TestCaseEnumerator(object):
    """Helper class that will enumerate possible values for variables"""
    def __init__(self):
        # start with built-in variables:
        self.values = BUILTIN_VARS.copy()

    def set_values(self, var, values):
        """Override values for variable `var`

        `values` must be a list of values
        """
        assert isinstance(values, list)
        self.values[var] = var_enum(values)

    def update_values(self, valuedict):
        for k,v in valuedict.items():
            self.set_values(k, v)

    def enumerate_cases(self, vars):
        """Generate individual items for all combinations of values for variables"""
        dbg("values before enum: %s", pprint.pformat(self.values))
        # enumerate the values for each variable, first
        cases = enum_vars(self.values, vars)
        dbg("cases after enum: %s", pprint.pformat(cases))
        # split the list into individual items with single values for all vars
        cases = mapchain(split_vars, cases, vars)
        dbg("cases after split: %s", pprint.pformat(cases))
        # create simple dictionaries for the specified variables
        cases = (dict((var,tc[var][0]) for var in vars) for tc in cases)
        dbg("cases after removing unset vars: %s", pprint.pformat(cases))
        return cases

class TestSpecification(object):
    def __init__(self, data):
        self._data = data
        self.normalize()

    def normalize(self):
        """Normalize test specification data

        * ensure 'command-line' is a list of arguments
        * 'monitor-commands' will be an array
        * values in 'defaults' and 'full' are lists
        """
        # if command-line is omitted, just run QEMU with no arguments:
        self._data.setdefault('command-line', ['$QEMU'])
        self._data.setdefault('monitor-commands', [])

        # 'monitor-commands' must be a list
        if not isinstance(self.get('monitor-commands'), list):
            self._data['monitor-commands'] = [self.get('monitor-commands')]

        # this:
        #   defaults:
        #     FOO: 'value'
        # becomes:
        #   defaults:
        #     FOO:
        #     - 'value'
        self._data.setdefault('defaults', {})
        defaults = self.get('defaults')
        for k in defaults:
            if not isinstance(defaults[k], list):
                defaults[k] = [defaults[k]]

        self._data.setdefault('full', {})
        full = self.get('full')
        for k in full:
            if not isinstance(full[k], list):
                full[k] = [defaults[k]]

    @classmethod
    def load_file(cls, file):
        data = yaml.load(open(file))
        return cls(data)

    def get(self, key, default=None):
        return self._data.get(key, default)

    def gen_test_cases(self, env):
        """Generate all test cases for this test specification"""
        vars = vars_for_template(self.get('command-line')) + vars_for_template(self.get('monitor-commands'))

        tc_enum = TestCaseEnumerator()

        defaults = self.get('defaults', {})
        if env.args.full:
            for k,v in self.get('full', {}).items():
                # 'full' includes 'defaults' implicitly
                v = defaults.get(k, []) + v
                tc_enum.set_values(k, v)
        else:
            for k,v in defaults.items():
                tc_enum.set_values(k, v)

        tc_enum.update_values(env.var_values)

        cases = tc_enum.enumerate_cases(vars)
        return (TestCase(self, c) for c in cases)

class TestCase(object):
    def __init__(self, spec, values):
        self.spec = spec
        self.values = values

    def __str__(self):
        return ' '.join('%s=%s' % (k, shquote(v)) for k,v in self.values.items())

    def is_expected_entry(self, expected_entry):
        """Check if `expected_entry` matches the testcase/results"""
        expected_vars = expected_entry.copy()
        for var,value in expected_vars.items():
            if self.values.get(var) != value:
                return False
        return True

    def is_expected_failure(self):
        for e in self.getField('expected-failures', []):
            if self.is_expected_entry(e):
                return True

    def getField(self, var, default=None):
        """Get value of test spec field, expanding variables"""
        return apply_template(self.spec.get(var, default), self.values)

    def qmp_cmd(self, vm, cmd):
        if isinstance(cmd, list):
            for c in cmd:
                self.qmp_cmd(vm, c)
        elif isinstance(cmd, dict):
            return vm.qmp_obj(cmd)
        else:
            raise InvalidSpecification("QMP command must be dict: %r" % (cmd))

    def hmp_cmd(self, vm, cmd):
        return vm.command('human-monitor-command', command_line=cmd)

    def monitor_cmd(self, vm, cmd):
        dbg("monitor cmd: %r", cmd)
        if isinstance(cmd, dict):
            for k,v in cmd.items():
                if k == 'qmp':
                    self.qmp_cmd(vm, v)
                elif k == 'hmp':
                    self.hmp_cmd(vm, v)
                else:
                    raise InvalidSpecification("Invalid monitor command: %r: %r" % (k, v))

    def command_line(self):
        """QEMU command-line used for test case"""
        cmdline = self.getField('command-line')
        if not isinstance(cmdline, list):
            cmdline = shlex.split(cmdline)
        return cmdline

    def run(self, env):
        """Check one specific test case

        Returns a dictionary containing failure information on error,
        or None on success
        """
        result = {'success': True }
        result['is-expected-failure'] = self.is_expected_failure()

        cmdline = self.command_line()
        qmp = self.getField('qmp', True)
        #TODO: use context manager to enter/exit borrowed VM from env
        vm = env.get_vm(cmdline, qmp)
        try:
            if not vm.is_launched():
                vm.launch()
            #TODO: generate/enumerate variables inside monitor commands too
            for cmd in self.getField('monitor-commands', []):
                self.monitor_cmd(vm, cmd)
            if not qmp:
                vm.wait()
                env.drop_vm()
        except KeyboardInterrupt:
            raise
        except QMPError as err:
            result['exception'] = repr(err)
            result['success'] = False
        except socket.error as err:
            result['exception'] = repr(err)
            result['success'] = False

        dbg('vm is %r', vm)
        ec = vm.exitcode()
        dbg("exit code: %r", ec)
        if ec is not None and ec != 0:
            result['success'] = False
        result['exitcode'] = ec
        result['log'] = vm.get_log()

        #TODO: use context manager to enter/exit borrowed VM from env
        if not result['success']:
            env.drop_vm()

        return result


class TestEnv(object):
    def __init__(self, args):
        self.args = args
        self._last_vm_args = None
        self._last_vm = None

    def qemu_binaries(self):
        return self.args.qemu_binaries

    def drop_vm(self):
        """Drop existing VM object"""
        if self._last_vm:
            #TODO: record failures here
            self._last_vm.shutdown()
            self._last_vm = None
            self._last_vm_args = None

    def get_vm(self, cmdline, qmp):
        """Get VM object for test case"""
        if self._last_vm_args == (cmdline, qmp) and self._last_vm.is_running():
            dbg("Reusing VM object for cmdline %r", cmdline)
            return self._last_vm

        dbg("Starting new VM for cmdline %r", cmdline)
        #FIXME: need to catch exitcode/segfaults here somehow  :(
        self.drop_vm()
        vm = QEMUMachine(binary=cmdline[0], args=cmdline[1:], qmp=qmp)
        self._last_vm = vm
        self._last_vm_args = (cmdline, qmp)
        return vm

def main():
    parser = argparse.ArgumentParser(description="Generic QEMU validator")
    parser.set_defaults(loglevel=logging.INFO)
    parser.add_argument('-V', metavar='VAR=VALUE', nargs='*',
                        help="Force variabie VAR to VALUE",
                        action='append', dest='vars', default=[])
    parser.add_argument('-d', '--debug',action='store_const',
                        dest='loglevel', const=logging.DEBUG,
                        help='debug output')
    parser.add_argument('-v', '--verbose',action='store_const',
                        dest='loglevel', const=logging.INFO,
                        help='verbose output')
    parser.add_argument('-q', '--quiet',action='store_const',
                        dest='loglevel', const=logging.WARN,
                        help='non-verbose output')
    parser.add_argument("--dry-run", action="store_true",
                        help="Don't run test cases")
    parser.add_argument("--full", action="store_true",
                        help="Run all test case combinations, not just the default for the test specification")
    parser.add_argument("--self-test", action="store_true",
                        help="Run script unit tests and doc tests")
    parser.add_argument("testfiles", nargs="*", metavar="FILE",
                        help="Load test case specification from FILE")
    args = parser.parse_args()

    logging.basicConfig(stream=sys.stdout, level=args.loglevel, format='%(levelname)s: %(message)s')

    if args.self_test:
        verbose = (args.loglevel <= logging.INFO)
        failure_count,test_count = doctest.testmod(verbose=verbose)
        if failure_count > 0:
            return 1
        unittest.main(argv=sys.argv[:1])
        return 0

    if not args.testfiles:
        parser.error("No test case specification provided")

    env = TestEnv(args)

    vars = {}
    if args.vars:
        for varval in itertools.chain(*args.vars):
            var,val = varval.split('=', 1)
            vars.setdefault(var, []).append(val)
    env.var_values = vars

    resultdict = {}
    try:
        for testfile in args.testfiles:
            specname = os.path.basename(testfile)
            #TODO: support test specifications pointing to Python modules
            spec = TestSpecification.load_file(testfile)
            logger.debug("Test specification:")
            logger.debug(pprint.pformat(spec._data))
            logger.debug('---')
            for tc in spec.gen_test_cases(env):
                if tc.is_expected_failure():
                    logger.info("%s: Skipped: %s", specname, str(tc))
                    continue
                logger.info("%s: Running: %s", specname, str(tc))
                logger.debug("%s: Command-line: %s", specname, cmdquote(tc.command_line()))
                if not args.dry_run:
                    r = tc.run(env)
                    logger.debug("Result:")
                    logger.debug(pprint.pformat(r))
                    if not r['success']:
                        logger.error("%s: failed: %s", specname, tc)
                    resultdict.setdefault(r['success'], []).append( (tc, r) )
    except KeyboardInterrupt:
        # Print partial test result summary on interrupt
        logger.info("Interrupted. Partial test summary follows")
        raise
        pass

    env.drop_vm()

    if not args.dry_run:
        logger.info('%d successes', len(resultdict.get(True, [])))
        failures = resultdict.get(False, [])
        if failures:
            logger.error('%d failures', len(failures))
            for tc,r in failures:
                logger.error("Failed: %s", tc)
                logger.error("Result:")
                pprint.pprint(r)
                dbg("Result: %r", r)

if __name__ == '__main__':
    sys.exit(main())
