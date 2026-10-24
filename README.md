# NoEject

Hide items from the taskbar *Safely Remove Hardware And Eject Media* menu. Support Windows 11 back to Windows 2000.

From this

![Original Eject Menu](pic/original.png)

To this

![Hidden by NoEject](pic/noejected.png)

Tasks per device are added in **Task Scheduler** NoEject sub-folder. (Windows XP and 2000 don't have sub-folders.)
If you don't need them any more, remove manually.

![Task Scheduler](pic/scheduler.png)

## Usage

```
noeject.exe /?
--------------
NoEject 1.1.0 (0BSD License) Hide devices from the eject menu.
For Windows 11 back to Windows 2000.
    -[?|h]              Show this help.
    -s                  Save the hiding.
Typical usage steps:
    noeject.exe         1. List removable devices.
    noeject.exe -s      2. Hide removable devices.
Unhide:
    Use Task Scheduler, delete tasks in NoEject sub-folder.
    (Windows XP and 2000 don't have sub-folders.)
Tips:
    If the device is not listed, but showing in the eject menu,
    eject the device but don't unplug, then re-run noeject.
```

## Is it safe?

I believe so. A scheduled task first check if the device of interest exists. 
If it exists, check whether it has the original removal capabilities. 
Only if all things are right, change the device capabilities to non-removable.

So if the device hardware is removed, the system remains intact. No harm should be done.

## Development

A VS2026 solution is provided to ease the development. It links to UCRT.

If compile using nmake makefile, the binaries link to the classic msvcrt.dll.

## License

Zero-Clause BSD
