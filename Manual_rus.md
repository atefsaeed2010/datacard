# DATACARD драйвер канала Asterisk для устройств Huawei UMTS #

ВНИМАНИЕ:

Этот драйвер находится в стадии бета-тестирования. Используйте его на свой страх и риск.

Пожалуйста, используйте последнюю версию ядра Linux, (2.6.33 + рекомендуется). Если вы используете FreeBSD, 8.0 + рекомендуется.

## Список протестированных устройств с которыми драйвер успешно работает ##
  * Huawei K3715
  * Huawei E169 / K3520
  * Huawei E1550 - Полностью поддерживается

## На данный момент не поддерживаются ##
  * Huawei E160 / K3565
  * Huawei E150 - Односторонняя слышимость

## Перед использованием драйвера необходимо ##
  * Отключить запрос PIN-кода на вашей SIM-карте.
  * Отключить услугу - ожидание вызова у вашего оператора.

## Поддерживаемые функции ##
  * Голосовые вызововы
  * Отправка и прием SMS
  * Отправка и получение USSD команд / сообщений

## Некоторые полезные АТ команды ##
```
AT+CCWA=0,0,1                  отключение удержания вызова
AT+CFUN=1,1                    перезагрузка модема
AT^CARDLOCK="<code>"           разблокировка модема от оператора (нужен код разблокировки)
AT^SYSCFG=13,0,3FFFFFFF,0,3    ограничение исспользования 3G
AT^U2DIAG=0                    отключение всех встроенных устройств, кроме модема
```

Вот пример для плана набора:

Приём СМС и сохранение их в log.
```
[datacard-incoming]
exten => sms,1,Verbose(Incoming SMS from ${CALLERID(num)} ${SMS})
exten => sms,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DATACARD} - ${CALLERID(num)}: ${SMS}' >> /var/log/asterisk/sms.txt)
exten => sms,n,Hangup()
```

Прием ответа USSD (таких как баланс и т.п.)
```
exten => ussd,1,Verbose(Incoming USSD: ${USSD})
exten => ussd,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DATACARD}: ${USSD}' >> /var/log/asterisk/ussd.txt)
exten => ussd,n,Hangup()
```
Прием голосового звонка на номер 2001
```
exten => s,1,Dial(SIP/2001@othersipserver)
exten => s,n,Hangup()
```
[othersipserver-incoming]

exten => _X.,1,Dial(Datacard/[r1](https://code.google.com/p/datacard/source/detail?r=1)/${EXTEN})
exten =>_X.,n,Hangup

you can also use this:

Call using a specific group:
exten => _X.,1,Dial(Datacard/g1/${EXTEN})_

Call using a specific datacard:
exten => _X.,1,Dial(Datacard/datacard0/${EXTEN})_

Call using a specific provider name:
exten => _X.,1,Dial(Datacard/p:PROVIDER NAME/${EXTEN})_

Call using a specific IMEI:
exten => _X.,1,Dial(Datacard/i:123456789012345/${EXTEN})_

Call using a IMSI prefix:
exten => _X.,1,Dial(Datacard/s:25099/${EXTEN})_

How to store your own number:

datacard cmd datacard0 AT+CPBS=\"ON\"
datacard cmd datacard0 AT+CPBW=1,\"+123456789\",145