"""chromaterm.default_config tests"""
import os

import chromaterm.default_config


def assert_highlight(positives, negatives, rule):
    """Assert that all positives are highlighted while negatives are not."""
    def permutate_data(data):
        output = []

        for entry in data if isinstance(data, list) else [data]:
            output.append(entry)  # Plain entry
            output.append('hello ' + entry)  # Start changed
            output.append(entry + ' world')  # End changed
            output.append('hello ' + entry + ' world')  # Start and end changed

        return output

    for data in permutate_data(positives):
        assert repr(rule.highlight(data)) != repr(data)

    for data in permutate_data(negatives):
        assert repr(rule.highlight(data)) == repr(data)


def test_rule_ipv4():
    """Default rule: IPv4 addresses."""
    positives = ['192.168.2.1', '255.255.255.255', '=240.3.2.1', '1.2.3.4/32']
    negatives = ['192.168.2.1.', '1.2.3.4.5', '256.255.255.255', '1.2.3']
    rule = chromaterm.default_config.RULE_IPV4

    assert_highlight(positives, negatives, rule)


def test_rule_ipv6():
    """Default rule: IPv6 addresses."""
    positives = [
        'A:b:3:4:5:6:7:8', 'A::', 'A:b:3:4:5:6:7::', 'A::8', '::b:3:4:5:6:7:8',
        '::8', 'A:b:3:4:5:6::8', 'A:b:3:4:5::7:8', 'A:b:3:4::6:7:8', '::',
        'A:b:3::5:6:7:8', 'A:b::4:5:6:7:8', 'A::3:4:5:6:7:8', 'A::7:8', 'A::8',
        'A:b:3:4:5::8', 'A::6:7:8', 'A:b:3:4::8', 'A::5:6:7:8', 'A:b:3::8',
        'A::4:5:6:7:8', 'A:b::8', 'A:b:3:4:5:6:7:8/64', '::255.255.255.255',
        '::ffff:255.255.255.255', 'fe80::1%tun', '::ffff:0:255.255.255.255',
        '00A:db8:3:4::192.0.2.33', '64:ff9b::192.0.2.33'
    ]
    negatives = [
        ':::', '1:2', '1:2:3', '1:2:3:4', '1:2:3:4:5', '1:2:3:4:5:6:7',
        '1:2:3:4:5:6:7:8:9', '1:2:3:4:5:6:7::8', 'abcd:xyz::1', 'fe80:1%tun'
    ]
    rule = chromaterm.default_config.RULE_IPV6

    assert_highlight(positives, negatives, rule)


def test_rule_mac():
    """Default rule: MAC addresses."""
    positives = ['0A:23:45:67:89:AB', '0a:23:45:67:89:ab', '0a23.4567.89ab']
    negatives = [
        '0A:23:45:67:89', '0A:23:45:67:89:AB:', '0A23.4567.89.AB',
        '0a23.4567.89ab.', '0a:23:45:67:xy:zx', '0a23.4567.xyzx'
    ]
    rule = chromaterm.default_config.RULE_MAC

    assert_highlight(positives, negatives, rule)


def test_rule_date():
    """Default rule: Date."""
    positives = [
        '2019-12-31', '2019-12-31', 'jan 2019', 'feb 2019', 'Mar 2019',
        'apr 2019', 'MAY 2019', 'Jun 2019', 'jul 2019', 'AUG 19', 'sep 20',
        'oct 21', 'nov 22', 'dec 23', '24 jan', '25 feb 2019'
    ]
    negatives = [
        '201-12-31', '2019-13-31', '2019-12-32', 'xyz 2019', 'Jun 201',
        'xyz 26', 'jun 32', '32 jun'
    ]
    rule = chromaterm.default_config.RULE_DATE

    assert_highlight(positives, negatives, rule)


def test_rule_time():
    """Default rule: Time."""
    positives = ['23:59', '23:01', '23:01:01', '23:01:01.123']
    negatives = ['24:59', '23:60', '23:1', '23:01:1', '23:01:01:']
    rule = chromaterm.default_config.RULE_TIME

    assert_highlight(positives, negatives, rule)


def test_write_default_config():
    """Write config file."""
    name = __name__ + '1'
    assert chromaterm.default_config.write_default_config('.', name)
    assert os.access(os.path.join('.', name), os.F_OK)
    os.remove(__name__ + '1')


def test_write_default_config_exists():
    """Config file already exists."""
    name = __name__ + '2'
    open(name, 'w').close()
    assert not chromaterm.default_config.write_default_config('.', name)
    os.remove(name)


def test_write_default_config_no_directory():
    """No directory for default config (e.g. no home)."""
    assert not chromaterm.default_config.write_default_config(None)


def test_write_default_config_no_permission():
    """No write permission on directory."""
    name = __name__ + '3'
    os.mkdir(name, mode=0o444)
    assert not chromaterm.default_config.write_default_config(name)
    os.rmdir(name)
