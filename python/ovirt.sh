#!/bin/bash

[ -f /opt/rh/rh-python35/enable ] && . /opt/rh/rh-python35/enable
/etc/zabbix/ovirt/bin/python /etc/zabbix/scripts/ovirt.py $@

RESULT=$?
exit $RESULT
