# Introduction #

At this moment, only **Huawei** modems are supported. Check **Details** for the most up to date list.

If you want to contribute, the best way is to find (www.dc-files.com) any stable firmware (or update) that contains the following chains:

```
*^DDSETEX*
*^CVOICE*
```

Open the firmware file with any text editor and look for the mentioned text. If found, that means the modem is voice capable. The next step is to check if voice is enabled or not. A good solution is to download dc-unlocker client for that purpose.


# Details #

| **Device\Feature** | **Voice** | **SMS** | **USSD** | **Note** |
|:-------------------|:----------|:--------|:---------|:---------|
| E150               | +         | +       | +        | ~~one way voice~~ see <sub>[1]</sub> |
| E1550              | +         | +       | +        |          |
| E1552              | +         | +       | +        |          |
| E1553              | +         | +       | +        |          |
| E156               | ?         | ?       | ?        | test required |
| E156C              | +         | +       | +        |          |
| E160               | ?         | ?       | ?        | test required |
| E1612              | -         | +       | +        |          |
| E166               | ?         | ?       | ?        | test required |
| E169               | +         | +       | +        |          |
| E169G              | -         | ?       | ?        | test required |
| E172               | -         | ?       | ?        | test required |
| E1756              | +         | +       | +        |          |
| E178               | ?         | ?       | ?        | test required |
| E180               | ?         | ?       | ?        | test required |
| E220               | -         | +       | +        |          |
| E270               | -         | +       | +        |          |
| EG162G             | ?         | ?       | ?        | test required |
| K3520              | +         | +       | +        |          |
| K3565              | -         | -       | -        |          |
| K3715              | +         | +       | +        |          |
| K3765              | +         | +       | +        |          |

<p />
[1 ](.md) voice capable with special driver, please read [One way voice on Huawei E150](http://wiki.e1550.mobi/doku.php?id=troubleshooting#one_way_voice_on_huawei_e150)

## Credits ##
Thanks to Paco for the idea and information