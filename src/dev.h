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
// If a device has one of the following features, we consider it removable:
// 1. It is CM_DEVCAP_REMOVABLE, but not CM_DEVCAP_SURPRISEREMOVALOK.
// 2. It is both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK, and ejected but not unplugped.
//
// Case 2 explanation:
// It's been observed, for a certain monitor, which has built-in devices A, B, and C, that A appears in the eject menu.
// Device A is both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK. Schedule to hide it doesn't work.
// Device B is A's parent device, It's both CM_DEVCAP_REMOVABLE and CM_DEVCAP_SURPRISEREMOVALOK.
// Schedule to hide device B alone, or simultaneously with A, doesn't work.
// If we click the eject menu to eject device A, A will disappear from the menu as expected.
// But through Device Manager, we can see A is still there intact.
// At the same time, we see device C ejected. C is a seemingly irrelevant device.
// Schedule to hide device C alone actually hide device A.
//
// So we can ask the user to eject a device, but not unplug it, to find the right device to hide.
//
#pragma once

#include <Windows.h>
#include <SetupAPI.h>


enum dev_ApiType {
	dev_kApiType_1, // Use CM_Get_DevNode_Registry_PropertyW, XP and earlier
	dev_kApiType_2, // Use SetupDiGetDevicePropertW. Vista and later
};

enum dev_ApiType dev_apiType;

// Should always call dev_uninit().
void dev_init(void);

// Always harmless to call.
void dev_uninit(void);

// If the device doesn't seem removable, return 0.
DWORD dev_getRemovalCapabilities(HDEVINFO infoSet, PSP_DEVINFO_DATA info);

// Use HeapFree() against the return value.
// Return NULL if the property doesn't exist or error occured.
wchar_t* dev_allocInstId(HDEVINFO infoSet, PSP_DEVINFO_DATA info);

// Param prop is CM_DRP_* in cfgmgr32.h
// Use HeapFree() against the return value.
// Return NULL if the property doesn't exist or error occured.
wchar_t* dev_allocStringProperty(HDEVINFO infoSet, PSP_DEVINFO_DATA info, DWORD prop);

// Device description is combined device information for human reading.
// It includes, but not limited to DEVPKEY_Device_DeviceDesc / CM_DRP_DEVICEDESC.
// Retrun static buffer
const wchar_t* dev_getDescription(HDEVINFO infoSet, PSP_DEVINFO_DATA info);
