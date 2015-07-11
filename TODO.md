
```

TODO list

added 20.11.2010
    1) Command queue or serialization of write operations to device
        done at 27.11.2010 r45
    2) Handling call waiting
        done at 27.11.2010 r45

3) Extended device name 'i:' 's:' for DEVICE_STATE() and DatacardStatus()

        4) automatic gain control for rxgain/txgain
                exists solution:
                        download speex library http://www.speex.org
                        build/install
                        rebuild asterisk with speex and func_speex
                        Set(AGC(rx)=8000) in dialplan

        5) Make context setting optional with 'default' as default value
                20.11.2010 already exists in base 175 rev. exported

        6) Conference support
                added 16.12.2010 with Dial() 'conference' option

        7) Remove warning 'Dont know how to indicate condition 20'
                done at 20.11.2010

8) LED control

        9) SMS receive in Unicode
                send in PDU added at 27.11.2010 r45
                send in UCS-2 added at 05.12.2010 r72
                receive side done at 06.12.2010 r75

10) Automatic device discovery by IMEI or IMSI

11) Complete fix of DTMF duplication

        12) Full support of SMS receive with active voice call
                feel satisfied at 16.12.2010

added 24.11.2010

        13) Device files locking
                done at 17.12.2010 in r106

        14) SMS PDU mode
                duplicate for 9)
                        added from internal source at 25.11.2010

15) Do a lot of testing with the channel driver

        16) Test sending SMS with a new line character in it
                done at 05.12.2010

17) Find a way how to proper detect remote side alerting (GSM 02.40)

18) Add PIN code detection

19) Cleanup code

        20) Make a better Makefile
                switch to autoconf at 05.12.2010

21) Write a better documentation
        done paritally with samples

22) Add more API commands

        22.1) 'disable' option in device section
                done 27.11.2010 at r46 with [defaults]

        22.2) [global] with template settings
                done 27.11.2010 at r46 with [defaults]. Also available asterisk template feature for config files

        22.3) exten
                added 06.12.2010 r83

23)

24) reconfigure on fly

        25) implement command "datacard show version"
                done at 06.12.2010 in r77

        26) implement command "datacard restart device"
                done at 06.12.2010 in r77


added 06.12.2010

27) Remove Call waiting status duplicated messages

28) outgoing SMS: set SRR, validity, SC address, on screen mode
        SRR and validity done 06.12.2010 in r75

29) incoming SMS: pass up raw PDU, raw message, SCA, SCTS, PID, UDH,  fields of DCS

30) incoming SMS reports

        31) outgoing SMS: send in 7Bit or 8Bit if possible
                done at 08.12.2010 in r81

31) Handling of response errors, CLIR for example

32) Control SMS receiving from dialplan
        (when message received channel Local created rules read message, delete or some other)

added 17.12.2010

33) Start conference from any channel

34) SMS deletion function

35) receive SMS notification

36) receive on-screen SMS
```