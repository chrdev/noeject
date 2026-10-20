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

#include <taskschd.h>

#include <stdbool.h>


bool schd_init(void);

// Always harmless to call.
void schd_uninit(void);

ITaskDefinition* schd_createTask(void);

bool schd_registerTask(const wchar_t* folder, const wchar_t* name, ITaskDefinition* task);

bool schd_taskSetUser(ITaskDefinition* task, const wchar_t* user);

bool schd_taskAddTriggerBoot(ITaskDefinition* task);

bool schd_taskAddAction(ITaskDefinition* task, const wchar_t* cmd, const wchar_t* params);

bool schd_taskClearSettings(ITaskDefinition* task);

bool schd_taskSetDesc(ITaskDefinition* task, const wchar_t* desc);
