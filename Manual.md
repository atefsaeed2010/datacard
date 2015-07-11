# Introduction #
Chan\_datacard is an Asterisk channel driver for Huawei UMTS / 3G datacards.

Supported features: Place voice call, terminate voice calls, send/receive SMS and send/receive USSD commands / messages

Example dialplan:
```
[datacard-incoming] 
exten => sms,1,Verbose(Incoming SMS from ${CALLERID(num)} ${SMS}) 
exten => sms,n,System(echo ‘${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} – ${DATACARD} – ${CALLERID(num)}: ${SMS}’ >> /var/log/asterisk/sms.txt) 
exten => sms,n,Hangup()

exten => ussd,1,Verbose(Incoming USSD: ${USSD}) 
exten => ussd,n,System(echo ‘${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} – ${DATACARD}: ${USSD}’ >> /var/log/asterisk/ussd.txt) 
exten => ussd,n,Hangup()

exten => s,1,Dial(SIP/2001@othersipserver) 
exten => s,n,Hangup()

[othersipserver-incoming]

exten => _X.,1,Dial(Datacard/r1/${EXTEN}) 
exten => _X.,n,Hangup
```
You can also use this:

Call using a specific group:
```
exten => _X.,1,Dial(Datacard/g1/${EXTEN})
```
Call using a specific datacard:
```
exten => _X.,1,Dial(Datacard/datacard0/${EXTEN})
```
Call using a specific provider name:
```
exten => _X.,1,Dial(Datacard/p:PROVIDER NAME/${EXTEN})
```
Call using a specific IMEI:
```
exten => _X.,1,Dial(Datacard/i:123456789012345/${EXTEN})
```
Call using a IMSI prefix:
```
exten => _X.,1,Dial(Datacard/s:250976764489/${EXTEN})
```
Predefined Variables:
```
${DATACARD}: Current datacard used. 
${IMSI}: IMSI of current datacard in use. 
${IMEI}: IMEI of current datacard in use. 
${PROVIDER}): PROVIDER of current datacard in use. 
${SMS}: Incoming SMS message. 
${USSD}: Incoming USSD message.
```
# Installation #
Check some dependencies like `automake` and `autoconf`. Also must have asterisk sources and development tools like `make`, `gcc` and so on.
## SVN ##


```
svn checkout http://datacard.googlecode.com/svn/trunk/ datacard-read-only
cd datacard-read-only
autoconf && automake -a
./configure && make && make install
```
## Compressed package ##
Go to http://code.google.com/p/datacard/downloads/list and download .tgz package, then

```
tar -zxvf chan_datacard_VERSION.tgz
./configure && make && make install
```

Once installed, copy example file datacard-read-only/etc/datacard.conf to /etc/asterisk. Configure it as desired and start asterisk. To load, stop or restart chan\_datacar:

```
CLI>module load chan_datacard.so
CLI>module unload chan_datacard.so
```