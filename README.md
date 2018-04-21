# libzbxovirt

## Overview
Zabbix have native support to monitor **VmWare** infrastructure. The goal is to add support for **oVirt/RHEV** into zabbix too.

This is at stage of **proof-of-concept**, any contributing is welcome.

## Requirements

### Zabbix sources

To compile native zabbix module you need zabbix source. You could get them from `https://www.zabbix.com/download_sources` or `https://sourceforge.net/projects/zabbix/files/ZABBIX%20Latest%20Stable/` page. I'm testing against version **3.4.7**, but any **3.4.** is good.

Follow the **instruction** on `https://www.zabbix.com/documentation/3.4/manual/config/items/loadablemodules`, part **2.5  Building modules, run `./configure`.


### Zabbix version
You need zabbix version at least **3.4** to get it all work. In this release was introduces **Dependent items**, see `https://www.zabbix.com/documentation/3.4/manual/config/items/itemtypes/dependent_items`

### Python
Discovery rules are written in python for simplicity. I'm using **python 2**, but maybe also **python 3** would work.

```
virtualenv ovirt
pip install -U pip setuptools
pip install urllib3
pip install requests
pip install libconf
```
I'm not using **ovirt-engine-sdk-python** library, since it's too big overhead for monitoring purposes.

### C libraries

To compile the module you will need **curl** and **config** libraries

#### Debian 9
```
apt-get install libcurl4-openssl-dev libconfig-dev
```
#### CentOS
```
yum install -y libcurl-devel libconfig-devel
```

## Instalation

### Files, Directories, Python
Review and run `./install.sh`. Check if you have in your **/etc/zabbix/zabbix_agentd.conf** file line

    Include=/etc/zabbix/zabbix_agentd.d
   
### Module 
In **module/Makefile** 

- tweak path to your zabbix sources
- if you set **LoadModulePath** in **/etc/zabbix/zabbix_agentd.conf**, update **MODULEPATH** here

```
cd modules
make
make install
```

### Configuration
Update the **/etc/zabbix/libzbxovirt.conf**

## How it works

It uses oVirt API, see `http://ovirt.github.io/ovirt-engine-api-model/`.

### Data types
There are 2 type of functions

- **data**: it will return full unmodified json output fro the API
- **stats**: it will parse `/statistics` endpoint and return just **key/value** result

On zabbix site, the data are populates on these two type of calls using the **Dependent Item** type. This offloads an engine side from queries.

### Zabbix template

Import zabbix template and associate it with the host running this module. It not needs to be engine itself.

### Object types
There are 3 object** types: **simple**, **vm**, **host**.

The **simple** input is full api endpoint, it looks like `/ovirt-engine/api/vms`.
The **host** input is host uid, it will call `/ovirt-engine/api/hosts/<uid>`.
The **vm** input is vm uid, it will call `/ovirt-engine/api/vms/<uid>`.

## Notes

### Goal
My idea is to build production ready oVirt as next service we provide on VmWare an Hyper-V. The VmWare has a lot of monitoring features and nice DWH reports and native zabbix integration.

### Thoughts
I'm testing on

-  Low powered (1xCPU, 1GB RAM) zabbix server, all in one
-  oVirt instance with about 50 VM and 3x hosts, engine (4xCPU, 6GB RAM)

The zabbix module is the only way how to offload the zabbix agent part. When using python olny implementation with **UserParameter**, the load was to high due the process forking.
With native module I was able to keep load very low.

To offload the engine part, the **Dependent Item** is used. But the load goes very high, about 8-10 ;(

This really not the way how to monitor the oVirts VM. In my case all hosts and VMs are monitored by zabbix agent and this is preferred way how to do that.

### What's next

There are following possibilities.

To get nice DHW graphs try to look at **oVirt Mertics**.

This still could be used to monitor basic oVirt parameters like **DataCenter**, **Cluster**, **Storage**, but it need future work on this.

Instead of the **API** the direct access to the **DWH** database could be used.

Instead of **API** we could try to hook on VDSM API, see **vdsm-client** on `https://www.ovirt.org/develop/developer-guide/vdsm/vdsm-client/`.

Use **zabbix-sender** with any previously mentioned any data source

### TODO
The module is using the **basic http auth**. The engine for each query call internally SSO so there are internal 3 requests. The code should use regular login call and reuse the bearer token. This should offload the engine a little bit.

The module is not reusing http connections, aka keep alive. The **SSL handshake** is done for each query and it's expensive.
