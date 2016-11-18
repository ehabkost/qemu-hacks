#!/usr/bin/env python
import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'scripts'))
import qtest
import unittest
import logging
import argparse

logger = logging.getLogger('qemu.tests.machineinfo')

# machines that we can't easily test because they can't run on all hosts:
BLACKLIST = ['xenpv', 'xenfv']

# architectures where machines are expected to report all available buses:
STRICT_ARCHES = set(['x86_64', 'i386', 's390x'])

class QueryMachinesTest(qtest.QEMUQtestTestCase):
    def walkQOMTree(self, vm, path):
        """Walk QOM tree recusrively, starting at path"""
        children = vm.qmp('qom-list', path=path)['return']
        for c in children:
            logging.debug('walking %s. child: %s', path, c)
            if not c['type'].startswith('child<'):
                continue

            cp = '%s/%s' % (path, c['name'])
            yield cp

            for gc in self.walkQOMTree(vm, cp):
                yield gc

    def findAllBuses(self, vm):
        """Find all bus objects in the QOM tree"""
        r = vm.qmp('qom-list-types', implements='bus')
        bus_types = set([b['name'] for b in r['return']])
        for cp in self.walkQOMTree(vm, '/machine'):
            t = vm.qmp('qom-get', path=cp, property='type')['return']
            if t in bus_types:
                dt = vm.qmp('qom-get', path=cp, property='device-type')['return']
                yield dict(path=cp, type=t, device_type=dt)

    def validateSupportedDeviceTypes(self, machine):
        """Validate 'supported-device-types' on 'query-machines'"""
        if machine['name'] in BLACKLIST:
            return

        vm = self.get_machine(['-S', '-machine', machine['name']], logging=False)
        vm.launch()
        try:
            buses = list(self.findAllBuses(vm))
            bus_types = set([b['type'] for b in buses])
            device_types = set([b['device_type'] for b in buses])
            logger.debug("buses for machine %s: %s", machine['name'], ' '.join(bus_types))
            logger.debug("device-type for machine %s: %s", machine['name'], ' '.join(device_types))
            reported_types = set(machine['supported-device-types'])
            extra_types = reported_types.difference(device_types)
            missing_types = device_types.difference(reported_types)
            # the machine MUST NOT report any types if the bus is not available
            # by default (in other words, extra_types should be empty)
            self.assertEquals(extra_types, set())
            # missing_types, on the other hand, may be empty. sometimes
            # a bus is available but the machine doesn't report it yet
            if missing_types:
                logger.info("extra device types present on machine %s: %s",
                            machine['name'], ' '.join(missing_types))
                if self.testArch() in STRICT_ARCHES:
                    self.fail("extra device types: %s" (' '.join(missing_types)))
        finally:
            vm.shutdown()

    @classmethod
    def addMachineTest(klass, method, machine):
        """Dynamically add a testMachine_<name>_<machine> method to the class"""
        def testMachine(self):
            return method(self, machine)
        machine_name = machine['name'].replace('-', '_').replace('.', '_')
        method_name = 'testMachine_%s_%s' % (method.__name__, machine_name)
        setattr(klass, method_name, testMachine)

    @classmethod
    def discoverMachines(klass):
        """Run query-machines

        This method is run before test cases are started, so we
        can dynamically add test cases for each machine supported
        by the binary.
        """
        vm = klass.get_machine(['-S', '-machine', 'none'], logging=False)
        vm.launch()
        try:
            machines = vm.qmp('query-machines')['return']
        finally:
            vm.shutdown()
        return machines

    @classmethod
    def addMachineTestCases(klass):
        """Dynamically add test methods for each machine-type"""
        machines = klass.discoverMachines()
        for m in machines:
            klass.addMachineTest(klass.validateSupportedDeviceTypes, m)

def load_tests(loader, tests, pattern):
    QueryMachinesTest.addMachineTestCases()
    ts = unittest.TestSuite()
    tests = loader.loadTestsFromTestCase(QueryMachinesTest)
    ts.addTests(tests)
    return ts

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbosity', help='Set verbosity', default=1)
    parser.add_argument('--quiet', '-q', help='Run tests quietly', action='store_const', dest='verbosity', const=0)
    parser.add_argument('--verbose', '-v', help='Run tests verbosely', action='store_const', dest='verbosity', const=2)
    args = parser.parse_args(argv[1:])
    unittest.main(argv=argv[:1], verbosity=args.verbosity)

if __name__ == '__main__':
    logging.basicConfig(loglevel=logging.DEBUG)
    # ignore extra command-line arguments provided by gtester
    main(sys.argv)
