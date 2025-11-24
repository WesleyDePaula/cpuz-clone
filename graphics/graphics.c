// graphics.c - implementations for GPU and VRAM information retrieval
//
// This module attempts to gather information about the system's primary
// graphics adapter.  It first tries to obtain detailed statistics via
// the NVIDIA Management Library (NVML) when a supported NVIDIA card is
// detected.  When NVML is not present or when the adapter is not
// NVIDIA, the code falls back to using WMI (Win32_VideoController) to
// obtain basic information such as the adapter name and total memory.
// The functions declared in graphics.h return their results as ANSI
// strings and indicate success or failure with a boolean return.

#define COBJMACROS

#include "graphics.h"

#include <windows.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#pragma comment(lib, "wbemuuid.lib")

// -----------------------------------------------------------------------------
//  NVML dynamic loader definitions
//
// To avoid a direct dependency on NVML at build time the functions are
// resolved at runtime via LoadLibrary and GetProcAddress.  Only a subset
// of NVML APIs are declared here sufficient to query name, power limit,
// clock speeds, memory size and bus width.  If any symbol cannot be
// resolved, NVML support will be disabled and the code will fall back to
// WMI.

typedef int nvmlReturn_t;
typedef void* nvmlDevice_t;

// Clocks domains used by nvmlDeviceGetMaxClockInfo.  Use graphics domain
// because it most closely reflects base GPU clock.  These values are
// defined by NVML but duplicated here to avoid including the header.
#define NVML_CLOCK_GRAPHICS 0

typedef struct nvmlMemory_st {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef struct nvmlPciInfo_st {
    char busId[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
    unsigned char reserved0[16];
    unsigned char reserved1[16];
} nvmlPciInfo_t;

// Function pointer typedefs matching NVML prototypes.
typedef nvmlReturn_t (*nvmlInitFunc)(void);
typedef nvmlReturn_t (*nvmlShutdownFunc)(void);
typedef nvmlReturn_t (*nvmlDeviceGetCountFunc)(unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndexFunc)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*nvmlDeviceGetNameFunc)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (*nvmlDeviceGetPowerManagementLimitConstraintsFunc)(nvmlDevice_t, unsigned long long*, unsigned long long*);
typedef nvmlReturn_t (*nvmlDeviceGetMaxClockInfoFunc)(nvmlDevice_t, int, unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfoFunc)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryBusWidthFunc)(nvmlDevice_t, unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetPciInfoFunc)(nvmlDevice_t, nvmlPciInfo_t*);

// Attempt to load NVML and query the first GPU.  On success a handle to the
// loaded library and the device handle are returned.  On failure libOut is
// NULL and deviceOut is untouched.
static bool try_nvml(nvmlDevice_t *deviceOut, HMODULE *libOut,
                     nvmlInitFunc *pInit, nvmlShutdownFunc *pShutdown,
                     nvmlDeviceGetCountFunc *pGetCount,
                     nvmlDeviceGetHandleByIndexFunc *pGetHandle,
                     nvmlDeviceGetNameFunc *pGetName,
                     nvmlDeviceGetPowerManagementLimitConstraintsFunc *pGetPowerLimits,
                     nvmlDeviceGetMaxClockInfoFunc *pGetMaxClock,
                     nvmlDeviceGetMemoryInfoFunc *pGetMemInfo,
                     nvmlDeviceGetMemoryBusWidthFunc *pGetBusWidth,
                     nvmlDeviceGetPciInfoFunc *pGetPciInfo)
{
    if (!deviceOut || !libOut) return false;
    *deviceOut = NULL;
    *libOut = NULL;

    // 1) Tenta pelo nome da DLL no PATH / System32
    const char *nvmlNames[] = { "nvml.dll", "nvml64.dll", NULL };
    HMODULE h = NULL;
    for (int i = 0; nvmlNames[i] != NULL && !h; ++i) {
        h = LoadLibraryA(nvmlNames[i]);
    }

    // 2) Se não deu, tenta caminhos padrão de instalação do driver:
    //    %ProgramW6432%\NVIDIA Corporation\NVSMI\nvml.dll
    //    %ProgramFiles%\NVIDIA Corporation\NVSMI\nvml.dll
    //    %ProgramFiles(x86)%\NVIDIA Corporation\NVSMI\nvml.dll
    if (!h) {
        char path[MAX_PATH];
        const char *envs[] = { "ProgramW6432", "ProgramFiles", "ProgramFiles(x86)", NULL };

        for (int i = 0; envs[i] != NULL && !h; ++i) {
            DWORD len = GetEnvironmentVariableA(envs[i], path, (DWORD)sizeof(path));
            if (len > 0 && len < sizeof(path)) {
                // Remove barra final, se houver
                if (path[len - 1] == '\\' || path[len - 1] == '/') {
                    path[len - 1] = '\0';
                }
                strncat(path, "\\NVIDIA Corporation\\NVSMI\\nvml.dll",
                        sizeof(path) - strlen(path) - 1);
                h = LoadLibraryA(path);
            }
        }
    }

    if (!h) {
        return false;
    }

    // Resolve as funções NVML necessárias
    *pInit     = (nvmlInitFunc)GetProcAddress(h, "nvmlInit");
    if (!*pInit) *pInit = (nvmlInitFunc)GetProcAddress(h, "nvmlInit_v2");

    *pShutdown = (nvmlShutdownFunc)GetProcAddress(h, "nvmlShutdown");
    *pGetCount = (nvmlDeviceGetCountFunc)GetProcAddress(h, "nvmlDeviceGetCount");

    *pGetHandle = (nvmlDeviceGetHandleByIndexFunc)GetProcAddress(h, "nvmlDeviceGetHandleByIndex");
    if (!*pGetHandle) {
        *pGetHandle = (nvmlDeviceGetHandleByIndexFunc)GetProcAddress(h, "nvmlDeviceGetHandleByIndex_v2");
    }

    *pGetName       = (nvmlDeviceGetNameFunc)GetProcAddress(h, "nvmlDeviceGetName");
    *pGetPowerLimits= (nvmlDeviceGetPowerManagementLimitConstraintsFunc)
                      GetProcAddress(h, "nvmlDeviceGetPowerManagementLimitConstraints");
    *pGetMaxClock   = (nvmlDeviceGetMaxClockInfoFunc)
                      GetProcAddress(h, "nvmlDeviceGetMaxClockInfo");
    *pGetMemInfo    = (nvmlDeviceGetMemoryInfoFunc)
                      GetProcAddress(h, "nvmlDeviceGetMemoryInfo");
    *pGetBusWidth   = (nvmlDeviceGetMemoryBusWidthFunc)
                      GetProcAddress(h, "nvmlDeviceGetMemoryBusWidth");
    *pGetPciInfo    = (nvmlDeviceGetPciInfoFunc)
                      GetProcAddress(h, "nvmlDeviceGetPciInfo");
    if (!*pGetPciInfo) {
        *pGetPciInfo = (nvmlDeviceGetPciInfoFunc)GetProcAddress(h, "nvmlDeviceGetPciInfo_v2");
    }

    if (!*pInit || !*pShutdown || !*pGetCount || !*pGetHandle || !*pGetName ||
        !*pGetPowerLimits || !*pGetMaxClock || !*pGetMemInfo || !*pGetBusWidth || !*pGetPciInfo) {
        FreeLibrary(h);
        return false;
    }

    if ((*pInit)() != 0) {
        FreeLibrary(h);
        return false;
    }

    unsigned int count = 0;
    if ((*pGetCount)(&count) != 0 || count == 0) {
        (*pShutdown)();
        FreeLibrary(h);
        return false;
    }

    nvmlDevice_t dev = NULL;
    if ((*pGetHandle)(0, &dev) != 0 || !dev) {
        (*pShutdown)();
        FreeLibrary(h);
        return false;
    }

    *deviceOut = dev;
    *libOut = h;
    return true;
}


// Helper to unload NVML gracefully
static void unload_nvml(HMODULE h, nvmlShutdownFunc pShutdown) {
    if (pShutdown) {
        pShutdown();
    }
    if (h) {
        FreeLibrary(h);
    }
}

// -----------------------------------------------------------------------------
//  WMI helper routines

// Initialise COM and connect to the ROOT\CIMV2 namespace, returning an IWbemServices
// pointer when successful.  The caller is responsible for releasing the
// returned object and calling CoUninitialize().
static bool init_wmi(IWbemServices **pSvcOut) {
    if (!pSvcOut) return false;
    *pSvcOut = NULL;
    HRESULT hr;
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        CoUninitialize();
        return false;
    }
    IWbemLocator *pLoc = NULL;
    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (LPVOID *)&pLoc);
    if (FAILED(hr) || !pLoc) {
        CoUninitialize();
        return false;
    }
    IWbemServices *pSvc = NULL;
    hr = IWbemLocator_ConnectServer(pLoc, L"ROOT\\CIMV2", NULL, NULL, 0, 0, NULL, NULL, &pSvc);
    IWbemLocator_Release(pLoc);
    if (FAILED(hr) || !pSvc) {
        CoUninitialize();
        return false;
    }
    hr = CoSetProxyBlanket((IUnknown*)pSvc,
                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr)) {
        IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }
    *pSvcOut = pSvc;
    return true;
}

// -----------------------------------------------------------------------------
//  Helper: map PCI subsystem vendor IDs to human readable board partner names
//
// This table covers a handful of common board partners.  If your GPU uses a
// board from a vendor not listed here the mapping will return NULL and the
// caller should handle the unknown case.
struct vendor_map_entry { unsigned int id; const char *name; };
static const struct vendor_map_entry vendor_map[] = {
    { 0x10DE, "NVIDIA" },      // NVIDIA reference / Founders Edition
    { 0x1043, "ASUS" },
    { 0x1458, "Gigabyte" },
    { 0x1462, "MSI" },
    { 0x196E, "PNY" },
    { 0x3842, "EVGA" },
    { 0x19DA, "Zotac" },
    { 0x1DA2, "Palit" },
    { 0x1787, "Sapphire" },
    { 0x17AA, "Lenovo" },
    { 0, NULL }
};

static const char *lookup_vendor(unsigned int subSystemVendorId) {
    for (int i = 0; vendor_map[i].name != NULL; ++i) {
        if (vendor_map[i].id == subSystemVendorId) {
            return vendor_map[i].name;
        }
    }
    return NULL;
}

// Converte um dígito hexa wide para valor 0..15 ou -1 em caso de erro
static int hex_val_w(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return 10 + (ch - L'a');
    if (ch >= L'A' && ch <= L'F') return 10 + (ch - L'A');
    return -1;
}

// Pega o n(4) do padrão SUBSYS_s(4)n(4) do PNPDeviceID
// Ex.: PCI\VEN_10DE&DEV_2489&SUBSYS_2489196E -> retorna 0x196E
static unsigned int parse_subvendor_from_pnpid(const wchar_t *pnpId)
{
    if (!pnpId) return 0;

    // Ver docs da MS: PCI\VEN_v(4)&DEV_d(4)&SUBSYS_s(4)n(4)&REV_r(2)
    // https://learn.microsoft.com/.../identifiers-for-pci-devices :contentReference[oaicite:2]{index=2}
    const wchar_t *sub = wcsstr(pnpId, L"SUBSYS_");
    if (!sub) return 0;

    sub += 7; // pula "SUBSYS_"

    // Esperamos 8 hex: ssss nnnn
    unsigned int subsys = 0;
    for (int i = 0; i < 8; ++i) {
        wchar_t ch = sub[i];
        int hv = hex_val_w(ch);
        if (hv < 0) {
            return 0; // formato inesperado
        }
        subsys = (subsys << 4) | (unsigned int)hv;
    }

    // n(4) = últimos 16 bits -> subsystem vendor ID
    return subsys & 0xFFFFu;
}


// -----------------------------------------------------------------------------
//  Public API implementations

bool get_gpu_name(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;

    // Tenta NVML primeiro
    nvmlDevice_t dev = NULL;
    HMODULE lib = NULL;
    nvmlInitFunc nvInit = NULL;
    nvmlShutdownFunc nvShutdown = NULL;
    nvmlDeviceGetCountFunc nvGetCount = NULL;
    nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
    nvmlDeviceGetNameFunc nvGetName = NULL;
    nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
    nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
    nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
    nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
    nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

    if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                 &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                 &nvGetBusWidth, &nvGetPciInfo)) {
        char nameBuf[128] = {0};
        if (nvGetName && nvGetName(dev, nameBuf, sizeof(nameBuf)) == 0) {
            unload_nvml(lib, nvShutdown);
            snprintf(buf, buf_size, "%s", nameBuf);
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }

    // Fallback WMI: escolher o controlador PCI com maior AdapterRAM
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }

    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(
        pSvc, L"WQL",
        L"SELECT Name,AdapterRAM,PNPDeviceID FROM Win32_VideoController",
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnum);

    if (FAILED(hr) || !pEnum) {
        if (pSvc) IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }

    ULONGLONG bestRam = 0;
    BOOL bestIsPci = FALSE;
    bool haveBest = false;
    char bestName[256] = {0};

    ULONG uReturn = 0;
    IWbemClassObject *pObj = NULL;

    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uReturn)) {
        VARIANT vtName, vtRam, vtPnp;
        VariantInit(&vtName);
        VariantInit(&vtRam);
        VariantInit(&vtPnp);

        HRESULT hrName = IWbemClassObject_Get(pObj, L"Name", 0, &vtName, NULL, NULL);
        HRESULT hrRam  = IWbemClassObject_Get(pObj, L"AdapterRAM", 0, &vtRam, NULL, NULL);
        HRESULT hrPnp  = IWbemClassObject_Get(pObj, L"PNPDeviceID", 0, &vtPnp, NULL, NULL);

        ULONGLONG ramBytes = 0;
        if (SUCCEEDED(hrRam) &&
            (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4 || vtRam.vt == VT_I8 || vtRam.vt == VT_UI8)) {
            if (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4) {
                ramBytes = (ULONGLONG)vtRam.uintVal;
            } else {
                ramBytes = (ULONGLONG)vtRam.ullVal;
            }
        }

        BOOL isPci = FALSE;
        if (SUCCEEDED(hrPnp) && vtPnp.vt == VT_BSTR && vtPnp.bstrVal) {
            if (wcsstr(vtPnp.bstrVal, L"PCI\\") != NULL) {
                isPci = TRUE;
            }
        }

        if (SUCCEEDED(hrName) && vtName.vt == VT_BSTR && vtName.bstrVal) {
            bool better =
                !haveBest ||
                (isPci && !bestIsPci) ||
                (isPci == bestIsPci && ramBytes > bestRam);

            if (better) {
                char tmp[256] = {0};
                int len = WideCharToMultiByte(CP_ACP, 0, vtName.bstrVal, -1,
                                              tmp, (int)sizeof(tmp),
                                              NULL, NULL);
                if (len > 0) {
                    snprintf(bestName, sizeof(bestName), "%s", tmp);
                    bestRam = ramBytes;
                    bestIsPci = isPci;
                    haveBest = true;
                }
            }
        }

        VariantClear(&vtName);
        VariantClear(&vtRam);
        VariantClear(&vtPnp);
        IWbemClassObject_Release(pObj);
    }

    if (pEnum) IEnumWbemClassObject_Release(pEnum);
    if (pSvc) IWbemServices_Release(pSvc);
    CoUninitialize();

    if (!haveBest) {
        return false;
    }

    snprintf(buf, buf_size, "%s", bestName);
    return true;
}


bool get_gpu_board_manufacturer(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return false;

    const char *nvmlVendor = NULL;

    // --- 1) Tenta NVML primeiro (pciSubSystemId -> subsystem vendor) ---
    {
        nvmlDevice_t dev = NULL;
        HMODULE lib = NULL;
        nvmlInitFunc nvInit = NULL;
        nvmlShutdownFunc nvShutdown = NULL;
        nvmlDeviceGetCountFunc nvGetCount = NULL;
        nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
        nvmlDeviceGetNameFunc nvGetName = NULL;
        nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
        nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
        nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
        nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
        nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

        if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                     &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                     &nvGetBusWidth, &nvGetPciInfo)) {

            nvmlPciInfo_t pci = {0};
            if (nvGetPciInfo && nvGetPciInfo(dev, &pci) == 0) {
                unsigned int subVid = (pci.pciSubSystemId >> 16) & 0xFFFFu;
                const char *v = lookup_vendor(subVid);
                if (v) {
                    nvmlVendor = v; // guarda pra usar se o WMI não der nada melhor
                }
            }

            unload_nvml(lib, nvShutdown);
        }
    }

    // --- 2) WMI: escolher o adaptador PCI com maior VRAM e pegar SUBSYS_s(4)n(4) do PNPDeviceID ---
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        // Sem WMI: se NVML conseguiu algo, usa; senão falha
        if (nvmlVendor) {
            snprintf(buf, buf_size, "%s", nvmlVendor);
            return true;
        }
        return false;
    }

    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(
        pSvc,
        L"WQL",
        L"SELECT AdapterCompatibility,AdapterRAM,PNPDeviceID FROM Win32_VideoController",
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnum
    );

    if (FAILED(hr) || !pEnum) {
        if (pSvc) IWbemServices_Release(pSvc);
        CoUninitialize();
        if (nvmlVendor) {
            snprintf(buf, buf_size, "%s", nvmlVendor);
            return true;
        }
        return false;
    }

    ULONGLONG bestRam = 0;
    BOOL bestIsPci = FALSE;
    bool haveBest = false;
    unsigned int bestSubVendor = 0;
    char bestCompat[256] = {0};

    ULONG uReturn = 0;
    IWbemClassObject *pObj = NULL;

    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uReturn)) {
        VARIANT vtCompat, vtRam, vtPnp;
        VariantInit(&vtCompat);
        VariantInit(&vtRam);
        VariantInit(&vtPnp);

        HRESULT hrCompat = IWbemClassObject_Get(pObj, L"AdapterCompatibility", 0, &vtCompat, NULL, NULL);
        HRESULT hrRam    = IWbemClassObject_Get(pObj, L"AdapterRAM",         0, &vtRam,    NULL, NULL);
        HRESULT hrPnp    = IWbemClassObject_Get(pObj, L"PNPDeviceID",        0, &vtPnp,    NULL, NULL);

        ULONGLONG ramBytes = 0;
        if (SUCCEEDED(hrRam) &&
            (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4 || vtRam.vt == VT_I8 || vtRam.vt == VT_UI8)) {
            if (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4)
                ramBytes = (ULONGLONG)vtRam.uintVal;
            else
                ramBytes = (ULONGLONG)vtRam.ullVal;
        }

        BOOL isPci = FALSE;
        unsigned int subVendor = 0;
        if (SUCCEEDED(hrPnp) && vtPnp.vt == VT_BSTR && vtPnp.bstrVal) {
            if (wcsstr(vtPnp.bstrVal, L"PCI\\") != NULL)
                isPci = TRUE;
            subVendor = parse_subvendor_from_pnpid(vtPnp.bstrVal);
        }

        // Decide se este é o "melhor" adaptador:
        // 1) Preferir PCI
        // 2) Entre PCI, maior VRAM
        bool better =
            !haveBest ||
            (isPci && !bestIsPci) ||
            (isPci == bestIsPci && ramBytes > bestRam);

        if (better) {
            bestRam = ramBytes;
            bestIsPci = isPci;
            bestSubVendor = subVendor;
            haveBest = true;

            bestCompat[0] = '\0';
            if (SUCCEEDED(hrCompat) && vtCompat.vt == VT_BSTR && vtCompat.bstrVal) {
                WideCharToMultiByte(CP_ACP, 0,
                                    vtCompat.bstrVal, -1,
                                    bestCompat, (int)sizeof(bestCompat),
                                    NULL, NULL);
            }
        }

        VariantClear(&vtCompat);
        VariantClear(&vtRam);
        VariantClear(&vtPnp);
        IWbemClassObject_Release(pObj);
    }

    if (pEnum) IEnumWbemClassObject_Release(pEnum);
    if (pSvc) IWbemServices_Release(pSvc);
    CoUninitialize();

    // --- 3) Decide o que exibir ---

    // 3.1) Se conseguimos um subsystem vendor pelo PNPDeviceID, tenta mapear (ASUS, MSI, PNY, etc)
    if (haveBest && bestSubVendor != 0) {
        const char *v = lookup_vendor(bestSubVendor);
        if (v) {
            snprintf(buf, buf_size, "%s", v);
            return true;
        }
    }

    // 3.2) Senão, se o NVML nos deu algo, usa (normalmente "NVIDIA" ou parceiro)
    if (nvmlVendor) {
        snprintf(buf, buf_size, "%s", nvmlVendor);
        return true;
    }

    // 3.3) Último fallback: AdapterCompatibility (ex.: "NVIDIA", "Meta Inc.", etc)
    if (haveBest && bestCompat[0] != '\0') {
        snprintf(buf, buf_size, "%s", bestCompat);
        return true;
    }

    return false;
}

bool get_gpu_tdp(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    nvmlDevice_t dev = NULL;
    HMODULE lib = NULL;
    nvmlInitFunc nvInit = NULL;
    nvmlShutdownFunc nvShutdown = NULL;
    nvmlDeviceGetCountFunc nvGetCount = NULL;
    nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
    nvmlDeviceGetNameFunc nvGetName = NULL;
    nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
    nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
    nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
    nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
    nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

    if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                 &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                 &nvGetBusWidth, &nvGetPciInfo)) {
        unsigned long long minLimit = 0, maxLimit = 0;
        if (nvGetPowerLimits && nvGetPowerLimits(dev, &minLimit, &maxLimit) == 0 && maxLimit > 0) {
            // Convert from milliwatts to watts
            double watts = (double)maxLimit / 1000.0;
            unload_nvml(lib, nvShutdown);
            snprintf(buf, buf_size, "%.1f W", watts);
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }
    return false;
}

bool get_gpu_base_clock(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    nvmlDevice_t dev = NULL;
    HMODULE lib = NULL;
    nvmlInitFunc nvInit = NULL;
    nvmlShutdownFunc nvShutdown = NULL;
    nvmlDeviceGetCountFunc nvGetCount = NULL;
    nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
    nvmlDeviceGetNameFunc nvGetName = NULL;
    nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
    nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
    nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
    nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
    nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

    if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                 &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                 &nvGetBusWidth, &nvGetPciInfo)) {
        unsigned int freq = 0;
        if (nvGetMaxClock && nvGetMaxClock(dev, NVML_CLOCK_GRAPHICS, &freq) == 0 && freq > 0) {
            unload_nvml(lib, nvShutdown);
            // NVML reports MHz directly
            snprintf(buf, buf_size, "%u MHz", freq);
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }
    return false;
}

bool get_vram_size(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    nvmlDevice_t dev = NULL;
    HMODULE lib = NULL;
    nvmlInitFunc nvInit = NULL;
    nvmlShutdownFunc nvShutdown = NULL;
    nvmlDeviceGetCountFunc nvGetCount = NULL;
    nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
    nvmlDeviceGetNameFunc nvGetName = NULL;
    nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
    nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
    nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
    nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
    nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

    if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                 &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                 &nvGetBusWidth, &nvGetPciInfo)) {
        nvmlMemory_t mem = {0};
        if (nvGetMemInfo && nvGetMemInfo(dev, &mem) == 0 && mem.total > 0) {
            unload_nvml(lib, nvShutdown);
            double gb = (double)mem.total / (1024.0 * 1024.0 * 1024.0);
            // Round to nearest whole number for display
            unsigned int gibs = (unsigned int)(gb + 0.5);
            snprintf(buf, buf_size, "%u GBytes", gibs);
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }
        // Fall back to WMI: escolher o adaptador PCI com maior AdapterRAM
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }

    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(
        pSvc, L"WQL",
        L"SELECT AdapterRAM,PNPDeviceID FROM Win32_VideoController",
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnum);

    if (FAILED(hr) || !pEnum) {
        if (pSvc) IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }

    ULONGLONG bestBytes = 0;
    BOOL bestIsPci = FALSE;
    bool haveBest = false;

    ULONG uReturn = 0;
    IWbemClassObject *pObj = NULL;

    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uReturn)) {
        VARIANT vtRam, vtPnp;
        VariantInit(&vtRam);
        VariantInit(&vtPnp);

        HRESULT hrRam = IWbemClassObject_Get(pObj, L"AdapterRAM", 0, &vtRam, NULL, NULL);
        HRESULT hrPnp = IWbemClassObject_Get(pObj, L"PNPDeviceID", 0, &vtPnp, NULL, NULL);

        ULONGLONG bytes = 0;
        if (SUCCEEDED(hrRam) &&
            (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4 || vtRam.vt == VT_I8 || vtRam.vt == VT_UI8)) {
            if (vtRam.vt == VT_I4 || vtRam.vt == VT_UI4) {
                bytes = (ULONGLONG)vtRam.uintVal;
            } else {
                bytes = (ULONGLONG)vtRam.ullVal;
            }
        }

        BOOL isPci = FALSE;
        if (SUCCEEDED(hrPnp) && vtPnp.vt == VT_BSTR && vtPnp.bstrVal) {
            if (wcsstr(vtPnp.bstrVal, L"PCI\\") != NULL) {
                isPci = TRUE;
            }
        }

        if (bytes > 0) {
            bool better =
                !haveBest ||
                (isPci && !bestIsPci) ||
                (isPci == bestIsPci && bytes > bestBytes);

            if (better) {
                bestBytes = bytes;
                bestIsPci = isPci;
                haveBest = true;
            }
        }

        VariantClear(&vtRam);
        VariantClear(&vtPnp);
        IWbemClassObject_Release(pObj);
    }

    if (pEnum) IEnumWbemClassObject_Release(pEnum);
    if (pSvc) IWbemServices_Release(pSvc);
    CoUninitialize();

    if (!haveBest) {
        return false;
    }

    double gb = (double)bestBytes / (1024.0 * 1024.0 * 1024.0);
    unsigned int gibs = (unsigned int)(gb + 0.5);
    snprintf(buf, buf_size, "%u GBytes", gibs);
    return true;
}

// -----------------------------------------------------------------------------
//  VRAM type and vendor via NVIDIA NVAPI (when available)
//
// NVML e WMI não expõem o tipo de VRAM (GDDR5/6, etc) nem o fabricante da
// memória.  Para placas NVIDIA conseguimos isso via NVAPI carregada
// dinamicamente. Em sistemas sem NVAPI os métodos retornam false e a UI
// mostra "Unknown".
// -----------------------------------------------------------------------------

typedef int NvAPI_Status;
typedef void *NvPhysicalGpuHandle;

typedef void *(__cdecl *NvAPI_QueryInterface_t)(unsigned int offset);
typedef NvAPI_Status (__cdecl *NvAPI_Initialize_t)(void);
typedef NvAPI_Status (__cdecl *NvAPI_EnumPhysicalGPUs_t)(NvPhysicalGpuHandle *handles, int *count);
typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamType_t)(NvPhysicalGpuHandle handle, int *type);
typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamMaker_t)(NvPhysicalGpuHandle handle, int *maker);

#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_INTERFACE_OFFSET_INITIALIZE         0x0150E828u
#define NVAPI_INTERFACE_OFFSET_ENUM_PHYSICAL_GPUS 0xE5AC921Fu
#define NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_TYPE   0x57F7CAAcu
#define NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_MAKER  0x42AEA16Au

static bool nvapi_get_primary_gpu(NvPhysicalGpuHandle *outHandle,
                                  NvAPI_QueryInterface_t *outQuery,
                                  HMODULE *outModule)
{
    if (!outHandle || !outQuery || !outModule) return false;
    *outHandle = NULL;
    *outQuery = NULL;
    *outModule = NULL;

    HMODULE h = LoadLibraryA("nvapi64.dll");
    if (!h) h = LoadLibraryA("nvapi.dll");
    if (!h) return false;

    NvAPI_QueryInterface_t query =
        (NvAPI_QueryInterface_t)GetProcAddress(h, "nvapi_QueryInterface");
    if (!query) {
        FreeLibrary(h);
        return false;
    }

    NvAPI_Initialize_t NvAPI_Initialize =
        (NvAPI_Initialize_t)query(NVAPI_INTERFACE_OFFSET_INITIALIZE);
    NvAPI_EnumPhysicalGPUs_t NvAPI_EnumPhysicalGPUs =
        (NvAPI_EnumPhysicalGPUs_t)query(NVAPI_INTERFACE_OFFSET_ENUM_PHYSICAL_GPUS);
    if (!NvAPI_Initialize || !NvAPI_EnumPhysicalGPUs) {
        FreeLibrary(h);
        return false;
    }

    if (NvAPI_Initialize() != 0) {
        FreeLibrary(h);
        return false;
    }

    NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
    int count = 0;
    if (NvAPI_EnumPhysicalGPUs(handles, &count) != 0 || count <= 0) {
        FreeLibrary(h);
        return false;
    }

    *outHandle = handles[0];   // usa a primeira GPU física como principal
    *outQuery  = query;
    *outModule = h;
    return true;
}

static const char *nvapi_ram_type_to_string(int t)
{
    switch (t) {
    case 1:  return "SDRAM";
    case 2:  return "DDR";
    case 3:  return "DDR2";
    case 4:  return "GDDR2";
    case 5:  return "GDDR3";
    case 6:  return "GDDR4";
    case 7:  return "DDR3";
    case 8:  return "GDDR5";
    case 9:  return "LPDDR2";
    case 10: return "GDDR5X";
    case 14: return "GDDR6";
    case 15: return "GDDR6X";
    default: return NULL;
    }
}

static const char *nvapi_ram_maker_to_string(int m)
{
    switch (m) {
    case 1:  return "Samsung";
    case 2:  return "Qimonda";
    case 3:  return "Elpida";
    case 4:  return "Etron";
    case 5:  return "Nanya";
    case 6:  return "Hynix";
    case 7:  return "Mosel";
    case 8:  return "Winbond";
    case 9:  return "Elite";
    case 10: return "Micron";
    default: return NULL;
    }
}

bool get_vram_type(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return false;

    NvPhysicalGpuHandle gpu = NULL;
    NvAPI_QueryInterface_t query = NULL;
    HMODULE module = NULL;

    // Usa o mesmo helper que você já usa em get_vram_vendor
    if (!nvapi_get_primary_gpu(&gpu, &query, &module)) {
        return false;
    }

    typedef int NvAPI_Status;
    typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamType_t)(NvPhysicalGpuHandle, int*);

    NvAPI_GPU_GetRamType_t NvAPI_GPU_GetRamType =
        (NvAPI_GPU_GetRamType_t)query(0x57F7CAACu); // offset oficial do fórum NVIDIA

    if (!NvAPI_GPU_GetRamType) {
        FreeLibrary(module);
        return false;
    }

    int type = 0;
    NvAPI_Status st = NvAPI_GPU_GetRamType(gpu, &type);

    // já podemos liberar a DLL; não vamos usar mais nada
    FreeLibrary(module);

    if (st != 0) {
        return false;
    }

    const char *typeStr = nvapi_ram_type_to_string(type);
    if (!typeStr) {
        // driver devolveu um valor desconhecido/novo
        return false;
    }

    snprintf(buf, buf_size, "%s", typeStr);
    return true;
}


bool get_vram_vendor(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;

    NvPhysicalGpuHandle gpu = NULL;
    NvAPI_QueryInterface_t query = NULL;
    HMODULE module = NULL;
    if (!nvapi_get_primary_gpu(&gpu, &query, &module)) {
        return false;
    }

    NvAPI_GPU_GetRamMaker_t NvAPI_GPU_GetRamMaker =
        (NvAPI_GPU_GetRamMaker_t)query(NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_MAKER);
    if (!NvAPI_GPU_GetRamMaker) {
        FreeLibrary(module);
        return false;
    }

    int maker = 0;
    NvAPI_Status st = NvAPI_GPU_GetRamMaker(gpu, &maker);
    if (st != 0) {
        FreeLibrary(module);
        return false;
    }

    const char *vendor = nvapi_ram_maker_to_string(maker);
    if (!vendor) {
        FreeLibrary(module);
        return false;
    }

    snprintf(buf, buf_size, "%s", vendor);
    FreeLibrary(module);
    return true;
}


bool get_vram_bus_width(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    nvmlDevice_t dev = NULL;
    HMODULE lib = NULL;
    nvmlInitFunc nvInit = NULL;
    nvmlShutdownFunc nvShutdown = NULL;
    nvmlDeviceGetCountFunc nvGetCount = NULL;
    nvmlDeviceGetHandleByIndexFunc nvGetHandle = NULL;
    nvmlDeviceGetNameFunc nvGetName = NULL;
    nvmlDeviceGetPowerManagementLimitConstraintsFunc nvGetPowerLimits = NULL;
    nvmlDeviceGetMaxClockInfoFunc nvGetMaxClock = NULL;
    nvmlDeviceGetMemoryInfoFunc nvGetMemInfo = NULL;
    nvmlDeviceGetMemoryBusWidthFunc nvGetBusWidth = NULL;
    nvmlDeviceGetPciInfoFunc nvGetPciInfo = NULL;

    if (try_nvml(&dev, &lib, &nvInit, &nvShutdown, &nvGetCount, &nvGetHandle,
                 &nvGetName, &nvGetPowerLimits, &nvGetMaxClock, &nvGetMemInfo,
                 &nvGetBusWidth, &nvGetPciInfo)) {
        unsigned int width = 0;
        if (nvGetBusWidth && nvGetBusWidth(dev, &width) == 0 && width > 0) {
            unload_nvml(lib, nvShutdown);
            snprintf(buf, buf_size, "%u bits", width);
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }
    return false;
}