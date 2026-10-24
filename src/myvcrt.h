/*
* Zero-Clause BSD
* =============
*
* Permission to use, copy, modify, and/or distribute this software for
* any purpose with or without fee is hereby granted.
*
* THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
* AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
* OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#pragma once


#define DLL_I __declspec(dllimport)


#define _CONSOLE_APP 1
#define LC_ALL       0
#define stdin        (&_iob[0])
#define stdout       (&_iob[1])
#define stderr       (&_iob[2])


// Ref to mingw64\include\corecrt_wstdio.h
// We use this dummy struct to get a size.
// Works on both x64 and x86 with the default pack size.
typedef struct FILE {
	char* dummy_c[3];
	int   dummy_i[5];
}FILE;

// _iob is always exported, from Windows 2000 to Windows 11.
// While __iob_func is not avalible on Windows 2000 msvcrt.dll.
DLL_I FILE           _iob[3];

DLL_I void   __cdecl __set_app_type(int type);
DLL_I int    __cdecl _snwprintf(wchar_t* buffer, size_t bufsz, const wchar_t* format, ...);
DLL_I int    __cdecl fflush(FILE* stream);
DLL_I int    __cdecl fprintf(FILE* stream, const char* format, ...);
DLL_I void*  __cdecl memcpy(void* dest, const void* src, size_t count);
DLL_I char*  __cdecl setlocale( int category, const char* locale );
DLL_I size_t __cdecl wcslen(const wchar_t *str);
