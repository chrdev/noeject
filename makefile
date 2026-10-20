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
	$O\schd.obj $O\noeject.obj $O\msvcrt.lib


# Flags
# =============================================================================

CC = cl

CFLAGS = $(CFLAGS) /nologo \
	/c /TC /W4 /WX /GS- \
	/O1 /Oi /Gy /GF /Zc:inline \
	/DNDEBUG /D_CONSOLE /D_UNICODE /DUNICODE \
	/DMY_MSVCRT_BUILD \

LD = link

LDFLAGS = $(LDFLAGS) /NOLOGO \
	/WX \
	/FIXED \
	/SUBSYSTEM:CONSOLE \
	/NODEFAULTLIB \
	/ENTRY:startup \

LDLIBS = kernel32.lib shell32.lib ole32.lib advapi32.lib \
	setupapi.lib taskschd.lib \


# Build
# =============================================================================

all : $O $T

$O :
	if not exist $O mkdir $O

$T : $(OBJS)
	$(LD) /OUT:$@ $** $(LDFLAGS) $(LDLIBS)


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
