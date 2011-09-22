MU Filesystem
=============

Mount megaupload as a filesystem and get an awesome VOD @ home.

Usage
=====

Create a list.txt with the following format :

    Breaking.Bad.S04E01.avi http://www.megaupload.com/?d=2BUIZP41
    Breaking.Bad.S04E02.avi http://www.megaupload.com/?d=L9CR3D30
    Breaking.Bad.S04E03.avi http://www.megaupload.com/?d=JBV8NQ3I
    Breaking.Bad.S04E04.avi http://www.megaupload.com/?d=0WA1U5ID
    Breaking.Bad.S04E05.avi http://www.megaupload.com/?d=NEPD1VL4
    Breaking.Bad.S04E06.avi http://www.megaupload.com/?d=3EUU5SZN
    Breaking.Bad.S04E07.avi http://www.megaupload.com/?d=WGLWMEDF
    Breaking.Bad.S04E08.avi http://www.megaupload.com/?d=ENT4I58B
    Breaking.Bad.S04E09.avi http://www.megaupload.com/?d=TO793Y02

Mount using :

    $ mufs </path/to/list.txt> <mount point> -omu:<MU user:Mu password>


How it works
============

You see files just like if they were on your local disk.
You just have to open your favorite media player and enjoy ;)
Pause, fastforward and so on are supported.

Tips
====

Share the mounted directory with SAMBA/VFS and play your library on your home theater (XBMC, boxee)
