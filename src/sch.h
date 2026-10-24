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

#include <Windows.h> // For wchar_t without compilation confusion like stddef.h causes.
#include <stdbool.h>


enum sch_ApiType {
	sch_kApiType_1, // Use mstask.h, IID_ITaskScheduler. XP and earlier
	sch_kApiType_2, // Use taskschd.h, IID_ITaskService, Vista and later
};

enum sch_ApiType sch_apiType;

// If successful, return true.
// If failed, return false.
// After the call, read sch_apiType to get th API type.
bool sch_init(void);

// Always harmless to call.
void sch_uninit(void);

// Create a boot task. If is API type 2, the task is in NoEject sub-folder.
bool sch_createTask(const wchar_t* name, const wchar_t* desc, const wchar_t* cmd, const wchar_t* arg);
