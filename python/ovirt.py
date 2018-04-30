#!/usr/bin/env python
#
# libzbxovirt - A oVirt/RHEV monitoring module for Zabbix
# Copyright (C) 2018 - Peter Hudec <phudec@cnc.sk>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import json
import requests
from requests.auth import HTTPBasicAuth
from requests import Request, Session

try:
    from urllib.parse import urlencode, urlparse
except ImportError:
    from urllib import urlencode
    from urlparse import urlparse

class ApiOvirt:
    def __init__(self, url, username, password):
        self.auth = HTTPBasicAuth(username, password)
        self.url = url

    def __headers(self):
        return { 'Accept': 'application/json' }

    def __get(self, uri, follow=None):
        prefix = urlparse(self.url).path
        if not prefix.endswith('/'):
            prefix += '/'
        if uri.startswith(prefix):
            uri = uri[len(prefix):]
        uri = "%s/%s" % (self.url, uri)
        if follow:
            uri = "%s?follow=%s" % (uri, ','.join(follow))
        r = requests.get(uri, headers=self.__headers(), auth=self.auth, verify=False)
        return r.json()

    # specific service
    def get(self, service, follow=None):
        return self.__get(service, follow=follow)

class ZabbixService:
    DISCOVERY_RULES = {
        'hosts': {
            'base_path': None,
            'service': 'hosts',
            'key': 'host',
            'prefix': 'HOST',
         },
        'vms': {
            'base_path': None,
            'service': 'vms',
            'key': 'vm',
            'prefix': 'VM',
         },
        'datacenters': {
            'base_path': None,
            'service': 'datacenters',
            'key': 'data_center',
            'prefix': 'DC',
         },
        'clusters': {
            'base_path': None,
            'service': 'clusters',
            'key': 'cluster',
            'prefix': 'CLUSTER',
         },
        'host_nics': {
            'base_path': 'hosts',
            'service': 'nics',
            'key': 'host_nic',
            'prefix': 'NIC',
         },
        'vm_nics': {
            'base_path': 'vms',
            'service': 'nics',
            'key': 'nic',
            'prefix': 'NIC',
         },
        'vm_disks': {
            'base_path': 'vms',
            'service': 'diskattachments',
            'key': 'disk_attachment',
            'subkey': 'disk',
            'prefix': 'DISK',
            'follow': ['disk']
         },

    }

    def __init__(self, api):
        self.api = api

    def discovery(self, service, uuid):
        service_map = self.DISCOVERY_RULES.get(service, None)
        if service_map is None:
            return None

        uri = service_map['service']
        if service_map.get('base_path', None) and uuid is not None:
            uri = "%s/%s/%s" % (service_map.get('base_path'), uuid, uri)

        list = self.api.get(uri, follow=service_map.get('follow', None))
        result = {
            'data': []
        }
        for item in list[service_map['key']]:
            if service_map.get('subkey', None):
                item = item[service_map.get('subkey')]
            result_item = dict()
            result_item["{#%s_ID}" % service_map['prefix']] = item['id']
            result_item["{#%s_HREF}" % service_map['prefix']] = item['href']
            result_item["{#%s_NAME}" % service_map['prefix']] = item['name']
            result['data'].append(result_item)
        print json.dumps(result, indent=4)

    def service_get(self, service, uuid, key, follow=None):
        if uuid is not None:
            service = "%s/%s" % (service, uuid)
        data = self.api.get(service, follow=follow)
        result = data
        for path in key.split('.'):
            result = result.get(path, None)
            if result is None:
                break
        return result

    def service_stats(self, service, uuid, follow=None):
        if uuid is not None:
            service = "%s/%s" % (service, uuid)
        service = "%s/statistics" % (service,)
        data = self.api.get(service, follow=follow)
        result = dict()

        for stat in data['statistic']:
            result[stat['name']] = stat['values']['value'][0]['datum']
        return result

    def service_data(self, service, uuid, follow=None):
        if uuid is not None:
            service = "%s/%s" % (service, uuid)
        data = self.api.get(service, follow=follow)
        return data


    def service_show(self, service, follow=None):
        data = self.api.get(service, follow=follow)
        print json.dumps(data, indent=4)

def main(args):
    api = ApiOvirt(
        url="%s/%s" % (config['ovirt']['engine']['server'], config['ovirt']['engine']['uri']),
        username=config['ovirt']['engine']['user'],
        password=config['ovirt']['engine']['pass'])

    zabbix = ZabbixService(api)
    if args.list is not None:
        zabbix.discovery(args.list, args.uuid)
    if args.get is not None:
        result = zabbix.service_get(args.service, args.uuid, args.get, follow=args.follow)
        print '' if result is None else result
    if args.stats:
        result = zabbix.service_stats(args.service, args.uuid, follow=args.follow)
        print '' if result is None else json.dumps(result, indent=2)
    if args.data:
        result = zabbix.service_data(args.service, args.uuid, follow=args.follow)
        print '' if result is None else json.dumps(result, indent=2)

    if args.show:
        zabbix.service_show(args.service, follow=args.follow)

if __name__ == "__main__":
    import urllib3
    urllib3.disable_warnings()

    import argparse
    parser = argparse.ArgumentParser(description = 'oVirt Hosts Wrapper')
    parser.add_argument('--list', dest='list', action='store', help='discovery method')
    parser.add_argument('--get', dest='get', action='store', help='get service value')
    parser.add_argument('--show', dest='show', action='store_true', help='get service value')
    parser.add_argument('--service', dest='service', action='store', help='service uri to list/get')
    parser.add_argument('--uuid', dest='uuid', action='store', help='uuid for the service')
    parser.add_argument('--stats', dest='stats', action='store_true', help='get stats value')
    parser.add_argument('--data', dest='data', action='store_true', help='get data value')
    parser.add_argument('--follow', dest='follow', action='append', help='follow links')
    parser.add_argument('--config', dest='config', action='store', default='/etc/zabbix/libzbxovirt.conf', help='configuration file')

    args = parser.parse_args()

    import io,libconf
    with io.open(args.config) as f:
        config = libconf.load(f)

    main(args)
