CC = gcc
SRC = libzbxovirt.c

TARGET = libzbxovirt.so
MODULEPATH = /usr/lib/zabbix/modules

LDFLAGS = -lcurl -lconfig
CFLAGS = -Wall -O3 -I../../zabbix-3.4.7/include

$(TARGET): $(SRC)
	$(CC) -shared -fPIC -o $@ $(OBJ) $(SRC)  $(LDFLAGS) $(CFLAGS) 
clean:
	rm -f $(TARGET) $(OBJ)

install:$(TARGET)
	service zabbix-agent stop
	install -d $(MODULEPATH)
	install -C $(TARGET) $(MODULEPATH)
	service zabbix-agent start
	service zabbix-agent status
	tail /var/log/zabbix/zabbix_agentd.log
