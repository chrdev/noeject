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
#include "dev.h"
#include "show.h"

#include <cfgmgr32.h>
#include <initguid.h> // Needed by Devpkey.h
#include <devpkey.h> // DEVPKEY_*, need initguid.h

#pragma comment(lib, "setupapi.lib")


enum {
	kCchInfoBuffer = 256,
};

enum dev_ApiType dev_apiType = dev_kApiType_1;


typedef BOOL(WINAPI* SetupDiGetDevicePropertyW_P)(
	_In_         HDEVINFO         DeviceInfoSet,
	_In_         PSP_DEVINFO_DATA DeviceInfoData,
	_In_   CONST DEVPROPKEY* PropertyKey,
	_Out_        DEVPROPTYPE* PropertyType,
	_Out_writes_bytes_to_opt_(PropertyBufferSize, *RequiredSize) PBYTE PropertyBuffer,
	_In_         DWORD            PropertyBufferSize,
	_Out_opt_    PDWORD           RequiredSize,
	_In_         DWORD            Flags
	);

static HINSTANCE lib_ = NULL;
static SetupDiGetDevicePropertyW_P SetupDiGetDevicePropertyW_ = NULL;


void dev_init(void)
{
	lib_ = LoadLibraryA("SetupAPI.dll");
	if (!lib_) return;
	SetupDiGetDevicePropertyW_ = (SetupDiGetDevicePropertyW_P)GetProcAddress(lib_, "SetupDiGetDevicePropertyW");
	if (!SetupDiGetDevicePropertyW_) return; // Expect user calls dev_uninit() later.

	dev_apiType = dev_kApiType_2;
}

void dev_uninit(void)
{
	if (lib_) FreeLibrary(lib_), lib_ = NULL;
}

static __forceinline DWORD
getRemovalCapabilities_1(DEVINST di) {
	ULONG cap;
	ULONG type;
	ULONG cb = sizeof(cap);
	CONFIGRET cr = CM_Get_DevNode_Registry_PropertyW(di, CM_DRP_CAPABILITIES, &type, &cap, &cb, 0);
	if (cr) return 0;

	if (!(cap & CM_DEVCAP_REMOVABLE)) return 0;

	if (cap & CM_DEVCAP_SURPRISEREMOVALOK) {
		ULONG status = 0;
		ULONG probCode = 0;
		cr = CM_Get_DevNode_Status(&status, &probCode, di, 0);
		if (cr) {
			show_cant("get", "device status", cr);
			return 0;
		}
		if (probCode != CM_PROB_HELD_FOR_EJECT) return 0;
	}
	return (DWORD)cap;
}

static __forceinline DWORD
getRemovalCapabilities_2(HDEVINFO infoSet, PSP_DEVINFO_DATA info) {
	DWORD cap;
	DEVPROPTYPE type;
	if (!SetupDiGetDevicePropertyW_(infoSet, info, &DEVPKEY_Device_Capabilities, &type, (BYTE*)&cap, sizeof(cap), NULL, 0)) return 0;

	if (!(cap & CM_DEVCAP_REMOVABLE)) return 0;

	if (cap & CM_DEVCAP_SURPRISEREMOVALOK) {
		DWORD probCode = 0;
		if (!SetupDiGetDevicePropertyW_(infoSet, info, &DEVPKEY_Device_ProblemCode, &type, (BYTE*)&probCode, sizeof(probCode), NULL, 0)) {
			show_cant("get", "device status", GetLastError());
			return 0;
		}
		if (probCode != CM_PROB_HELD_FOR_EJECT) return 0;
	}
	return cap;
}

DWORD dev_getRemovalCapabilities(HDEVINFO infoSet, PSP_DEVINFO_DATA info)
{
	switch (dev_apiType) {
	case dev_kApiType_1:
		return getRemovalCapabilities_1(info->DevInst);
	case dev_kApiType_2:
		return getRemovalCapabilities_2(infoSet, info);
	default:
		return 0U;
	}
}

static __forceinline wchar_t*
allocStringProperty_1(DWORD devInst, ULONG prop) {
	ULONG type;
	ULONG cb = 0;
	CONFIGRET cret = CM_Get_DevNode_Registry_PropertyW(devInst, prop, &type, NULL, &cb, 0);
	if (!cb) return NULL; // No need to check type, for now.
	wchar_t* p = HeapAlloc(GetProcessHeap(), 0, cb);
	if (!p) {
		show_lowMem();
		return NULL;
	}
	cret = CM_Get_DevNode_Registry_PropertyW(devInst, prop, &type, p, &cb, 0);
	if (cret) {
		HeapFree(GetProcessHeap(), 0, p);
		return NULL;
	}
	return p;
}

static __forceinline wchar_t*
allocStringProperty_2(HDEVINFO infoSet, PSP_DEVINFO_DATA info, const DEVPROPKEY* key) {
	DEVPROPTYPE type;
	DWORD cb;
	SetupDiGetDevicePropertyW_(infoSet, info, key, &type, NULL, 0, &cb, 0);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return NULL; // If the key dosen't exist or error occured, return NULL without further action.

	wchar_t* p = HeapAlloc(GetProcessHeap(), 0, cb);
	if (!p) {
		show_lowMem();
		return NULL;
	}
	if (!SetupDiGetDevicePropertyW_(infoSet, info, key, &type, (BYTE*)p, cb, NULL, 0)) {
		show_code("SetupDiGetDevicePropertyW()", GetLastError());
		HeapFree(GetProcessHeap(), 0, p);
		return NULL;
	}
	return p;
}

wchar_t* dev_allocStringProperty(HDEVINFO infoSet, PSP_DEVINFO_DATA info, DWORD prop)
{
	switch (dev_apiType) {
	case dev_kApiType_1:
		return allocStringProperty_1(info->DevInst, prop);
	case dev_kApiType_2:
	{
		const DEVPROPKEY* key = NULL;
		switch (prop) {
		case CM_DRP_FRIENDLYNAME: key = &DEVPKEY_Device_FriendlyName; break;
		case CM_DRP_DEVICEDESC: key = &DEVPKEY_Device_DeviceDesc; break;
		case CM_DRP_MFG: key = &DEVPKEY_Device_Manufacturer; break;
		case CM_DRP_LOCATION_INFORMATION: key = &DEVPKEY_Device_LocationInfo; break;
		}
		return key ? allocStringProperty_2(infoSet, info, key) : NULL;
	}
	default:
		return NULL;
	}
}

static __forceinline wchar_t*
allocInstId_1(DWORD devInst) {
	ULONG cch;
	CONFIGRET cret = CM_Get_Device_ID_Size(&cch, devInst, 0);
	if (cret) return NULL;
	++cch;
	wchar_t* p = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * cch);
	if (!p) {
		show_lowMem();
		return NULL;
	}
	cret = CM_Get_Device_IDW(devInst, p, cch, 0);
	if (cret) {
		show_cant("get", "device instance id", cret);
		return NULL;
	}
	return p;
}

static __forceinline wchar_t*
allocInstId_2(HDEVINFO infoSet, PSP_DEVINFO_DATA info) {
	return allocStringProperty_2(infoSet, info, &DEVPKEY_Device_InstanceId);
}

wchar_t* dev_allocInstId(HDEVINFO infoSet, PSP_DEVINFO_DATA info)
{
	switch (dev_apiType) {
	case dev_kApiType_1:
		return allocInstId_1(info->DevInst);
	case dev_kApiType_2:
		return allocInstId_2(infoSet, info);
	default:
		return NULL;
	}
}

static __forceinline void
getInfoPartV_1(wchar_t* partV[4], DWORD devInst) {
	partV[0] = allocStringProperty_1(devInst, CM_DRP_FRIENDLYNAME);
	partV[1] = allocStringProperty_1(devInst, CM_DRP_DEVICEDESC);
	partV[2] = allocStringProperty_1(devInst, CM_DRP_MFG);
	partV[3] = allocStringProperty_1(devInst, CM_DRP_LOCATION_INFORMATION);
}

static __forceinline void
getInfoPartV_2(wchar_t* partV[4], HDEVINFO infoSet, PSP_DEVINFO_DATA info) {
	partV[0] = allocStringProperty_2(infoSet, info, &DEVPKEY_Device_FriendlyName);
	partV[1] = allocStringProperty_2(infoSet, info, &DEVPKEY_Device_DeviceDesc);
	partV[2] = allocStringProperty_2(infoSet, info, &DEVPKEY_Device_Manufacturer);
	partV[3] = allocStringProperty_2(infoSet, info, &DEVPKEY_Device_LocationInfo);
}

// Merge at most 2 out of the 4 parts into one string.
// NULL parts are ignored.
// If all parts are NULL, return NULL.
static const wchar_t*
mergeDescription(wchar_t buffer[kCchInfoBuffer], const wchar_t* const partV[4]) {
	size_t lenV[4];
	for (int i = 0; i < 4; ++i) {
		lenV[i] = partV[i] ? wcslen(partV[i]) : 0;
	}

	wchar_t* p = buffer;
	size_t cchR = kCchInfoBuffer - 1;
	size_t partCount = 0;
	for (int i = 0; i < 4; ++i) {
		if (!lenV[i]) continue;
		if (p != buffer) {
			*p++ = L' ';
			if (!--cchR) goto fin;
		}
		size_t cch = lenV[i] <= cchR ? lenV[i] : cchR;
		memcpy(p, partV[i], sizeof(wchar_t) * cch);
		p += cch;
		cchR -= cch;
		if (!cchR) goto fin;
		if (++partCount == 2) goto fin;
	}
fin:
	*p = L'\0';
	if (p == buffer) return NULL;
	return buffer;
}

const wchar_t* dev_getDescription(HDEVINFO infoSet, PSP_DEVINFO_DATA info)
{
	static wchar_t buffer[kCchInfoBuffer];
	wchar_t* partV[4] = { 0 };

	switch (dev_apiType) {
	case dev_kApiType_1:
		getInfoPartV_1(partV, info->DevInst);
		break;
	case dev_kApiType_2:
		getInfoPartV_2(partV, infoSet, info);
		break;
	default:
		return NULL;
	}
	const wchar_t* p = mergeDescription(buffer, partV);

	HANDLE heap = GetProcessHeap();
	for (int i = 0; i < ARRAYSIZE(partV); ++i) {
		if (partV[i]) HeapFree(heap, 0, partV[i]);
	}
	
	return p;
}
