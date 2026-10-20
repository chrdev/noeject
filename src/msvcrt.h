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

#define DllImport __declspec(dllimport)

// Fake FILE to have a size.
typedef struct FILE {
	char* dummy_c[3];
	int   dummy_i[5];
}FILE;

DllImport FILE* __cdecl __iob_func(void);
#define stdout (__iob_func() + 1)
#define stderr (__iob_func() + 2)

#define LC_ALL 0

DllImport char* __cdecl setlocale( int category, const char* locale );
DllImport int __cdecl fprintf(FILE* stream, const char* format, ...);
DllImport int __cdecl _snwprintf(wchar_t* buffer, size_t bufsz, const wchar_t* format, ...);
DllImport size_t __cdecl wcslen(const wchar_t *str);
DllImport void* __cdecl memcpy(void* dest, const void* src, size_t count);
