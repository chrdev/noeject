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
#include "sch.h"
#include "show.h"

#include <taskschd.h> // Vista and later
#include <mstask.h> // XP and earlier

#ifdef MY_MSVCRT_BUILD
#include "myvcrt.h"
#else
#include <stdio.h>
#endif

#pragma comment(lib, "uuid.lib") // IID_IPersistFile
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "mstask.lib")


enum dev_ApiType sch_apiType = sch_kApiType_1;


static ITaskService* service_ = NULL;
static ITaskFolder* rootFolder_ = NULL;


static __forceinline HRESULT
init_2(void) {
	HRESULT hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,	&IID_ITaskService, &service_);
	if (FAILED(hr)) return hr;
	VARIANT var = { 0 };
	hr = service_->lpVtbl->Connect(service_, var, var, var, var);
	if (FAILED(hr)) goto fail;
	hr = service_->lpVtbl->GetFolder(service_, L"\\", &rootFolder_);
	if (FAILED(hr)) goto fail;
	return hr;
fail:
	if (service_) service_->lpVtbl->Release(service_), service_ = NULL;
	return hr;
}

bool sch_init(void)
{
	HRESULT hr = init_2();
	if (SUCCEEDED(hr)) {
		sch_apiType = sch_kApiType_2;
		return true;
	}
	switch (hr) {
	case REGDB_E_CLASSNOTREG:
		return true; // Use API type 1, no further actions.
	default:
		show_cant("initialize", "task scheduler", hr);
		return false;
	}
}

static __forceinline void
uninit_2(void) {
	if (rootFolder_) rootFolder_->lpVtbl->Release(rootFolder_), rootFolder_ = NULL;
	if (service_) service_->lpVtbl->Release(service_), service_ = NULL;
}

void sch_uninit(void)
{
	if (sch_apiType == sch_kApiType_1) return;
	uninit_2();
}

static __forceinline bool
addBootTrigger_1(ITask* task) {
	WORD index = 0;
	ITaskTrigger* trigger = NULL;

	HRESULT hr = task->lpVtbl->CreateTrigger(task, &index, &trigger);
	if (FAILED(hr)) {
		show_cant("create", "trigger", hr);
		return false;
	}

	TASK_TRIGGER t = {
		.cbTriggerSize = sizeof(TASK_TRIGGER),
		.wBeginYear = 1999,
		.wBeginMonth = 1,
		.wBeginDay = 1,
		.TriggerType = TASK_EVENT_TRIGGER_AT_SYSTEMSTART,
	};

	hr = trigger->lpVtbl->SetTrigger(trigger, &t);
	trigger->lpVtbl->Release(trigger);
	if (FAILED(hr)) {
		show_cant("set", "boot trigger", hr);
		return false;
	}
	hr = task->lpVtbl->SetMaxRunTime(task, INFINITE);
	return true;
}

static __forceinline ITask*
createBootTask_1(const wchar_t* name, const wchar_t* comment) {
	if (!name) return NULL;

	ITaskScheduler* sch = NULL;
	HRESULT hr = CoCreateInstance(&CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskScheduler, &sch);
	if (FAILED(hr)) {
		show_cant("create", "ITaskScheduler", hr);
		return NULL;
	}

	ITask* task = NULL;
	hr = sch->lpVtbl->NewWorkItem(sch, name, &CLSID_CTask, &IID_ITask, (IUnknown**)&task);
	if (hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS)) {
		hr = sch->lpVtbl->Delete(sch, name);
		if (FAILED(hr)) {
			show_cant("delete", "existing task", hr);
			sch->lpVtbl->Release(sch);
			return NULL;
		}
		hr = sch->lpVtbl->NewWorkItem(sch, name, &CLSID_CTask, &IID_ITask, (IUnknown**)&task);
	}
	sch->lpVtbl->Release(sch);
	if (FAILED(hr)) {
		show_cant("create", "task", hr);
		return NULL;
	}

	if (!addBootTrigger_1(task)) {
		task->lpVtbl->Release(task);
		return NULL;
	}

	hr = task->lpVtbl->SetAccountInformation(task, L"", NULL);
	if (FAILED(hr)) {
		show_cant("set", "account", hr);
		task->lpVtbl->Release(task);
		return NULL;
	}

	if (!comment) return task;

	hr = task->lpVtbl->SetComment(task, comment);
	if (FAILED(hr)) {
		show_cant("set", "comment", hr);
		// Only prompt. Ignore the error.
	}

	return task;
}

static __forceinline bool
setTaskAction_1 (ITask* task, const wchar_t* cmd, const wchar_t* params, const wchar_t* workingDir) {
	if (!cmd) return false;

	bool rlt = false;
	HRESULT hr = task->lpVtbl->SetApplicationName(task, cmd);
	if (FAILED(hr)) {
		show_cant("set", "command", hr);
		goto fin;
	}
	if (params) {
		hr = task->lpVtbl->SetParameters(task, params);
		if (FAILED(hr)) {
			show_cant("set", "parameters", hr);
			goto fin;
		}
	}
	if (workingDir) {
		hr = task->lpVtbl->SetWorkingDirectory(task, workingDir);
		if (FAILED(hr)) {
			show_cant("set", "working directory", hr);
			goto fin;
		}
	}
	rlt = true;
fin:
	return rlt;
}

// If succeeded, taks is released.
static __forceinline bool
finalizeTask_1(ITask* task) {
	IPersistFile* pf = NULL;
	HRESULT hr = task->lpVtbl->QueryInterface(task, &IID_IPersistFile, &pf);
	if (FAILED(hr)) {
		show_cant("query interface for", "IPersistFile", hr);
		return false;
	}
	hr = pf->lpVtbl->Save(pf, NULL, TRUE);
	pf->lpVtbl->Release(pf);
	if (FAILED(hr)) {
		show_cant("save", "task", hr);
		return false;
	}
	task->lpVtbl->Release(task);
	return true;
}

static __forceinline bool
createTask_1(const wchar_t* name, const wchar_t* desc, const wchar_t* cmd, const wchar_t* arg) {
	bool ok = false;

	ITask* task = createBootTask_1(name, desc);
	if (!task) goto fin;
	ok = setTaskAction_1(task, cmd, arg, NULL);
	if (!ok) goto fin;
	ok = finalizeTask_1(task);
	if (ok) task = NULL;
fin:
	if (task) task->lpVtbl->Release(task);
	return ok;
}

static __forceinline ITaskDefinition*
createTaskDefinition_2(void) {
	ITaskDefinition* task = NULL;
	HRESULT hr = service_->lpVtbl->NewTask(service_, 0, &task);
	if (FAILED(hr)) {
		show_cant("create", "task", hr);
		return NULL;
	}
	return task;
}

static __forceinline bool
setUser_2 (ITaskDefinition* task, const wchar_t* user) {
	IPrincipal* principal = NULL;
	HRESULT hr = task->lpVtbl->get_Principal(task, &principal);
	if (FAILED(hr)) {
		show_cant("get", "IPrincipal", hr);
		return false;
	}

	hr = principal->lpVtbl->put_Id(principal, (BSTR)L"Author");	if (FAILED(hr)) goto fin;
	hr = principal->lpVtbl->put_UserId(principal, (BSTR)user);	if (FAILED(hr)) goto fin;
	hr = principal->lpVtbl->put_LogonType(principal, TASK_LOGON_SERVICE_ACCOUNT);
fin:
	principal->lpVtbl->Release(principal);
	if (FAILED(hr)) show_cant("put", "user property", hr);
	return SUCCEEDED(hr);
}

static __forceinline bool
addAction_2(ITaskDefinition* task, const wchar_t* cmd, const wchar_t* arg) {
	IActionCollection* actionSet = NULL;
	HRESULT hr = task->lpVtbl->get_Actions(task, &actionSet);
	if (FAILED(hr)) {
		show_cant("get", "IActionCollection", hr);
		return false;
	}
	hr = actionSet->lpVtbl->put_Context(actionSet, (BSTR)L"Author");
	if (FAILED(hr)) {
		actionSet->lpVtbl->Release(actionSet);
		show_cant("put", "context", hr);
		return false;
	}

	IAction* action = NULL;
	hr = actionSet->lpVtbl->Create(actionSet, TASK_ACTION_EXEC, &action);
	actionSet->lpVtbl->Release(actionSet);
	if (FAILED(hr)) {
		show_cant("create", "IAction", hr);
		return false;
	}

	IExecAction* execAction = NULL;
	hr = action->lpVtbl->QueryInterface(action, &IID_IExecAction, &execAction);
	action->lpVtbl->Release(action);
	if (FAILED(hr)) {
		show_cant("query interface for", "IExecAction", hr);
		return false;
	}

	hr = execAction->lpVtbl->put_Path(execAction, (BSTR)cmd);
	if (FAILED(hr)) {
		show_cant("put", "command", hr);
		goto fin;
	}
	if (arg) {
		hr = execAction->lpVtbl->put_Arguments(execAction, (BSTR)arg);
		if (FAILED(hr)) {
			show_cant("put", "arguments", hr);
			goto fin;
		}
	}
fin:
	execAction->lpVtbl->Release(execAction);
	return SUCCEEDED(hr);
}

static __forceinline bool
addBootTrigger_2(ITaskDefinition* task) {
	ITriggerCollection* triggerSet = NULL;
	HRESULT hr = task->lpVtbl->get_Triggers(task, &triggerSet);
	if (FAILED(hr)) {
		show_cant("get", "ITriggerCollection", hr);
		return false;
	}

	ITrigger* trigger = NULL;
	hr = triggerSet->lpVtbl->Create(triggerSet, TASK_TRIGGER_BOOT, &trigger);
	triggerSet->lpVtbl->Release(triggerSet);
	if (FAILED(hr)) {
		show_cant("create", "boot trigger", hr);
		return false;
	}

	trigger->lpVtbl->Release(trigger);
	return true;
}

static __forceinline bool
clearSettings_2(ITaskDefinition* task) {
	ITaskSettings* s = NULL;
	HRESULT hr = task->lpVtbl->get_Settings(task, &s);
	if (FAILED(hr)) {
		show_cant("get", "ITaskSettings", hr);
		return false;
	}

	hr = s->lpVtbl->put_DisallowStartIfOnBatteries(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_StopIfGoingOnBatteries(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_AllowHardTerminate(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_AllowDemandStart(s, 0); if (FAILED(hr)) goto fin;
	hr = s->lpVtbl->put_ExecutionTimeLimit(s, L"PT0S");
fin:
	s->lpVtbl->Release(s);
	if (FAILED(hr)) show_cant("put", "settings property", hr);
	return SUCCEEDED(hr);
}

static __forceinline bool
setDesc_2(ITaskDefinition* task, const wchar_t* desc) {
	IRegistrationInfo* r = NULL;
	HRESULT hr = task->lpVtbl->get_RegistrationInfo(task, &r);
	if (FAILED(hr)) {
		show_cant("get", "IRegistrationInfo", hr);
		return false;
	}

	hr = r->lpVtbl->put_Description(r, (BSTR)desc);

	r->lpVtbl->Release(r);
	if (FAILED(hr)) show_cant("put", "description", hr);
	return SUCCEEDED(hr);
}

static __forceinline ITaskFolder*
createFolder_2(const wchar_t* path) {
	ITaskFolder* f = NULL;
	VARIANT ssdl = { 0 };
	HRESULT hr = rootFolder_->lpVtbl->CreateFolder(rootFolder_, (BSTR)path, ssdl, &f);
	if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
		hr = rootFolder_->lpVtbl->GetFolder(rootFolder_, (BSTR)path, &f);
	}
	if (FAILED(hr)) show_cant("create", "folder", hr);
	return f;
}

static __forceinline bool
registerTask_2(const wchar_t* taskFolder, const wchar_t* name, ITaskDefinition* task) {
	ITaskFolder* folder = createFolder_2(taskFolder);
	if (!folder) return false;

	HRESULT hr = folder->lpVtbl->DeleteTask(folder, (BSTR)name, 0);
	if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && FAILED(hr)) {
		show_cant("delete", "existing task", hr);
		goto fin;
	}

	VARIANT userId = { VT_EMPTY };
	VARIANT password = { VT_EMPTY };
	VARIANT sddl = { VT_EMPTY };
	IRegisteredTask* newTask = NULL;
	hr = folder->lpVtbl->RegisterTaskDefinition(folder, (BSTR)name, task, TASK_CREATE, userId, password, TASK_LOGON_NONE, sddl, &newTask);
	if (FAILED(hr)) {
		show_cant("register", "task", hr);
		goto fin;
	}
	newTask->lpVtbl->Release(newTask);
fin:
	folder->lpVtbl->Release(folder);
	return SUCCEEDED(hr);
}

// Param name must not be NULL.
// Param desc can be NULL.
// Param cmd must not be NULL.
// Param arg can be NULL.
static __forceinline bool
createTask_2(const wchar_t* name, const wchar_t* desc, const wchar_t* cmd, const wchar_t* arg) {
	bool ok = false;

	ITaskDefinition* task = createTaskDefinition_2();
	if (!task) goto fin;
	ok = setUser_2(task, L"S-1-5-18"); // SYSTEM
	if (!ok) goto fin;
	ok = addAction_2(task, cmd, arg);
	if (!ok) goto fin;
	ok = addBootTrigger_2(task);
	if (!ok) goto fin;
	clearSettings_2(task); // We don't care if clearSettings_2() succeeded.
	if (desc) {
		setDesc_2(task, desc); // We don't care if setDesc_2() succeeded.
	}
	ok = registerTask_2(L"NoEject", name, task);
fin:
	if (task) task->lpVtbl->Release(task);
	return ok;
}

bool sch_createTask(const wchar_t* name, const wchar_t* desc, const wchar_t* cmd, const wchar_t* arg)
{
	switch (sch_apiType) {
	case sch_kApiType_1:
		return createTask_1(name, desc, cmd, arg);
	case sch_kApiType_2:
		return createTask_2(name, desc, cmd, arg);
	default:
		return false;
	}
}
