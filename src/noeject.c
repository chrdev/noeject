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
//
#define MY_VER "1.1.0"

#include "dev.h"
#include "sch.h"
#include "show.h"

#include <cfgmgr32.h> // MAX_DEVICE_ID_LEN

#include <stdbool.h>

#ifdef MY_MSVCRT_BUILD
#include "myvcrt.h"
#else
#include <locale.h>
#include <stdio.h>
#endif

#pragma comment(lib, "shell32.lib") // CommandLineToArgvW()
#pragma comment(lib, "ole32.lib") // CoInitializeEx()
#pragma comment(lib, "advapi32.lib") // OpenProcessToken()


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
		"For Windows 11 back to Windows 2000.\n"
		"    -[?|h]              Show this help.\n"
		"    -s                  Save the hiding.\n"
		"Typical usage steps:\n"
		"    noeject.exe         1. List removable devices.\n"
		"    noeject.exe -s      2. Hide removable devices.\n"
		"Unhide:\n"
		"    Use Task Scheduler, delete tasks in NoEject sub-folder.\n"
		"    (Windows XP and 2000 don't have sub-folders.)\n"
		"Tips:\n"
		"    If the device is not listed, but showing in the eject menu,\n"
		"    eject the device but don't unplug, then re-run noeject.\n";

	fprintf(stdout, "%s", kHelp);
	fflush(stdout);
}

static __forceinline bool
isProcessElevated(void) {
	HANDLE h;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) return false;
	TOKEN_ELEVATION token = { 0 };
	BOOL ok = GetTokenInformation(h, TokenElevation, &token, sizeof(token), &(DWORD){0});
	CloseHandle(h);
	if (!ok) {
		return GetLastError() == ERROR_INVALID_PARAMETER;
	}
	return token.TokenIsElevated;
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
	fprintf(stdout, "%-13u%ls", (unsigned)index, inst);
	fprintf(stdout, "  %ls", desc);

	return ok ? ci.dwCursorPosition.Y : (WORD)-1;
}

static __forceinline void
showScheduleResult(bool isGood, WORD y) {
	static const char kGood[] = "Hidden";
	static const char kBad[] = "Failed";

	CONSOLE_SCREEN_BUFFER_INFO ci = { 0 };
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	BOOL ok = h != INVALID_HANDLE_VALUE;
	if (ok) ok = GetConsoleScreenBufferInfo(h, &ci);
	if (ok && y != (WORD)-1) {
		ok = SetConsoleCursorPosition(h, (COORD){5, y});
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
			fprintf(stderr, "%s", "\nAll device(s) not hidden.\n");
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

// The task name is instance id with special characters replaced.
// Retrun static buffer
static const wchar_t*
getTaskName(const wchar_t* instId) {
	static wchar_t buffer[MAX_DEVICE_ID_LEN];
	const size_t cch = wcslen(instId) + 1;
	if (cch > ARRAYSIZE(buffer)) {
		fprintf(stderr, "%s", "\nBad instId.\n");
		return NULL;
	}
	memcpy(buffer, instId, sizeof(wchar_t) * cch);
	for (wchar_t* p = buffer; *p; ++p) {
		switch (*p) {
		case L'\\':
			*p = L'#';
			break;
		}
	}
	return buffer;
}

// Return static buffer
static const wchar_t*
getTaskArg(const wchar_t* instId, DWORD cap) {
	static const wchar_t kPat[] = 
		L"/c reg query \"HKLM\\SYSTEM\\CurrentControlSet\\Enum\\%ls\" /v Capabilities" // Avoid "/t REG_DWORD" to support earlier versions of reg.exe.
		L" | findstr /I /E 0x%X"
		L" && reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Enum\\%ls\" /v Capabilities /t REG_DWORD /d 0x%X /f";
	static wchar_t buffer[ARRAYSIZE(kPat) + MAX_DEVICE_ID_LEN + MAX_DEVICE_ID_LEN + 17]; // 17 is mealy to make the buffer size a whole number, for no good reason.

	// Since we bypass CRTStartup, we lose some string function compatibilities.
	// These function signatures vary depending on versions: swprintf(), swprintf_s(), _snwprintf_s().
	// Using them may work on some Windows, while crashing on another.
	// So we use _snwprintf(). It's also compatibale to C11 snwprintf_s().
	const int cch = _snwprintf(buffer, ARRAYSIZE(buffer), kPat, instId, cap, instId, cap & ~CM_DEVCAP_REMOVABLE);
	if (cch <= 0 || cch >= ARRAYSIZE(buffer)) {
		fprintf(stderr, "%s", "\nBad instId.\n");
		return NULL;
	}
	return buffer;
}

static __forceinline bool
schedule(const wchar_t* instId, DWORD cap, const wchar_t* desc) {
	const wchar_t* name = getTaskName(instId);
	if (!name) return false;
	const wchar_t* arg = getTaskArg(instId, cap);
	if (!arg) return false;

	return sch_createTask(name, desc, L"cmd.exe", arg);
}

int startup(void) {
#ifdef MY_MSVCRT_BUILD
	__set_app_type(_CONSOLE_APP); // If msvcrt has its own error, report on console.
#endif
	setlocale(LC_ALL, "");  // Some device info may be localized. We better call setlocale().
	
	enum Intent intent = parseCmd();
	if (intent == kHelp) {
		showHelp();
		return 0;
	}
	
	// Vista is a API milestone for both device and scheduler.
	// But we handle device and scheduler separately in case their paces diverse in the future.
	dev_init();
	HDEVINFO infoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (infoSet == INVALID_HANDLE_VALUE) {
		show_code("SetupDiGetClassDevsW()", GetLastError());
		return 1;
	}

	int rlt = 1;
	bool isToSchedule = intent == kSchedule;
	bool isElevated = isProcessElevated();

	if (isToSchedule) {
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr)) {
			show_code("CoInitializeEx()", hr);
			goto fin;
		}
		if (!sch_init()) goto fin;
	}

	if (!isElevated) showElevationTip(isToSchedule);
	size_t devCount = 0;
	size_t schdCount = 0;
	SP_DEVINFO_DATA info = { sizeof(info) };
	for (DWORD idx = 0; SetupDiEnumDeviceInfo(infoSet, idx, &info); ++idx) {
		DWORD cap = dev_getRemovalCapabilities(infoSet, &info);
		if (!cap) continue;
		++devCount;

		wchar_t* instId = dev_allocInstId(infoSet, &info);
		if (!instId) {
			show_cant("get", "device instance Id", GetLastError());
			continue;
		}
		const wchar_t* desc = dev_getDescription(infoSet, &info);
		WORD consoleY = showDevice(devCount, instId, desc);

		if (isToSchedule && isElevated) {
			bool ok = schedule(instId, cap, desc);
			if (ok) ++schdCount;
			showScheduleResult(ok, consoleY);
		}

		HeapFree(GetProcessHeap(), 0, instId);

		fprintf(stdout, "\n");
	}
	showSummary(isToSchedule, devCount, schdCount);
	fflush(NULL); // If redirected, earlier Windows need this.

	if (!isToSchedule || schdCount == devCount) rlt = 0;
fin:
	if (isToSchedule) {
		sch_uninit();
		CoUninitialize();
	}
	SetupDiDestroyDeviceInfoList(infoSet);
	dev_uninit();
	ExitProcess((UINT)rlt); // A simple return may stuck on earlier Windows. Need ExitProcess().
}
