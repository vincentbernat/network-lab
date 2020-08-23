# ...

Example of boot from Cisco device:

```text
*Mar  1 00:03:12.275: AUTOINSTALL: Obtain tftp server address (opt 150) 172.16.15.252
Loading c2960-lanbasek9-tar.150-2.SE11.txt from 172.16.15.252 (via Vlan1): !
[OK - 35 bytes]

*Mar  1 00:04:46.210: %AAA-3-ACCT_LOW_MEM_UID_FAIL: AAA unable to create UID for incoming calls due to insufficient processor memory
*Mar  1 00:04:47.452: %SYS-2-MALLOCFAIL: Memory allocation of 254880 bytes failed from 0xF18DB4, alignment 0
Pool: Processor  Free: 204544  Cause: Not enough free memory
Alternate Pool: None  Free: 0  Cause: No Alternate pool
 -Process= "DHCP Autoinstall", ipl= 0, pid= 284
-Traceback= E94DD0z 1453E1Cz 145AA2Cz 179F518z F18DB8z F1B57Cz F1799Cz 608F48z 60C838z 60DA5Cz 122D408z 122D6ECz 14CF4F4z 14C9AA8z
 Could not buffer tarfile...using multiple downloads
examining image...
extracting info (107 bytes)
extracting c2960-lanbasek9-mz.150-2.SE11/info (537 bytes)
extracting info (107 bytes)

System Type:             0x00000000
  Ios Image File Size:   0x00B4A200
  Total Image File Size: 0x00F8EA00
  Minimum Dram required: 0x04000000
  Image Suffix:          lanbasek9-150-2.SE11
  Image Directory:       c2960-lanbasek9-mz.150-2.SE11
  Image Name:            c2960-lanbasek9-mz.150-2.SE11.bin
  Image Feature:         LAYER_2|SSH|3DES|MIN_DRAM_MEG=64
  FRU Module Version:    No FRU Version Specified

Old image for switch 1: unknown

Extracting images from archive into flash...
c2960-lanbasek9-mz.150-2.SE11/ (directory)
extracting c2960-lanbasek9-mz.150-2.SE11/info (537 bytes)
extracting c2960-lanbasek9-mz.150-2.SE11/dc_default_profiles.txt (68774 bytes)
c2960-lanbasek9-mz.150-2.SE11/html/ (directory)
extracting c2960-lanbasek9-mz.150-2.SE11/html/ip.js (3500 bytes)
extracting c2960-lanbasek9-mz.150-2.SE11/html/graph.js (39650 bytes)
[...]
extracting c2960-lanbasek9-mz.150-2.SE11/c2960-lanbasek9-mz.150-2.SE11.bin (11832946 bytes)
extracting info (107 bytes)

Installing (renaming): `flash:update/c2960-lanbasek9-mz.150-2.SE11' ->
                                       `flash:/c2960-lanbasek9-mz.150-2.SE11'
New software image installed in flash:/c2960-lanbasek9-mz.150-2.SE11
All software images installed.
Requested system reload in progress...
[...]
Loading "flash:/c2960-lanbasek9-mz.150-2.SE11/c2960-lanbasek9-mz.150-2.SE11.bin"...@@@@@@@@@@
[...]
File "flash:/c2960-lanbasek9-mz.150-2.SE11/c2960-lanbasek9-mz.150-2.SE11.bin" uncompressed and installed, entry point: 0x3000
executing...

              Restricted Rights Legend

Use, duplication, or disclosure by the Government is
subject to restrictions as set forth in subparagraph
(c) of the Commercial Computer Software - Restricted
Rights clause at FAR sec. 52.227-19 and subparagraph
(c) (1) (ii) of the Rights in Technical Data and Computer
Software clause at DFARS sec. 252.227-7013.

           cisco Systems, Inc.
           170 West Tasman Drive
           San Jose, California 95134-1706



Cisco IOS Software, C2960 Software (C2960-LANBASEK9-M), Version 15.0(2)SE11, RELEASE SOFTWARE (fc3)
Technical Support: http://www.cisco.com/techsupport
Copyright (c) 1986-2017 by Cisco Systems, Inc.
[...]
```
