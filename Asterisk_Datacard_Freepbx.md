UBUNTU 10.04 LTS Server + Asterisk+FreePBX+GSM HUAWAY
##### 1. Install of Operational System #####

Do typical setup of OS Ubuntu 10.04 LTS Server with these components:
```
LAMP Server
OpenSSH Server
Samba file Server
```

Localization: english

Passwords everywhere: YourPassWord

After setup done, should activate root account, because remaining done with full rights
`sudo passwd root`
##### 2. Installing components from network #####

Download and setup from repositories ~44,4 MB + +~12 of updates
```
apt-get update
apt-get install php5-mysql libapache2-mod-php5 mysql-server libmysqlclient15-dev php-db php5-gd php-pear sox curl g++ libncurses-dev libxml2-dev subversion
```

Setup DAHDI drivers (optional if conference support not required or no appropriate hardware
```
apt-get install dahdi
/etc/init.d/dahdi start
cd /tmp
wget http://downloads.asterisk.org/pub/telephony/dahdi-linux-complete/releases/dahdi-linux-complete-2.4.0+2.4.0.tar.gz
cd /usr/src
tar -zxvf /tmp/dahdi-linux-complete-2.4.0+2.4.0.tar.gz
cd dahdi-linux-complete-2.4.0+2.4.0/
make
make install
make config
```

Download Asterisk and FreePBX sources from vendors. Size around 24MB + 0.9MB + 6.2MB

```
cd /tmp
wget http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-1.6.2.13.tar.gz
wget http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-addons-1.6.2.2.tar.gz
wget http://mirror.freepbx.org/freepbx-2.8.0.tar.gz
```

Unpack packed sources:
```
cd /usr/src
tar xvfz /tmp/asterisk-1.6.2.13.tar.gz
tar xvfz /tmp/asterisk-addons-1.6.2.2.tar.gz
tar xvfz /tmp/freepbx-2.8.0.tar.gz
```

##### 3. Setup Asterisk #####
```
cd /usr/src/asterisk-1.6.2.13
./configure
```

When installing voice packages automatically downloaded. Check what has internet connection at that time
```
make install
make config
make samples
```
##### 4. Installing Asterisk Addons #####
Addons required for save CDRs to mysql in format required by FreePBX
```
cd /usr/src/asterisk-addons-1.6.2.2
perl -p -i.bak -e 's/CFLAGS.*D_GNU_SOURCE/CFLAGS+=-D_GNU_SOURCE\nCFLAGS+=-DMYSQL_LOGUNIQUEID/' Makefile
./configure
make
make install
```

##### 5. Configure Apache2 #####


Adding user and get rights
```
adduser asterisk --disabled-password --gecos "Asterisk PBX"
adduser www-data asterisk
```

Force apache2 started under asterisk permissions
```
nano /etc/apache2/envvars
```


commenting next lines
```
  #export APACHE_RUN_USER=www-data
  #export APACHE_RUN_GROUP=www-data
```

adding next lines
```
export APACHE_RUN_USER=asterisk
export APACHE_RUN_GROUP=asterisk
```

save changes with Ctrl+O and exit with Ctrl+X


Restarting apache2
```
apache2ctl graceful
```
##### 6. Installing FreePBX #####

#### Prepare Mysql databases for FreePBX ####
```
cd /usr/src/freepbx-2.8.0
mysqladmin create asterisk -pYourPassWord
mysqladmin create asteriskcdrdb -pYourPassWord
mysql asterisk < SQL/newinstall.sql -pYourPassWord
mysql asteriskcdrdb < SQL/cdr_mysql_table.sql -pYourPassWord
mysql -pYourPassWord
GRANT ALL PRIVILEGES ON asterisk.* TO asteriskuser@localhost IDENTIFIED BY 'YourPassWord';
GRANT ALL PRIVILEGES ON asteriskcdrdb.* TO asteriskuser@localhost IDENTIFIED BY 'YourPassWord';
flush privileges;
quit;
```

#### Installing FreePBX ####

Run asterisk before FreeBPX install
```
/etc/init.d/asterisk start
```

Run installer
```
./install_amp
```

IMPORTANT do not change other setting !!!
Setting should changes in setup wizard:
```
  Enter your PASSWORD to connect to the 'asterisk' database:
  [amp109] YourPassWord

  Enter a PASSWORD to connect to the Asterisk Manager interface:
  [amp111] YourPassWord

  Enter the path to use for your AMP web root:
  [/var/www/html]
  /var/www/freepbx

  Enter a PASSWORD to perform call transfers with the Flash Operator Panel:
  [passw0rd] YourPassWord
```

#### Tune FreePBX settings after installing for right work ####

nano /etc/amportal.conf

Comment AMPWEBADDRESS for working FOP (flash panel)
#AMPWEBADDRESS=xx.xx.xx.xx


Change value of AUTHTYPE for secured access for administratove pannel and store accounts in database (adminadmin by default):

AUTHTYPE=database

Change value of password for administrative access to call record panel.

ARI\_ADMIN\_PASSWORD=YourPassWord


save changes with Ctrl+O and exit with Ctrl+X

#### Fix for enable russian language in web panel of FreePBX ####

nano /usr/share/locale/locale.alias

Remove line with encoding for russian and adding 3 lines instead
```
russian         ru
ru              ru_RU
ru_RU           ru_RU.UTF-8
```

save changes with Ctrl+O and exit with Ctrl+X

Enable auto start at boot
`nano /etc/rc.local`

adding lines before exit 0

/usr/local/sbin/amportal start

Reboot server for auto start check
`reboot`

#### Updating and installing FreeBPX modules from vendor site ####

Enter to web panel

http://192.168.0.218/freepbx/admin (admin/admin)

At first apply settings

"Управление модулями" -> "Проверить обновление on-line" -> "Скачать все" -> "Обновить все" -> "Запустить процесс"

Move down and press 'Confirm'

Should restart and continue process in case if stopped until all is done

##### 7. Attaching Huawei datacard to Asterisk #####

#### Preparation ####

Firmware Updates: http://www.dc-files.com/files/huawei/

Mandatory: turn off all optional devices like CD-ROM, Flash reader, network interface
With terminal type AT^U2DIAG=0 and replug modem

#### Installing of chan\_datacard ####

Downloading and installing of chan\_datacard channel module
```
apt-get install automake
svn co https://datacard.googlecode.com/svn/trunk/ /usr/src/datacard
cd /usr/src/datacard
automake
./configure
make install
cp etc/datacard.conf /etc/asterisk
```

#### chan\_datacard settings ####

`nano /etc/asterisk/datacard.conf`
at bottom of file remove all data and write
```
[000101]
context=from-gsm
audio=/dev/ttyUSB1
data=/dev/ttyUSB2
group=1
rxgain=2
txgain=-5
resetdatacard=yes
autodeletesms=yes
usecallingpres=yes
callingpres=allowed_passed_screen
```

save changes with Ctrl+O and exit with Ctrl+X

Create context for incoming calls from datacard
`nano /etc/asterisk/extensions_custom.conf`
```
[from-gsm]
exten => s,1,Set(CALLERID(all)=${CALLERID(num)})
exten => s,n,Set(CALLERID(num)=8${CALLERID(num):2})
exten => s,n,goto(from-trunk,${IMEI},1)
```

restarting Asterisk
`service asterisk restart`

#### Check state & Control of datacard ####


Checking devices state
```
asterisk -r
datacard show devices
```

Turn off and on power for modem

Определяем порт:
`dmesg usb | grep ttyUSB`
check last line

> [5191.602917](.md) usb 1-3.5: GSM modem (1-port) converter now attached to ttyUSB3

Turn off and on power for usb device 1-3.5
```
  echo suspend > /sys/bus/usb/devices/1-3.5/power/level
  echo on > /sys/bus/usb/devices/1-3.5/power/level
```

Send to modem reboot command
`datacard cmd 000101 AT+CFUN=1,1`


#### Settings of FreePBX for datacards ####

  * Create exten (number of client phone)

Конфигурация => Внутренние номера => Добавить SIP устройство (указываем имя, номер, пароль)

  * Create special trunk

Конфигурация => Транки => Добавить специальный транк => Исходящие настройки Специальный набор => datacard/i:123456789012345/$OUTNUM$
where 123456789012345 (IMEI of datacard)

  * Create outgoing route

Конфигурация => Исходящая маршрутизация => math patern => . (ставим точку в это поле), а в Trunk Sequence for Matched Routes выбираем наш транк.

  * Create incoming route

Конфигурация => Входящая маршрутизация => Добавить входящий маршрут => Номер DID => 123456789012345 (IMEI модема),
а в "Установить направление" выбрать получателя звонков, ваш экстент.