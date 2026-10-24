# Build noeject.exe x64 or x86. VS2026 nmake.
# chrdev, 0BSD.


.SUFFIXES : .c .def


# Paths
# =============================================================================

N = noeject
S = src
B = build
O = $B\$(PLATFORM)
T = $O\$N.exe
R = $N-$(PLATFORM)
C = $B\$R


# Source files
# =============================================================================

OBJS = \
	$O\dev.obj $O\sch.obj $O\noeject.obj $O\myvcrt.lib


# Flags
# =============================================================================

CC = cl

CFLAGS = $(CFLAGS) /nologo \
	/c /TC /W4 /WX /GS- \
	/O1 /Oi /Gy /GF \
	/GL \
	/DNDEBUG /D_CONSOLE /D_UNICODE /DUNICODE \
	/DMY_MSVCRT_BUILD \

LD = link

LDFLAGS = $(LDFLAGS) /NOLOGO \
	/WX \
	/LTCG \
	/DYNAMICBASE:NO \
	/NODEFAULTLIB \
	/ENTRY:startup \

LDLIBS = kernel32.lib shell32.lib ole32.lib advapi32.lib \
	setupapi.lib taskschd.lib mstask.lib uuid.lib \

!IF "$(PLATFORM)" == "x86"
# 5.00 / Windows 2000 is not supported by Visual Stiduo 2026 link.exe.
# But we can later use editbin.exe to set 5.00, albeit it gives a warning.
# Subsystem 5.01 is Windows XP 32-bit
LDFLAGS_SUBSYS = /SUBSYSTEM:CONSOLE,5.01
!ELSE
# Subsystem 5.02 is Windows XP 64-bit
LDFLAGS_SUBSYS = /SUBSYSTEM:CONSOLE,5.02
!ENDIF


# Build
# =============================================================================

all : $O $T

$O :
	if not exist $O mkdir $O

$T : $(OBJS)
	$(LD) /OUT:$@ $** $(LDFLAGS) $(LDFLAGS_SUBSYS) $(LDLIBS)
!IF "$(PLATFORM)" == "x86"
# Support Windows 2000
	editbin /NOLOGO /SUBSYSTEM:CONSOLE,5.00 $@
!ENDIF


# Pack
# =============================================================================

collect : all
	if exist $C rmdir /s /q $C
	mkdir $C
	copy /y $T $C
	copy /y README.* $C
	copy /y LICENSE* $C
	xcopy /s /i pic $C\pic

release : collect
	cd $B
	7z a -mx -mtr=off -stl -sdel $R.7z $R
	cd ..


# Inferences
# =============================================================================

{$S}.c{$O}.obj:
	$(CC) $(CFLAGS) /Fo$@ $<

{$S}.def{$O}.lib:
	lib /nologo /MACHINE:$(PLATFORM) /OUT:$@ /DEF:$<


# Maintenance
# =============================================================================

clean:
	if exist $O rmdir /s /q $O

cleanall:
	if exist $B rmdir /s /q $B
