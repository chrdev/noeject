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
#include "schd.h"

#include <sddl.h>

#ifdef MY_MSVCRT_BUILD
#include "msvcrt.h"
#else
#include <stdio.h>
#endif

#pragma comment(lib, "taskschd.lib")


static ITaskService* service_ = NULL;
static ITaskFolder* rootFolder_ = NULL;


static __forceinline void
showErrorGet(const char* obj, HRESULT hr) {
	fprintf(stderr, "Can't get %s: 0x%X.\n", obj, hr);
}

bool schd_init(void)
{
	HRESULT hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,	&IID_ITaskService, &service_);
	if (FAILED(hr)) goto fin;
	VARIANT var = { 0 };
	hr = service_->lpVtbl->Connect(service_, var, var, var, var);
	if (FAILED(hr)) goto fin;
	hr = service_->lpVtbl->GetFolder(service_, L"\\", &rootFolder_);
fin:
	if (FAILED(hr)) {
		fprintf(stderr, "Can't initialize task scheduler: 0x%X.\n", hr);
		if (service_) service_->lpVtbl->Release(service_), service_ = NULL;
	}
	return SUCCEEDED(hr);
}

void schd_uninit(void) {
	if (rootFolder_) rootFolder_->lpVtbl->Release(rootFolder_), rootFolder_ = NULL;
	if (service_) service_->lpVtbl->Release(service_), service_ = NULL;
}

ITaskDefinition* schd_createTask(void)
{
	ITaskDefinition* task = NULL;
	HRESULT hr = service_->lpVtbl->NewTask(service_, 0, &task);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't create task: 0x%X.\n", hr);
		return NULL;
	}
	return task;
}

static __forceinline ITaskFolder*
createFolder(const wchar_t* path) {
	ITaskFolder* f = NULL;
	VARIANT ssdl = { 0 };
	HRESULT hr = rootFolder_->lpVtbl->CreateFolder(rootFolder_, (BSTR)path, ssdl, &f);
	if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
		hr = rootFolder_->lpVtbl->GetFolder(rootFolder_, (BSTR)path, &f);
	}
	if (FAILED(hr)) fprintf(stderr, "Can't create folder \"%ls\": 0x%X.\n", path, hr);
	return f;
}

bool schd_registerTask(const wchar_t* taskFolder, const wchar_t* name, ITaskDefinition* task)
{
	ITaskFolder* folder = createFolder(taskFolder);
	if (!folder) return false;

	HRESULT hr = folder->lpVtbl->DeleteTask(folder, (BSTR)name, 0);
	if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && FAILED(hr)) {
		fprintf(stderr, "Can't delete old task \"%ls\": 0x%X.\n", name, hr);
		goto fin;
	}

	VARIANT userId = { VT_EMPTY };
	VARIANT password = { VT_EMPTY };
	VARIANT sddl = { VT_EMPTY };
	IRegisteredTask* newTask = NULL;
	hr = folder->lpVtbl->RegisterTaskDefinition(folder, (BSTR)name, task, TASK_CREATE, userId, password, TASK_LOGON_NONE, sddl, &newTask);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't register task \"%ls\": 0x%X.\n", name, hr);
		goto fin;
	}
	newTask->lpVtbl->Release(newTask);
fin:
	folder->lpVtbl->Release(folder);
	return SUCCEEDED(hr);
}

bool schd_taskSetUser(ITaskDefinition* task, const wchar_t* user)
{
	IPrincipal* principal = NULL;
	HRESULT hr = task->lpVtbl->get_Principal(task, &principal);
	if (FAILED(hr)) {
		showErrorGet("IPrincipal", hr);
		return false;
	}

	hr = principal->lpVtbl->put_Id(principal, (BSTR)L"Author");	if (FAILED(hr)) goto fin;
	hr = principal->lpVtbl->put_UserId(principal, (BSTR)user);	if (FAILED(hr)) goto fin;
	hr = principal->lpVtbl->put_LogonType(principal, TASK_LOGON_SERVICE_ACCOUNT);
fin:
	principal->lpVtbl->Release(principal);
	if (FAILED(hr)) fprintf(stderr, "Can't put user property: 0x%X.\n", hr);
	return SUCCEEDED(hr);
}

bool schd_taskAddTriggerBoot(ITaskDefinition* task)
{
	ITriggerCollection* triggerSet = NULL;
	HRESULT hr = task->lpVtbl->get_Triggers(task, &triggerSet);
	if (FAILED(hr)) {
		showErrorGet("ITriggerCollection", hr);
		return false;
	}

	ITrigger* trigger = NULL;
	hr = triggerSet->lpVtbl->Create(triggerSet, TASK_TRIGGER_BOOT, &trigger);
	triggerSet->lpVtbl->Release(triggerSet);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't create boot trigger: 0x%X.\n", hr);
		return false;
	}

	trigger->lpVtbl->Release(trigger);
	return true;
}

bool schd_taskAddAction(ITaskDefinition* task, const wchar_t* cmd, const wchar_t* params)
{
	IActionCollection* actionSet = NULL;
	HRESULT hr = task->lpVtbl->get_Actions(task, &actionSet);
	if (FAILED(hr)) {
		showErrorGet("IActionCollection", hr);
		return false;
	}
	hr = actionSet->lpVtbl->put_Context(actionSet, (BSTR)L"Author");
	if (FAILED(hr)) {
		actionSet->lpVtbl->Release(actionSet);
		fprintf(stderr, "Can't put context: 0x%X.\n", hr);
		return false;
	}

	IAction* action = NULL;
	hr = actionSet->lpVtbl->Create(actionSet, TASK_ACTION_EXEC, &action);
	actionSet->lpVtbl->Release(actionSet);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't create exec action: 0x%X.\n", hr);
		return false;
	}

	IExecAction* execAction = NULL;
	hr = action->lpVtbl->QueryInterface(action, &IID_IExecAction, &execAction);
	action->lpVtbl->Release(action);
	if (FAILED(hr)) {
		showErrorGet("IExecAction", hr);
		return false;
	}

	hr = execAction->lpVtbl->put_Path(execAction, (BSTR)cmd);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't set action command \"%ls\": 0x%X.\n", cmd, hr);
		goto fin;
	}
	hr = execAction->lpVtbl->put_Arguments(execAction, (BSTR)params);
	if (FAILED(hr)) {
		fprintf(stderr, "Can't set action arguments \"%ls\": 0x%X.\n", params, hr);
	}
fin:
	execAction->lpVtbl->Release(execAction);
	return SUCCEEDED(hr);
}

bool schd_taskClearSettings(ITaskDefinition* task)
{
	ITaskSettings* s = NULL;
	HRESULT hr = task->lpVtbl->get_Settings(task, &s);
	if (FAILED(hr)) {
		showErrorGet("ITaskSettings", hr);
		return false;
	}

	hr = s->lpVtbl->put_DisallowStartIfOnBatteries(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_StopIfGoingOnBatteries(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_AllowHardTerminate(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_AllowDemandStart(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_ExecutionTimeLimit(s, L"PT0S");
fin:
	s->lpVtbl->Release(s);
	if (FAILED(hr)) fprintf(stderr, "Can't put settings property: 0x%X.\n", hr);
	return SUCCEEDED(hr);
}

bool schd_taskSetDesc(ITaskDefinition* task, const wchar_t* desc)
{
	IRegistrationInfo* r = NULL;
	HRESULT hr = task->lpVtbl->get_RegistrationInfo(task, &r);
	if (FAILED(hr)) {
		showErrorGet("IRegistrationInfo", hr);
		return false;
	}
	
	hr = r->lpVtbl->put_Description(r, (BSTR)desc);

	r->lpVtbl->Release(r);
	if (FAILED(hr)) fprintf(stderr, "Can't put description: 0x%X.\n", hr);
	return SUCCEEDED(hr);
}
