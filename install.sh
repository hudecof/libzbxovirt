#!/bin/bash

ZABBIX_DIR_ETC=/etc/zabbix

# create directories and copy files
mkdir -p ${ZABBIX_DIR_ETC}/scripts
mkdir -p ${ZABBIX_DIR_ETC}/zabbix_agentd.d
cp ovirt.conf ${ZABBIX_DIR_ETC}/zabbix_agentd.d
cp python/ovirt.py ${ZABBIX_DIR_ETC}/scripts
cp python/ovirt.sh ${ZABBIX_DIR_ETC}/scripts

if [ ! -f ${ZABBIX_DIR_ETC}/libzbxovirt.conf ]; then
	cp libzbxovirt.conf.sample ${ZABBIX_DIR_ETC}/libzbxovirt.conf
fi

# create virtualenv
virtualenv ${ZABBIX_DIR_ETC}/ovirt
. ${ZABBIX_DIR_ETC}/ovirt/bin/activate
pip install -U pip setuptools
pip install urllib3 requests libconf
deactivate
