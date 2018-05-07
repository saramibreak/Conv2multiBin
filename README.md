# Conv2multiBin
## Summary
This program converts img(clonecd) or bin(isobuster) to multiple bin(redump.org).
It works on Windows PC (WinXP or higher).

## Usage
fix datfile[(maker) - (machine) (yyyymmdd hh-mm-ss).dat]
	[ & ]->[ &amp; ]
Start cmd.exe & run exe. for more information, run in no arg.

## Tested dat
Audio CD
Sega - Saturn
Sony - PlayStation
SNK - Neo Geo CD

## Development Tool
Visual Studio 2017 (Visual C++ 2017)

## License
See LICENSE

## Disclaimer
Use this tool at own your risk.
Trouble in regard to the use of this tool, I can not guarantee any.

## Change Log
### 2012-12-06
http://www.mediafire.com/?6vbwjz9xapy0bs7  
Initial release  

### 2012-12-28
http://www.mediafire.com/?comcjwksktxte10  
Support audio only disc

### 2014-04-11
add: Using EccEdc.exe, insert pregap sector

### 2018-04-19
changed: LICENSE (MS-PL -> Apache License Version 2.0)  
changed: Visual Studio 2013 to 2017

### 2018-05-07
added: error check (write offsets + drive offsets > -1165)
