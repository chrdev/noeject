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
//
/*
* If a device has one of the following features, we consider it removable and if the user wish, take care of it:
* 1. It has CM_DEVCAP_REMOVABLE, but not CM_DEVCAP_SURPRISEREMOVALOK Capabilities.
* 2. It has both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK capabilities, and ejected but not unplugped.
* 
* Case 2 explanation:
* It's been observed, for a certain monitor, which has built-in devices A, B, and C, that A appears in the eject menu.
* Device A is both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK. Schedule to hide it doesn't work.
* Device B is A's parent device, It's both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK.
* Schedule to hide device B alone, or simultaneously with A, doesn't work.
* If we click the eject menu to eject device A, A will disappear from the menu as expected.
* But through Device Manager, we can see A is still there intact.
* At the same time, we see device C ejected. C is a seemingly irrelevant device.
* Schedule to hide device C alone actually hide device A.
* 
* So we can ask the user to eject a device, but not unplug it, to find the right device to schedule hiding.
*/
//
#define MY_VER "1.0.0"


#define _WIN32_DCOM
#include <Windows.h>
#include <cfgmgr32.h>
#include <SetupAPI.h>
#include <initguid.h> // Needed by Devpkey.h
#include <Devpkey.h> // DEVPKEY_*, need initguid.h

#include <stdbool.h>

#ifdef MY_MSVCRT_BUILD
#include "msvcrt.h"
#else
#include <locale.h>
#include <stdio.h>
#endif

#include "schd.h"

#pragma comment(lib, "shell32.lib") // CommandLineToArgvW()
#pragma comment(lib, "ole32.lib") // CoInitializeEx()
#pragma comment(lib, "advapi32.lib") // OpenProcessToken()
#pragma comment(lib, "setupapi.lib")


enum Intent{
	kList,
	kSchedule,
	kHelp,
};

static __forceinline enum Intent
parseArg(int argc, const wchar_t* const * argv) {
	if (argc == 1) return kList;
	if (argc > 2) return kHelp;
	
	const wchar_t* p = argv[1];
	for (; *p == L'-' || *p == L'/'; ++p);
	switch (*p) {
	case L's':
	case L'S':
		if (*++p) return kHelp;
		return kSchedule;
	default:
		return kHelp;
	}
}

static __forceinline enum Intent
parseCmd(void) {
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) return kHelp;
	enum Intent rlt = parseArg(argc, argv);
	LocalFree(argv);
	return rlt;
}

static __forceinline void
showHelp(void) {
	static const char kHelp[] =
		"NoEject " MY_VER " (0BSD License) Hide devices from the eject menu.\n"
		"    -[?|h]              Show this help.\n"
		"    -s                  Save the hiding.\n"
		"Typical usage steps:\n"
		"    noeject.exe         1. List removable devices.\n"
		"    noeject.exe -s      2. Hide removable devices.\n"
		"Unhide:\n"
		"    Use Task Scheduler, delete tasks in NoEject sub-folder.\n"
		"Tips:\n"
		"    If the device is not listed, but showing in the eject menu,\n"
		"    eject the device but don't unplug, then re-run noeject.\n";

	fprintf(stdout, "%s", kHelp);
}

static __forceinline void
showElevationTip(bool isToSchedule) {
	const WORD attr = isToSchedule ?
		BACKGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY :
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
	BOOL ok = h != INVALID_HANDLE_VALUE;
	CONSOLE_SCREEN_BUFFER_INFO ci = { 0 };
	if (ok) ok = GetConsoleScreenBufferInfo(h, &ci);
	if (ok) ok = SetConsoleTextAttribute(h, attr);

	static const char kTip[] = "Run as admin to use \"-s\".";
	fprintf(stderr, "%s", kTip);

	if (ok) SetConsoleTextAttribute(h, ci.wAttributes);
	fprintf(stderr, "\n");
}

static __forceinline WORD
showDevice(size_t index, const wchar_t* inst, const wchar_t* desc) {
	CONSOLE_SCREEN_BUFFER_INFO ci = { 0 };
	BOOL ok = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);

	// Dont' use %zu. It crashes on older Windows.
	fprintf(stdout, "%u            %ls", (unsigned)index, inst); // Spaces are preserved to show schedule result.
	fprintf(stdout, "  %ls", desc);

	return ok ? ci.dwCursorPosition.Y : (WORD)-1;
}

static __forceinline void
showScheduleResult(bool isGood, WORD y) {
	static const char kGood[] = "Hidden";
	static const char kBad[] = "Failed";

	COORD coord;
	CONSOLE_SCREEN_BUFFER_INFO ci = { 0 };
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	BOOL ok = h != INVALID_HANDLE_VALUE;
	if (ok) ok = GetConsoleScreenBufferInfo(h, &ci);
	if (ok && y != (WORD)-1) {
		coord.X = 5;
		coord.Y = y;
		ok = SetConsoleCursorPosition(h, coord);
	}

	if (!ok) fprintf(stdout, "\n     "); // If stdout is redirected, print indented at the next line.
	if (isGood) {
		if (ok) SetConsoleTextAttribute(h, BACKGROUND_INTENSITY);
		fprintf(stdout, "%s", kGood);
	}
	else {
		if (ok) SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY);
		fprintf(stdout, "%s", kBad);
	}
	if (ok) SetConsoleTextAttribute(h, ci.wAttributes);
	if (ok && y != (WORD)-1) ok = SetConsoleCursorPosition(h, ci.dwCursorPosition);
}

static __forceinline void
showSummary(bool isToSchedule, size_t devCount, size_t schCount) {
	if (devCount) {
		if (!isToSchedule) {
			fprintf(stdout, "%s", "Use \"-s\" to hide.\n");
			return;
		}
		if (schCount != devCount) {
			fprintf(stderr, "%s", "All device(s) not hidden.\n");
		}
		if (schCount) fprintf(stdout, "%s", "Reboot to take effect.\n");
	}
	else {
		static const char kNoDevice[] =
			"No removable device.\n"
			"If your device is actually showing in the eject menu, eject it, but don't unplug, then re-run me.\n";
		fprintf(stdout, "%s", kNoDevice);
	}
}

static __forceinline bool
isProcessElevated(void)
{
	HANDLE h;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) return false;
	TOKEN_ELEVATION token = { 0 };
	GetTokenInformation(h, TokenElevation, &token, sizeof(token), &(DWORD){0});
	CloseHandle(h);
	return token.TokenIsElevated;
}

// Return NULL if the property doesn't exist or error occured.
static wchar_t*
allocDeviceStringProp(HDEVINFO infoSet, PSP_DEVINFO_DATA info, const DEVPROPKEY* key) {
	DEVPROPTYPE type;
	DWORD cb;
	SetupDiGetDevicePropertyW(infoSet, info, key, &type, NULL, 0, &cb, 0);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return NULL; // If the key dosen't exist, return NULL without any other action.

	wchar_t* prop = HeapAlloc(GetProcessHeap(), 0, cb);
	if (!prop) {
		fprintf(stderr, "Low memory.\n");
		return NULL;
	}
	if (!SetupDiGetDevicePropertyW(infoSet, info, key, &type, (BYTE*)prop, cb, NULL, 0)) {
		fprintf(stderr, "SetupDiGetDevicePropertyW() error 0x%X.\n", GetLastError());
		HeapFree(GetProcessHeap(), 0, prop);
		return NULL;
	}
	return prop;
}

// Return static buffer
// Take at most 2 out of the 4 parts, merge them into one string and return.
// NULL parts are ignored.
// If all parts are NULL, return NULL.
static const wchar_t*
mergeDeviceInfo(const wchar_t* const partv[4]) {
	static wchar_t buffer[256];

	size_t lenv[4];
	for (int i = 0; i < 4; ++i) {
		lenv[i] = partv[i] ? wcslen(partv[i]) : 0;
	}

	wchar_t* p = buffer;
	size_t cchR = sizeof(buffer) / sizeof(buffer[0]) - 1;
	size_t partCount = 0;
	for (int i = 0; i < 4; ++i) {
		if (!lenv[i]) continue;
		if (p != buffer) {
			*p++ = L' ';
			if (!--cchR) goto fin;
		}
		size_t cch = lenv[i] <= cchR ? lenv[i] : cchR;
		memcpy(p, partv[i], sizeof(wchar_t) * cch);
		p += cch;
		cchR -= cch;
		if (!cchR) goto fin;
		if (++partCount == 2) goto fin;
	}
fin:
	if (p == buffer) return NULL;
	*p = L'\0';
	return buffer;
}

// Retrun static buffer
// Device info is a brief description for human reading.
static __forceinline const wchar_t*
getDeviceInfo(HDEVINFO infoSet, PSP_DEVINFO_DATA info) {
	HANDLE heap = GetProcessHeap();
	wchar_t* name = allocDeviceStringProp(infoSet, info, &DEVPKEY_Device_FriendlyName);
	wchar_t* desc = allocDeviceStringProp(infoSet, info, &DEVPKEY_Device_DeviceDesc);
	wchar_t* manu = allocDeviceStringProp(infoSet, info, &DEVPKEY_Device_Manufacturer);
	wchar_t* p4 = allocDeviceStringProp(infoSet, info, &DEVPKEY_Device_LocationInfo);

	const wchar_t* const partv[4] = { name, desc, manu, p4 };
	const wchar_t* p = mergeDeviceInfo(partv);

	if (name) HeapFree(heap, 0, name);
	if (desc) HeapFree(heap, 0, desc);
	if (manu) HeapFree(heap, 0, manu);
	if (p4) HeapFree(heap, 0, p4);
	return p;
}

// Return static buffer
static const wchar_t*
getTaskParams(const wchar_t* instId, DWORD cap) {
	static const wchar_t kPat[] = 
		L"/c reg query \"HKLM\\SYSTEM\\CurrentControlSet\\Enum\\%ls\" /v Capabilities /t REG_DWORD"
		L" | findstr /i 0x%X"
		L" && reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Enum\\%ls\" /v Capabilities /t REG_DWORD /d 0x%X /f";
	static wchar_t buffer[ARRAYSIZE(kPat) + 400];

	// Since we bypass CRTStartup, we lost come string function compatibilities.
	// These function signatures vary on versions: swprintf(), swprintf_s(), _snwprintf_s().
	// Using them may work on some Windows, while crash on another.
	// So we use _snwprintf(). It's also compatibale to C11 snwprintf_s().
	const int cch = _snwprintf(buffer, ARRAYSIZE(buffer), kPat, instId, cap, instId, cap & ~CM_DEVCAP_REMOVABLE);
	if (cch <= 0 || cch >= ARRAYSIZE(buffer)) {
		fprintf(stderr, "%s", "Command parameters buffer too small.");
		return NULL;
	}
	return buffer;
}

// The task name is instance id with special characters replaced.
static __forceinline wchar_t*
allocTaskName(const wchar_t* instId) {
	const size_t cb = sizeof(wchar_t) * (wcslen(instId) + 1);
	wchar_t* name = HeapAlloc(GetProcessHeap(), 0, cb);
	if (!name) {
		fprintf(stderr, "Low memory.\n");
		return NULL;
	}

	memcpy(name, instId, cb);
	for (wchar_t* p = name; *p; ++p) {
		switch (*p) {
		case L'\\':
			*p = L'-';
			break;
		}
	}
	return name;
}

static __forceinline bool
schedule(const wchar_t* instId, DWORD cap, const wchar_t* taskDesc) {
	const wchar_t* params = getTaskParams(instId, cap);
	if (!params) return false;
	wchar_t* name = allocTaskName(instId);
	if (!name) return false;

	bool ok = false;
	ITaskDefinition* task = schd_createTask();
	if (!task) goto fin;
	ok = schd_taskSetUser(task, L"S-1-5-18"); // SYSTEM
	if (!ok) goto fin;
	ok = schd_taskAddAction(task, L"cmd.exe", params);
	if (!ok) goto fin;
	ok = schd_taskAddTriggerBoot(task);
	if (!ok) goto fin;
	ok = schd_taskClearSettings(task);
	// Don't care whether successful.
	if (taskDesc) {
		ok = schd_taskSetDesc(task, taskDesc);
		// Don't care whether successful.
	}
	ok = schd_registerTask(L"NoEject", name, task);
fin:
	HeapFree(GetProcessHeap(), 0, name);
	if (task) task->lpVtbl->Release(task);
	return ok;
}

void startup(void) {
	// Some device info may be localized. We better call setlocale().
	setlocale(LC_ALL, "");
	
	enum Intent intent = parseCmd();
	if (intent == kHelp) {
		showHelp();
		ExitProcess(0);
	}

	HDEVINFO infoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (infoSet == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "SetupDiGetClassDevsW() error 0x%X.\n", GetLastError());
		ExitProcess(1);
	}

	int rlt = 1;
	bool isToSchedule = intent == kSchedule;
	bool isElevated = isProcessElevated();

	if (isToSchedule) {
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr)) {
			fprintf(stderr, "CoInitializeEx() error 0x%X.\n", hr);
			goto fin;
		}
		if (!schd_init()) goto fin;
	}

	if (!isElevated) showElevationTip(isToSchedule);
	size_t devCount = 0;
	size_t schdCount = 0;
	SP_DEVINFO_DATA info = { sizeof(info) };
	HANDLE heap = GetProcessHeap();
	for (DWORD idx = 0; SetupDiEnumDeviceInfo(infoSet, idx, &info); ++idx) {
		DEVPROPTYPE type;
		DWORD cap;
		if (!SetupDiGetDevicePropertyW(infoSet, &info, &DEVPKEY_Device_Capabilities, &type, (BYTE*)&cap, sizeof(cap), NULL, 0)) continue;

		if (!(cap & CM_DEVCAP_REMOVABLE)) continue;

		if (cap & CM_DEVCAP_SURPRISEREMOVALOK) {
			DWORD problemCode = 0;
			SetupDiGetDevicePropertyW(infoSet, &info, &DEVPKEY_Device_ProblemCode, &type, (BYTE*)&problemCode, sizeof(problemCode), NULL, 0);
			if (problemCode != (CM_PROB_REENUMERATION | CM_PROB_DISABLED_SERVICE)) continue; // Ejected.
		}

		++devCount;
		wchar_t* instId = allocDeviceStringProp(infoSet, &info, &DEVPKEY_Device_InstanceId);
		if (!instId) goto fin;
		const wchar_t* deviceInfo = getDeviceInfo(infoSet, &info);
		WORD consoleY = showDevice(devCount, instId, deviceInfo);

		if (isToSchedule && isElevated) {
			bool ok = schedule(instId, cap, deviceInfo);
			if (ok) ++schdCount;
			showScheduleResult(ok, consoleY);
		}

		fprintf(stdout, "\n");
		HeapFree(heap, 0, instId);
	}
	showSummary(isToSchedule, devCount, schdCount);
	if (!isToSchedule || schdCount == devCount) rlt = 0;
fin:
	if (isToSchedule) {
		schd_uninit();
		CoUninitialize();
	}
	SetupDiDestroyDeviceInfoList(infoSet);
	ExitProcess((UINT)rlt);
}
