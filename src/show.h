/* Zero-Clause BSD
// =============
//
// Permission to use, copy, modify, and/or distribute this software for
// any purpose with or without fee is hereby granted.
//
// THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
// FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
// DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
// AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
// OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#pragma once

#ifdef MY_MSVCRT_BUILD
#include "myvcrt.h"
#else
#include <stdio.h>
#endif


static __forceinline void
show_cant(const char* verb, const char* noun, unsigned code) {
	fprintf(stderr, "\nCan't %s %s: 0x%X.\n", verb, noun, code);
}

static __forceinline void
show_code(const char* what, unsigned code) {
	fprintf(stderr, "\n%s error: 0x%X.\n", what, code);
}

static __forceinline void
show_lowMem(void) {
	fprintf(stderr, "\nLow memory.\n");
}
