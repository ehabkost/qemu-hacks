#!/usr/bin/env python
#
# query-cpu-model-* validation and sanity checks
#
#  Copyright (c) 2016-2019 Red Hat Inc
#
# Author:
#  Eduardo Habkost <ehabkost@redhat.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#

import sys, os
import unittest
import logging
import copy
import avocado_qemu

class CPUModelExpansion(avocado_qemu.Test):
    longMessage = True
    maxDiff = None

    def migration_unsafe_features(self):
        """Return list of features that are not migration-safe"""
        if self.arch in ['x86_64', 'i386']:
            return ['pmu', 'host-cache-info']
        else:
            return []

    def is_static(self, model):
        """Check if CpuModelInfo represents a static CPU model expansion

        A static CPU model expansion must contain a stati CPU model name
        and only migration-safe properties.
        """
        name = model['name']
        if not self.cpu_models[name]['static']:
            return False
        for p in model.get('props', {}).keys():
            if p in self.migration_unsafe_features():
                return False
        return True

    def check_one_expansion(self, model, type, msg):
        """Perform one query-cpu-model-expansion operation, validate results

        @model is a CpuModelInfo struct.
        Returns a new CpuModelInfo struct, with expanded CPU model data.
        """
        logging.info("%s: testing type=%s", msg, type)
        logging.debug("%s: input: %r", msg, model)

        expanded = self.vm.command('query-cpu-model-expansion',
                                   type=type, model=model)['model']

        logging.debug("%s: expanded: %r", msg, expanded)

        # validate expansion results:
        if type == 'static':
            self.assertTrue(self.is_static(expanded), '%s: expansion is not static' % (msg))

        return expanded

    def check_expansions(self, model, msg):
        """Perform multiple expansion operations on model, validate results

        @model is a CpuModelInfo struct
        """
        s = self.check_one_expansion(model, 'static', '%s.static' % (msg))
        f = self.check_one_expansion(model, 'full', '%s.full' % (msg))
        ss = self.check_one_expansion(s, 'static', '%s.static.static' % (msg))
        sf = self.check_one_expansion(s, 'full', '%s.static.full' % (msg))
        ff = self.check_one_expansion(f, 'full', '%s.full.full' % (msg))

        # static expansion twice should result in the same data:
        self.assertEquals(s, ss, '%s: static != static+static' % (msg))
        # full expansion twice should also result in the same data:
        self.assertEquals(f, ff, '%s: full != full+full' % (msg))

        # CPU models that are already static should not be affected by
        # additional static expansion:
        if self.is_static(model):
            self.assertEquals(sf, f, '%s: static+full != full' % (msg))

    def check_one_model(self, m):
        """Run multiple query-cpu-model-expansion checks

        * Test simple CPU model name
        * Test CPU model with unsafe features explicitly disabled
          if it's not migration-safe
        * Test CPU model with unsafe features enabled
        * Test CPU model with unavailable features disabled,
          if unavailable-features is set

        @m is a CpuDefinitionInfo struct from query-cpu-definitions
        """
        msg = '%s.%s' % (self.accel, m['name'])
        logging.info("%s: check_one_model", msg)

        # simple expansion of CPU model name:
        model = { 'name': m['name'] }
        self.check_expansions(model, msg)

        # check if we do the right thing when one unsafe feature is explicitly
        # enabled:
        for f in self.migration_unsafe_features():
            unsafe_model = {
                'name':  m['name'],
                'props': { f: True },
            }
            self.check_expansions(unsafe_model, msg + ".unsafe." + f)

        # check if we do the right thing when all unsafe features are
        # explicitly disabled:
        safe_model = {
            'name':  m['name'],
            'props': { f: False for f in self.migration_unsafe_features() },
        }
        self.check_expansions(safe_model, msg + ".safe")

    def check_all_models(self):
        # use <accel>:tcg so QEMU won't refuse to start if KVM is unavailable
        self.vm.add_args('-S', '-machine', 'accel=%s:tcg' % (self.accel))
        self.vm.launch()

        if self.accel == 'kvm':
            if not self.vm.command('query-kvm')['enabled']:
                self.skipTest("Failed to enable KVM")

        models = self.vm.command('query-cpu-definitions')
        self.cpu_models = dict((m['name'], m) for m in models)

        for m in models:
            # some validations on query-cpu-definitions output:
            if m.get('static'):
                self.assertTrue(m['migration-safe'])

            self.check_one_model(m)

    def testTCGModels(self):
        self.accel = 'tcg'
        self.check_all_models()

    def testKVMModels(self):
        self.accel = 'kvm'
        self.check_all_models()
