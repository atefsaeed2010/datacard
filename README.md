
```
--------------------------------------------------
chan_datacard channel driver for Huawei UMTS cards
--------------------------------------------------

WARNING:

This channel driver is in alpha stage.
I am not responsible if this channel driver will eat your money on
your SIM card or do any unpredicted things.

Please use a recent Linux kernel, 2.6.33+ recommended.
If you use FreeBSD, 8.0+ recommended.

This channel driver should work with the folowing UMTS cards:
* Huawei K3715
* Huawei E169 / K3520
* Huawei E155X
* Huawei E175X
* Huawei K3765

Check complete list in:
http://code.google.com/p/datacard/wiki/Supported_devices_eng

Before using the channel driver make sure to:

* Disable PIN code on your SIM card

Supported features:
* Place voice calls and terminate voice calls
* Send SMS and receive SMS
* Send and receive USSD commands / messages

Some useful AT commands:
AT+CCWA=0,0,1                                   #disable call-waiting
AT+CFUN=1,1                                     #reset datacard
AT^CARDLOCK="<code>"                            #unlock code
AT^SYSCFG=13,0,3FFFFFFF,0,3                     #modem 2G only, automatic search any band, no roaming
AT^U2DIAG=0                                     #enable modem function

Here is an example for the dialplan:

[datacard-incoming]
exten => sms,1,Verbose(Incoming SMS from ${CALLERID(num)} ${BASE64_DECODE(${SMS_BASE64})})
exten => sms,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DATACARD} - ${CALLERID(num)}: ${BASE64_DECODE(${SMS_BASE64})}' >> /var/log/asterisk/sms.txt)
exten => sms,n,Hangup()

exten => ussd,1,Verbose(Incoming USSD: ${BASE64_DECODE(${USSD_BASE64})})
exten => ussd,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DATACARD}: ${BASE64_DECODE(${USSD_BASE64})}' >> /var/log/asterisk/ussd.txt)
exten => ussd,n,Hangup()

exten => s,1,Dial(SIP/2001@othersipserver)
exten => s,n,Hangup()

[othersipserver-incoming]

exten => _X.,1,Dial(Datacard/r1/${EXTEN})
exten => _X.,n,Hangup

you can also use this:

Call using a specific group:
exten => _X.,1,Dial(Datacard/g1/${EXTEN})

Call using a specific group in round robin:
exten => _X.,1,Dial(Datacard/r1/${EXTEN})

Call using a specific datacard:
exten => _X.,1,Dial(Datacard/datacard0/${EXTEN})

Call using a specific provider name:
exten => _X.,1,Dial(Datacard/p:PROVIDER NAME/${EXTEN})

Call using a specific IMEI:
exten => _X.,1,Dial(Datacard/i:123456789012345/${EXTEN})

Call using a specific IMSI prefix:
exten => _X.,1,Dial(Datacard/s:25099203948/${EXTEN})

How to store your own number:

datacard cmd datacard0 AT+CPBS=\"ON\"
datacard cmd datacard0 AT+CPBW=1,\"+123456789\",145


Other CLI commands:

datacard reset <device>
datacard restart gracefully <device>
datacard restart now <device>
datacard restart when convenient <device>
datacard show device <device>
datacard show devices
datacard show version
datacard sms <device> number message
datacard ussd <device> number message
datacard stop gracefully <device>
datacard stop now <device>
datacard stop when convenient <device>
datacard start <device>
datacard restart gracefully <device>
datacard restart now <device>
datacard restart when convenient <device>
datacard remove gracefully <device>
datacard remove now <device>
datacard remove when convenient <device>
datacard reload gracefully
datacard reload now
datacard reload when convenient

```