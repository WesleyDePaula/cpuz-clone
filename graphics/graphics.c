// graphics.c - Informações de GPU e memória de vídeo
// Busca dados usando as APIs oficiais: NVML (NVIDIA), ADL (AMD), IGCL (Intel)
// Se não houver API disponível, usa WMI como alternativa

#define COBJMACROS

#include "graphics.h"

#include <windows.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#pragma comment(lib, "wbemuuid.lib")

// ============================================================================
// ADL - Biblioteca da AMD para placas de vídeo Radeon
// ============================================================================

// Códigos de retorno da ADL
#define ADL_OK 0
#define ADL_ERR -1

// Tipos de memória suportados pela ADL
#define ADL_MEMORTYPE_GDDR5 5
#define ADL_MEMORTYPE_GDDR6 6

typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);

typedef struct AdapterInfo {
    int iSize;
    int iAdapterIndex;
    char strAdapterName[256];
    char strDisplayName[256];
    int iPresent;
    int iExist;
    char strDriverPath[256];
    char strDriverPathExt[256];
    char strPNPString[256];
    int iOSDisplayIndex;
} AdapterInfo, *LPAdapterInfo;

typedef struct ADLMemoryInfo {
    long long iMemorySize;
    char strMemoryType[256];
    long long iMemoryBandwidth;
} ADLMemoryInfo;

typedef struct ADLPMActivity {
    int iSize;
    int iEngineClock;
    int iMemoryClock;
    int iVddc;
    int iActivityPercent;
    int iCurrentPerformanceLevel;
    int iCurrentBusSpeed;
    int iCurrentBusLanes;
    int iMaximumBusLanes;
    int iReserved;
} ADLPMActivity;

typedef struct ADLVersionsInfo {
    char strDriverVer[256];
    char strCatalystVersion[256];
    char strCatalystWebLink[256];
} ADLVersionsInfo;

// ADL function pointers
typedef int (*ADL_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (*ADL_MAIN_CONTROL_DESTROY)();
typedef int (*ADL_ADAPTER_NUMBEROFADAPTERS_GET)(int*);
typedef int (*ADL_ADAPTER_ADAPTERINFO_GET)(LPAdapterInfo, int);
typedef int (*ADL_ADAPTER_ACTIVE_GET)(int, int*);
typedef int (*ADL_ADAPTER_MEMORYINFO_GET)(int, ADLMemoryInfo*);
typedef int (*ADL_OVERDRIVE5_CURRENTACTIVITY_GET)(int, ADLPMActivity*);
typedef int (*ADL_ADAPTER_ASICFAMILYTYPE_GET)(int, int*, int*);
typedef int (*ADL_ADAPTER_VERSIONINFO_GET)(int, ADLVersionsInfo*);

// Função auxiliar para alocação de memória da ADL
static void* __stdcall adl_malloc_callback(int size) {
    return malloc(size);
}

// Armazena o estado da biblioteca ADL
typedef struct {
    HMODULE lib;
    ADL_MAIN_CONTROL_CREATE ADL_Main_Control_Create;
    ADL_MAIN_CONTROL_DESTROY ADL_Main_Control_Destroy;
    ADL_ADAPTER_NUMBEROFADAPTERS_GET ADL_Adapter_NumberOfAdapters_Get;
    ADL_ADAPTER_ADAPTERINFO_GET ADL_Adapter_AdapterInfo_Get;
    ADL_ADAPTER_ACTIVE_GET ADL_Adapter_Active_Get;
    ADL_ADAPTER_MEMORYINFO_GET ADL_Adapter_MemoryInfo_Get;
    ADL_OVERDRIVE5_CURRENTACTIVITY_GET ADL_Overdrive5_CurrentActivity_Get;
    ADL_ADAPTER_ASICFAMILYTYPE_GET ADL_Adapter_ASICFamilyType_Get;
    ADL_ADAPTER_VERSIONINFO_GET ADL_Adapter_VersionsInfo_Get;
    int adapterIndex;
    bool initialized;
} ADLContext;

// Tenta carregar a biblioteca ADL e encontrar uma placa AMD ativa
static bool try_adl(ADLContext *ctx) {
    if (!ctx) return false;
    memset(ctx, 0, sizeof(ADLContext));

    // Tenta carregar a DLL da AMD (64-bit ou 32-bit)
    ctx->lib = LoadLibraryA("atiadlxx.dll");
    if (!ctx->lib) {
        ctx->lib = LoadLibraryA("atiadlxy.dll");
    }
    if (!ctx->lib) {
        return false;
    }

    // Carrega as funções da biblioteca ADL
    ctx->ADL_Main_Control_Create = (ADL_MAIN_CONTROL_CREATE)
        GetProcAddress(ctx->lib, "ADL_Main_Control_Create");
    ctx->ADL_Main_Control_Destroy = (ADL_MAIN_CONTROL_DESTROY)
        GetProcAddress(ctx->lib, "ADL_Main_Control_Destroy");
    ctx->ADL_Adapter_NumberOfAdapters_Get = (ADL_ADAPTER_NUMBEROFADAPTERS_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_NumberOfAdapters_Get");
    ctx->ADL_Adapter_AdapterInfo_Get = (ADL_ADAPTER_ADAPTERINFO_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_AdapterInfo_Get");
    ctx->ADL_Adapter_Active_Get = (ADL_ADAPTER_ACTIVE_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_Active_Get");
    ctx->ADL_Adapter_MemoryInfo_Get = (ADL_ADAPTER_MEMORYINFO_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_MemoryInfo_Get");
    ctx->ADL_Overdrive5_CurrentActivity_Get = (ADL_OVERDRIVE5_CURRENTACTIVITY_GET)
        GetProcAddress(ctx->lib, "ADL_Overdrive5_CurrentActivity_Get");
    ctx->ADL_Adapter_ASICFamilyType_Get = (ADL_ADAPTER_ASICFAMILYTYPE_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_ASICFamilyType_Get");
    ctx->ADL_Adapter_VersionsInfo_Get = (ADL_ADAPTER_VERSIONINFO_GET)
        GetProcAddress(ctx->lib, "ADL_Adapter_VersionsInfo_Get");

    if (!ctx->ADL_Main_Control_Create || !ctx->ADL_Main_Control_Destroy ||
        !ctx->ADL_Adapter_NumberOfAdapters_Get || !ctx->ADL_Adapter_AdapterInfo_Get ||
        !ctx->ADL_Adapter_Active_Get) {
        FreeLibrary(ctx->lib);
        return false;
    }

    // Inicializa a biblioteca ADL
    if (ctx->ADL_Main_Control_Create(adl_malloc_callback, 1) != ADL_OK) {
        FreeLibrary(ctx->lib);
        return false;
    }

    // Descobre quantas placas AMD existem no sistema
    int numAdapters = 0;
    if (ctx->ADL_Adapter_NumberOfAdapters_Get(&numAdapters) != ADL_OK || numAdapters <= 0) {
        ctx->ADL_Main_Control_Destroy();
        FreeLibrary(ctx->lib);
        return false;
    }

    // Allocate adapter info array
    LPAdapterInfo adapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * numAdapters);
    if (!adapterInfo) {
        ctx->ADL_Main_Control_Destroy();
        FreeLibrary(ctx->lib);
        return false;
    }

    memset(adapterInfo, 0, sizeof(AdapterInfo) * numAdapters);

    // Get adapter info
    if (ctx->ADL_Adapter_AdapterInfo_Get(adapterInfo, sizeof(AdapterInfo) * numAdapters) != ADL_OK) {
        free(adapterInfo);
        ctx->ADL_Main_Control_Destroy();
        FreeLibrary(ctx->lib);
        return false;
    }

    // Find first active adapter
    ctx->adapterIndex = -1;
    for (int i = 0; i < numAdapters; i++) {
        int isActive = 0;
        if (adapterInfo[i].iAdapterIndex >= 0 &&
            ctx->ADL_Adapter_Active_Get(adapterInfo[i].iAdapterIndex, &isActive) == ADL_OK &&
            isActive) {
            ctx->adapterIndex = adapterInfo[i].iAdapterIndex;
            break;
        }
    }

    free(adapterInfo);

    if (ctx->adapterIndex < 0) {
        ctx->ADL_Main_Control_Destroy();
        FreeLibrary(ctx->lib);
        return false;
    }

    ctx->initialized = true;
    return true;
}

// Libera recursos da biblioteca ADL
static void cleanup_adl(ADLContext *ctx) {
    if (ctx && ctx->initialized) {
        if (ctx->ADL_Main_Control_Destroy) {
            ctx->ADL_Main_Control_Destroy();
        }
        if (ctx->lib) {
            FreeLibrary(ctx->lib);
        }
        ctx->initialized = false;
    }
}

// ============================================================================
// IGCL - Biblioteca da Intel para placas de vídeo integradas e Arc
// ============================================================================

// Códigos de retorno da IGCL
#define CTL_RESULT_SUCCESS 0x00000000

// Tipos de dispositivo Intel
#define CTL_DEVICE_TYPE_GRAPHICS 1

// Tipos de memória suportados pela Intel
typedef enum {
    CTL_MEM_TYPE_DDR3 = 0,
    CTL_MEM_TYPE_DDR4 = 1,
    CTL_MEM_TYPE_DDR5 = 2,
    CTL_MEM_TYPE_LPDDR3 = 3,
    CTL_MEM_TYPE_LPDDR4 = 4,
    CTL_MEM_TYPE_LPDDR5 = 5,
    CTL_MEM_TYPE_GDDR5 = 6,
    CTL_MEM_TYPE_GDDR6 = 7,
    CTL_MEM_TYPE_GDDR6X = 8,
    CTL_MEM_TYPE_HBM = 9,
    CTL_MEM_TYPE_HBM2 = 10
} ctl_mem_type_t;

typedef unsigned int ctl_result_t;
typedef void* ctl_api_handle_t;
typedef void* ctl_device_adapter_handle_t;

typedef struct ctl_init_args_t {
    unsigned int Size;
    unsigned int Version;
    unsigned int flags;
} ctl_init_args_t;

typedef struct ctl_device_adapter_properties_t {
    unsigned int Size;
    unsigned int Version;
    void* pDeviceID;
    unsigned int device_id_size;
    unsigned int type;
    char name[256];
    void* pDriverVersion;
    unsigned int driver_version_size;
} ctl_device_adapter_properties_t;

typedef struct ctl_mem_properties_t {
    unsigned int Size;
    unsigned int Version;
    unsigned long long physicalSize;
    int memoryType;
    unsigned int busWidth;
    unsigned int numChannels;
} ctl_mem_properties_t;

typedef struct ctl_freq_properties_t {
    unsigned int Size;
    unsigned int Version;
    unsigned int canControl;
    double min;
    double max;
} ctl_freq_properties_t;

typedef struct ctl_power_properties_t {
    unsigned int Size;
    unsigned int Version;
    unsigned int canControl;
    int defaultLimit;
    int maxLimit;
    int minLimit;
} ctl_power_properties_t;

// IGCL function pointers
typedef ctl_result_t (*CTL_INIT)(ctl_init_args_t*, ctl_api_handle_t*);
typedef ctl_result_t (*CTL_CLOSE)(ctl_api_handle_t);
typedef ctl_result_t (*CTL_ENUM_DEVICES)(ctl_api_handle_t, unsigned int*, ctl_device_adapter_handle_t*);
typedef ctl_result_t (*CTL_GET_DEVICE_PROPERTIES)(ctl_device_adapter_handle_t, ctl_device_adapter_properties_t*);
typedef ctl_result_t (*CTL_GET_MEM_PROPERTIES)(ctl_device_adapter_handle_t, ctl_mem_properties_t*);
typedef ctl_result_t (*CTL_GET_FREQ_PROPERTIES)(ctl_device_adapter_handle_t, ctl_freq_properties_t*);
typedef ctl_result_t (*CTL_GET_POWER_PROPERTIES)(ctl_device_adapter_handle_t, ctl_power_properties_t*);

// Intel context
typedef struct {
    HMODULE lib;
    ctl_api_handle_t api_handle;
    ctl_device_adapter_handle_t device_handle;
    CTL_INIT ctlInit;
    CTL_CLOSE ctlClose;
    CTL_ENUM_DEVICES ctlEnumDevices;
    CTL_GET_DEVICE_PROPERTIES ctlGetDeviceProperties;
    CTL_GET_MEM_PROPERTIES ctlGetMemProperties;
    CTL_GET_FREQ_PROPERTIES ctlGetFreqProperties;
    CTL_GET_POWER_PROPERTIES ctlGetPowerProperties;
    bool initialized;
} IntelContext;

// Tenta carregar a biblioteca Intel e encontrar uma placa gráfica ativa
static bool try_intel(IntelContext *ctx) {
    if (!ctx) return false;
    memset(ctx, 0, sizeof(IntelContext));

    // Tenta carregar a DLL da Intel
    ctx->lib = LoadLibraryA("ControlLib.dll");
    if (!ctx->lib) {
        ctx->lib = LoadLibraryA("igcl.dll");
    }
    if (!ctx->lib) {
        return false;
    }

    // Carrega as funções da biblioteca Intel
    ctx->ctlInit = (CTL_INIT)GetProcAddress(ctx->lib, "ctlInit");
    ctx->ctlClose = (CTL_CLOSE)GetProcAddress(ctx->lib, "ctlClose");
    ctx->ctlEnumDevices = (CTL_ENUM_DEVICES)GetProcAddress(ctx->lib, "ctlEnumDevices");
    ctx->ctlGetDeviceProperties = (CTL_GET_DEVICE_PROPERTIES)GetProcAddress(ctx->lib, "ctlGetDeviceProperties");
    ctx->ctlGetMemProperties = (CTL_GET_MEM_PROPERTIES)GetProcAddress(ctx->lib, "ctlGetMemProperties");
    ctx->ctlGetFreqProperties = (CTL_GET_FREQ_PROPERTIES)GetProcAddress(ctx->lib, "ctlGetFreqProperties");
    ctx->ctlGetPowerProperties = (CTL_GET_POWER_PROPERTIES)GetProcAddress(ctx->lib, "ctlGetPowerProperties");

    if (!ctx->ctlInit || !ctx->ctlClose || !ctx->ctlEnumDevices || !ctx->ctlGetDeviceProperties) {
        FreeLibrary(ctx->lib);
        return false;
    }

    // Inicializa a biblioteca Intel
    ctl_init_args_t init_args = {0};
    init_args.Size = sizeof(ctl_init_args_t);
    init_args.Version = 0;

    if (ctx->ctlInit(&init_args, &ctx->api_handle) != CTL_RESULT_SUCCESS) {
        FreeLibrary(ctx->lib);
        return false;
    }

    // Lista todos os dispositivos Intel
    unsigned int deviceCount = 0;
    if (ctx->ctlEnumDevices(ctx->api_handle, &deviceCount, NULL) != CTL_RESULT_SUCCESS || deviceCount == 0) {
        ctx->ctlClose(ctx->api_handle);
        FreeLibrary(ctx->lib);
        return false;
    }

    ctl_device_adapter_handle_t *devices = (ctl_device_adapter_handle_t*)malloc(sizeof(ctl_device_adapter_handle_t) * deviceCount);
    if (!devices) {
        ctx->ctlClose(ctx->api_handle);
        FreeLibrary(ctx->lib);
        return false;
    }

    if (ctx->ctlEnumDevices(ctx->api_handle, &deviceCount, devices) != CTL_RESULT_SUCCESS) {
        free(devices);
        ctx->ctlClose(ctx->api_handle);
        FreeLibrary(ctx->lib);
        return false;
    }

    // Procura a primeira placa de vídeo
    ctx->device_handle = NULL;
    for (unsigned int i = 0; i < deviceCount; i++) {
        ctl_device_adapter_properties_t props = {0};
        props.Size = sizeof(ctl_device_adapter_properties_t);
        props.Version = 0;

        if (ctx->ctlGetDeviceProperties(devices[i], &props) == CTL_RESULT_SUCCESS) {
            if (props.type == CTL_DEVICE_TYPE_GRAPHICS) {
                ctx->device_handle = devices[i];
                break;
            }
        }
    }

    free(devices);

    if (!ctx->device_handle) {
        ctx->ctlClose(ctx->api_handle);
        FreeLibrary(ctx->lib);
        return false;
    }

    ctx->initialized = true;
    return true;
}

// Libera recursos da biblioteca Intel
static void cleanup_intel(IntelContext *ctx) {
    if (ctx && ctx->initialized) {
        if (ctx->ctlClose && ctx->api_handle) {
            ctx->ctlClose(ctx->api_handle);
        }
        if (ctx->lib) {
            FreeLibrary(ctx->lib);
        }
        ctx->initialized = false;
    }
}

// Convert Intel memory type enum to string
static const char *intel_mem_type_to_string(int type) {
    switch (type) {
    case CTL_MEM_TYPE_DDR3:    return "DDR3";
    case CTL_MEM_TYPE_DDR4:    return "DDR4";
    case CTL_MEM_TYPE_DDR5:    return "DDR5";
    case CTL_MEM_TYPE_LPDDR3:  return "LPDDR3";
    case CTL_MEM_TYPE_LPDDR4:  return "LPDDR4";
    case CTL_MEM_TYPE_LPDDR5:  return "LPDDR5";
    case CTL_MEM_TYPE_GDDR5:   return "GDDR5";
    case CTL_MEM_TYPE_GDDR6:   return "GDDR6";
    case CTL_MEM_TYPE_GDDR6X:  return "GDDR6X";
    case CTL_MEM_TYPE_HBM:     return "HBM";
    case CTL_MEM_TYPE_HBM2:    return "HBM2";
    default:                   return NULL;
    }
}

// ============================================================================
//  NVML dynamic loader - runtime resolution via LoadLibrary to avoid build dependency
//  Queries: device name, power limits, clocks, memory size/bus width
// ============================================================================

typedef int nvmlReturn_t;
typedef void* nvmlDevice_t;

// NVML clock domain for base GPU frequency
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

// Load NVML from standard paths and initialize first GPU device
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

    // Try loading from PATH/System32
    const char *nvmlNames[] = { "nvml.dll", "nvml64.dll", NULL };
    HMODULE h = NULL;
    for (int i = 0; nvmlNames[i] != NULL && !h; ++i) {
        h = LoadLibraryA(nvmlNames[i]);
    }

    // Fallback: try standard NVIDIA driver installation paths
    if (!h) {
        char path[MAX_PATH];
        const char *envs[] = { "ProgramW6432", "ProgramFiles", "ProgramFiles(x86)", NULL };

        for (int i = 0; envs[i] != NULL && !h; ++i) {
            DWORD len = GetEnvironmentVariableA(envs[i], path, (DWORD)sizeof(path));
            if (len > 0 && len < sizeof(path)) {
                // Remove trailing slash if present
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

    // Resolve required NVML functions
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

// Initialize COM and connect to WMI ROOT\CIMV2 namespace
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

// Map PCI subsystem vendor IDs to board partner names
// Extracted from PNPDeviceID SUBSYS field or NVML pciSubSystemId
struct vendor_map_entry { unsigned int id; const char *name; };
static const struct vendor_map_entry vendor_map[] = {
    { 0x8086, "Intel" },       // 0x8086: Intel Corporation
    { 0x1002, "AMD" },         // 0x1002: AMD reference
    { 0x10DE, "NVIDIA" },      // 0x10DE: NVIDIA reference/Founders Edition
    { 0x1043, "ASUS" },        // 0x1043: ASUS
    { 0x1458, "Gigabyte" },    // 0x1458: Gigabyte
    { 0x1462, "MSI" },         // 0x1462: MSI
    { 0x196E, "PNY" },
    { 0x3842, "EVGA" },
    { 0x19DA, "Zotac" },
    { 0x1DA2, "Palit" },
    { 0x1787, "Sapphire" },    // 0x1787: Sapphire (AMD partner)
    { 0x1682, "XFX" },         // 0x1682: XFX (AMD partner)
    { 0x148C, "PowerColor" },  // 0x148C: PowerColor (AMD partner)
    { 0x17AA, "Lenovo" },
    { 0x174B, "Sapphire" },    // Alternative Sapphire ID
    { 0x1028, "Dell" },        // 0x1028: Dell
    { 0x103C, "HP" },          // 0x103C: HP
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

// Convert wide hex digit to 0-15, or -1 on error
static int hex_val_w(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return 10 + (ch - L'a');
    if (ch >= L'A' && ch <= L'F') return 10 + (ch - L'A');
    return -1;
}

// Extract subsystem vendor ID from PNPDeviceID SUBSYS field
// Format: PCI\VEN_v(4)&DEV_d(4)&SUBSYS_s(4)n(4)&REV_r(2)
// Example: SUBSYS_2489196E returns 0x196E (lower 16 bits)
static unsigned int parse_subvendor_from_pnpid(const wchar_t *pnpId)
{
    if (!pnpId) return 0;

    const wchar_t *sub = wcsstr(pnpId, L"SUBSYS_");
    if (!sub) return 0;

    sub += 7; // pula "SUBSYS_"

    // Parse 8 hex digits: ssss(device) nnnn(vendor)
    unsigned int subsys = 0;
    for (int i = 0; i < 8; ++i) {
        wchar_t ch = sub[i];
        int hv = hex_val_w(ch);
        if (hv < 0) {
            return 0; // unexpected format
        }
        subsys = (subsys << 4) | (unsigned int)hv;
    }

    // Lower 16 bits = subsystem vendor ID
    return subsys & 0xFFFFu;
}


// ============================================================================
//  Public API - GPU Information
// ============================================================================

// GPU Name: NVML nvmlDeviceGetName() -> ADL AdapterInfo.strAdapterName -> WMI Win32_VideoController.Name
bool get_gpu_name(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;

    // Try NVML first
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

    // Try ADL for AMD GPUs
    ADLContext adl;
    if (try_adl(&adl)) {
        int numAdapters = 0;
        if (adl.ADL_Adapter_NumberOfAdapters_Get(&numAdapters) == ADL_OK && numAdapters > 0) {
            LPAdapterInfo adapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * numAdapters);
            if (adapterInfo) {
                memset(adapterInfo, 0, sizeof(AdapterInfo) * numAdapters);
                if (adl.ADL_Adapter_AdapterInfo_Get(adapterInfo, sizeof(AdapterInfo) * numAdapters) == ADL_OK) {
                    for (int i = 0; i < numAdapters; i++) {
                        if (adapterInfo[i].iAdapterIndex == adl.adapterIndex) {
                            snprintf(buf, buf_size, "%s", adapterInfo[i].strAdapterName);
                            free(adapterInfo);
                            cleanup_adl(&adl);
                            return true;
                        }
                    }
                }
                free(adapterInfo);
            }
        }
        cleanup_adl(&adl);
    }

    // Try Intel IGCL
    IntelContext intel;
    if (try_intel(&intel)) {
        if (intel.ctlGetDeviceProperties) {
            ctl_device_adapter_properties_t props = {0};
            props.Size = sizeof(ctl_device_adapter_properties_t);
            props.Version = 0;
            if (intel.ctlGetDeviceProperties(intel.device_handle, &props) == CTL_RESULT_SUCCESS) {
                if (props.name[0] != '\0') {
                    snprintf(buf, buf_size, "%s", props.name);
                    cleanup_intel(&intel);
                    return true;
                }
            }
        }
        cleanup_intel(&intel);
    }

    // Fallback to WMI: select PCI controller with largest AdapterRAM
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


// Board Manufacturer: PNPDeviceID SUBSYS -> vendor_map[] lookup
// Falls back to NVML pciSubSystemId, ADL PNPString parsing, or WMI AdapterCompatibility
bool get_gpu_board_manufacturer(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return false;

    const char *nvmlVendor = NULL;
    const char *adlVendor = NULL;
    const char *intelVendor = NULL;

    // Try NVML first: pciSubSystemId -> subsystem vendor
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
                    nvmlVendor = v; // save for fallback if WMI fails
                }
            }

            unload_nvml(lib, nvShutdown);
        }
    }

    // Try ADL for AMD: parse PNPString for subsystem vendor
    {
        ADLContext adl;
        if (try_adl(&adl)) {
            int numAdapters = 0;
            if (adl.ADL_Adapter_NumberOfAdapters_Get(&numAdapters) == ADL_OK && numAdapters > 0) {
                LPAdapterInfo adapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * numAdapters);
                if (adapterInfo) {
                    memset(adapterInfo, 0, sizeof(AdapterInfo) * numAdapters);
                    if (adl.ADL_Adapter_AdapterInfo_Get(adapterInfo, sizeof(AdapterInfo) * numAdapters) == ADL_OK) {
                        for (int i = 0; i < numAdapters; i++) {
                            if (adapterInfo[i].iAdapterIndex == adl.adapterIndex) {
                                // Convert PNPString to wide char for parsing
                                wchar_t widePnp[256] = {0};
                                MultiByteToWideChar(CP_ACP, 0, adapterInfo[i].strPNPString, -1,
                                                    widePnp, 256);
                                unsigned int subVid = parse_subvendor_from_pnpid(widePnp);
                                if (subVid != 0) {
                                    const char *v = lookup_vendor(subVid);
                                    if (v) {
                                        adlVendor = v;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    free(adapterInfo);
                }
            }
            cleanup_adl(&adl);
        }
    }

    // Try Intel via WMI for subsystem vendor (IGCL doesn't expose board manufacturer directly)
    {
        IWbemServices *pSvcIntel = NULL;
        if (init_wmi(&pSvcIntel)) {
            IEnumWbemClassObject *pEnumIntel = NULL;
            HRESULT hrIntel = IWbemServices_ExecQuery(
                pSvcIntel, L"WQL",
                L"SELECT PNPDeviceID FROM Win32_VideoController WHERE PNPDeviceID LIKE 'PCI\\\\VEN_8086%'",
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL, &pEnumIntel);

            if (SUCCEEDED(hrIntel) && pEnumIntel) {
                ULONG uRet = 0;
                IWbemClassObject *pObjIntel = NULL;
                if (S_OK == IEnumWbemClassObject_Next(pEnumIntel, WBEM_INFINITE, 1, &pObjIntel, &uRet)) {
                    VARIANT vtPnpIntel;
                    VariantInit(&vtPnpIntel);
                    HRESULT hrPnpIntel = IWbemClassObject_Get(pObjIntel, L"PNPDeviceID", 0, &vtPnpIntel, NULL, NULL);
                    if (SUCCEEDED(hrPnpIntel) && vtPnpIntel.vt == VT_BSTR && vtPnpIntel.bstrVal) {
                        unsigned int subVidIntel = parse_subvendor_from_pnpid(vtPnpIntel.bstrVal);
                        if (subVidIntel != 0) {
                            const char *v = lookup_vendor(subVidIntel);
                            if (v) {
                                intelVendor = v;
                            }
                        }
                    }
                    VariantClear(&vtPnpIntel);
                    IWbemClassObject_Release(pObjIntel);
                }
                IEnumWbemClassObject_Release(pEnumIntel);
            }
            if (pSvcIntel) IWbemServices_Release(pSvcIntel);
            CoUninitialize();
        }
    }

    // WMI: select PCI adapter with largest VRAM, extract SUBSYS from PNPDeviceID
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        // No WMI: use NVML result if available, otherwise fail
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

        // Select best adapter: prefer PCI, then highest VRAM
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

    // Priority: PNPDeviceID subsystem vendor -> NVML result -> ADL result -> AdapterCompatibility

    // Try mapping subsystem vendor ID to known board partner
    if (haveBest && bestSubVendor != 0) {
        const char *v = lookup_vendor(bestSubVendor);
        if (v) {
            snprintf(buf, buf_size, "%s", v);
            return true;
        }
    }

    // Use NVML result (typically "NVIDIA" or board partner)
    if (nvmlVendor) {
        snprintf(buf, buf_size, "%s", nvmlVendor);
        return true;
    }

    // Use ADL result (AMD board partner)
    if (adlVendor) {
        snprintf(buf, buf_size, "%s", adlVendor);
        return true;
    }

    // Use Intel result (Intel or OEM)
    if (intelVendor) {
        snprintf(buf, buf_size, "%s", intelVendor);
        return true;
    }

    // Final fallback: WMI AdapterCompatibility string
    if (haveBest && bestCompat[0] != '\0') {
        snprintf(buf, buf_size, "%s", bestCompat);
        return true;
    }

    return false;
}

// GPU TDP: NVML nvmlDeviceGetPowerManagementLimitConstraints() max limit (AMD ADL does not provide TDP)
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
    // Note: ADL does not provide TDP information
    return false;
}

// GPU Base Clock: NVML nvmlDeviceGetMaxClockInfo(NVML_CLOCK_GRAPHICS) -> ADL Overdrive5_CurrentActivity
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

    // Try ADL for AMD GPUs
    ADLContext adl;
    if (try_adl(&adl)) {
        if (adl.ADL_Overdrive5_CurrentActivity_Get) {
            ADLPMActivity activity = {0};
            activity.iSize = sizeof(ADLPMActivity);
            if (adl.ADL_Overdrive5_CurrentActivity_Get(adl.adapterIndex, &activity) == ADL_OK &&
                activity.iEngineClock > 0) {
                // ADL reports clock in 10 kHz units, convert to MHz
                unsigned int freq = activity.iEngineClock / 100;
                cleanup_adl(&adl);
                snprintf(buf, buf_size, "%u MHz", freq);
                return true;
            }
        }
        cleanup_adl(&adl);
    }

    // Try Intel IGCL (note: IGCL provides max frequency, not base clock)
    IntelContext intel;
    if (try_intel(&intel)) {
        if (intel.ctlGetFreqProperties) {
            ctl_freq_properties_t freqProps = {0};
            freqProps.Size = sizeof(ctl_freq_properties_t);
            freqProps.Version = 0;
            if (intel.ctlGetFreqProperties(intel.device_handle, &freqProps) == CTL_RESULT_SUCCESS) {
                if (freqProps.max > 0) {
                    // Intel reports frequency in MHz
                    unsigned int freq = (unsigned int)freqProps.max;
                    cleanup_intel(&intel);
                    snprintf(buf, buf_size, "%u MHz", freq);
                    return true;
                }
            }
        }
        cleanup_intel(&intel);
    }
    return false;
}

// VRAM Size: NVML nvmlDeviceGetMemoryInfo() -> ADL MemoryInfo -> IGCL MemProperties -> WMI Win32_VideoController.AdapterRAM
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
            double mb = (double)mem.total / (1024.0 * 1024.0);
            if (mb >= 1024.0) {
                double gb = mb / 1024.0;
                snprintf(buf, buf_size, "%.0f GBytes", gb);
            } else {
                snprintf(buf, buf_size, "%.0f MBytes", mb);
            }
            return true;
        }
        unload_nvml(lib, nvShutdown);
    }

    // Try ADL for AMD GPUs
    ADLContext adl;
    if (try_adl(&adl)) {
        if (adl.ADL_Adapter_MemoryInfo_Get) {
            ADLMemoryInfo memInfo = {0};
            if (adl.ADL_Adapter_MemoryInfo_Get(adl.adapterIndex, &memInfo) == ADL_OK &&
                memInfo.iMemorySize > 0) {
                // ADL reports memory in bytes (usually)
                double mb = (double)memInfo.iMemorySize / (1024.0 * 1024.0);
                cleanup_adl(&adl);
                if (mb >= 1024.0) {
                    double gb = mb / 1024.0;
                    snprintf(buf, buf_size, "%.0f GBytes", gb);
                } else {
                    snprintf(buf, buf_size, "%.0f MBytes", mb);
                }
                return true;
            }
        }
        cleanup_adl(&adl);
    }

    // Try Intel IGCL
    IntelContext intel;
    if (try_intel(&intel)) {
        if (intel.ctlGetMemProperties) {
            ctl_mem_properties_t memProps = {0};
            memProps.Size = sizeof(ctl_mem_properties_t);
            memProps.Version = 0;
            if (intel.ctlGetMemProperties(intel.device_handle, &memProps) == CTL_RESULT_SUCCESS) {
                if (memProps.physicalSize > 0) {
                    double mb = (double)memProps.physicalSize / (1024.0 * 1024.0);
                    cleanup_intel(&intel);
                    if (mb >= 1024.0) {
                        double gb = mb / 1024.0;
                        snprintf(buf, buf_size, "%.0f GBytes", gb);
                    } else {
                        snprintf(buf, buf_size, "%.0f MBytes", mb);
                    }
                    return true;
                }
            }
        }
        cleanup_intel(&intel);
    }

    // Try DXGI for more accurate VRAM reporting (especially for Intel integrated)
    // This is more reliable than WMI AdapterRAM for integrated graphics
    typedef HRESULT (WINAPI *PFN_CREATE_DXGI_FACTORY)(REFIID, void**);
    HMODULE hDxgi = LoadLibraryA("dxgi.dll");
    if (hDxgi) {
        PFN_CREATE_DXGI_FACTORY pCreateDXGIFactory =
            (PFN_CREATE_DXGI_FACTORY)GetProcAddress(hDxgi, "CreateDXGIFactory");

        if (pCreateDXGIFactory) {
            // Define DXGI structures
            typedef struct DXGI_ADAPTER_DESC {
                WCHAR Description[128];
                UINT VendorId;
                UINT DeviceId;
                UINT SubSysId;
                UINT Revision;
                SIZE_T DedicatedVideoMemory;
                SIZE_T DedicatedSystemMemory;
                SIZE_T SharedSystemMemory;
                LARGE_INTEGER AdapterLuid;
            } DXGI_ADAPTER_DESC;

            // Define vtable structures for proper COM access
            typedef struct IDXGIAdapterVtbl {
                HRESULT (WINAPI *QueryInterface)(void*, REFIID, void**);
                ULONG (WINAPI *AddRef)(void*);
                ULONG (WINAPI *Release)(void*);
                HRESULT (WINAPI *SetPrivateData)(void*, REFGUID, UINT, const void*);
                HRESULT (WINAPI *SetPrivateDataInterface)(void*, REFGUID, const IUnknown*);
                HRESULT (WINAPI *GetPrivateData)(void*, REFGUID, UINT*, void*);
                HRESULT (WINAPI *GetParent)(void*, REFIID, void**);
                HRESULT (WINAPI *EnumOutputs)(void*, UINT, void**);
                HRESULT (WINAPI *GetDesc)(void*, DXGI_ADAPTER_DESC*);
                HRESULT (WINAPI *CheckInterfaceSupport)(void*, REFGUID, LARGE_INTEGER*);
            } IDXGIAdapterVtbl;

            typedef struct IDXGIAdapter {
                IDXGIAdapterVtbl *lpVtbl;
            } IDXGIAdapter;

            typedef struct IDXGIFactoryVtbl {
                HRESULT (WINAPI *QueryInterface)(void*, REFIID, void**);
                ULONG (WINAPI *AddRef)(void*);
                ULONG (WINAPI *Release)(void*);
                HRESULT (WINAPI *SetPrivateData)(void*, REFGUID, UINT, const void*);
                HRESULT (WINAPI *SetPrivateDataInterface)(void*, REFGUID, const IUnknown*);
                HRESULT (WINAPI *GetPrivateData)(void*, REFGUID, UINT*, void*);
                HRESULT (WINAPI *GetParent)(void*, REFIID, void**);
                HRESULT (WINAPI *EnumAdapters)(void*, UINT, IDXGIAdapter**);
                HRESULT (WINAPI *MakeWindowAssociation)(void*, HWND, UINT);
                HRESULT (WINAPI *GetWindowAssociation)(void*, HWND*);
                HRESULT (WINAPI *CreateSwapChain)(void*, IUnknown*, void*, void**);
                HRESULT (WINAPI *CreateSoftwareAdapter)(void*, HMODULE, void**);
            } IDXGIFactoryVtbl;

            typedef struct IDXGIFactory {
                IDXGIFactoryVtbl *lpVtbl;
            } IDXGIFactory;

            IDXGIFactory *pFactory = NULL;
            // IID_IDXGIFactory = {0x7b7166ec-21c7-44ae-b21a-c9ae321ae369}
            GUID iidFactory = {0x7b7166ec, 0x21c7, 0x44ae, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}};

            if (SUCCEEDED(pCreateDXGIFactory(&iidFactory, (void**)&pFactory)) && pFactory) {
                IDXGIAdapter *pAdapter = NULL;
                HRESULT hr = pFactory->lpVtbl->EnumAdapters(pFactory, 0, &pAdapter);

                if (SUCCEEDED(hr) && pAdapter) {
                    DXGI_ADAPTER_DESC desc = {0};
                    hr = pAdapter->lpVtbl->GetDesc(pAdapter, &desc);

                    if (SUCCEEDED(hr)) {
                        // Use DedicatedVideoMemory for dedicated GPUs, or shared for integrated
                        SIZE_T vramBytes = desc.DedicatedVideoMemory;
                        if (vramBytes == 0 && desc.SharedSystemMemory > 0) {
                            // For integrated graphics, use shared memory but cap at reasonable value
                            vramBytes = desc.SharedSystemMemory;
                            if (vramBytes > 512 * 1024 * 1024) {
                                vramBytes = 128 * 1024 * 1024;  // Cap at 128 MB for integrated
                            }
                        }

                        if (vramBytes > 0) {
                            pAdapter->lpVtbl->Release(pAdapter);
                            pFactory->lpVtbl->Release(pFactory);
                            FreeLibrary(hDxgi);

                            double mb = (double)vramBytes / (1024.0 * 1024.0);
                            if (mb >= 1024.0) {
                                double gb = mb / 1024.0;
                                snprintf(buf, buf_size, "%.0f GBytes", gb);
                            } else {
                                snprintf(buf, buf_size, "%.0f MBytes", mb);
                            }
                            return true;
                        }
                    }
                    pAdapter->lpVtbl->Release(pAdapter);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
        }
        FreeLibrary(hDxgi);
    }

    // Fallback to WMI: select PCI adapter with largest AdapterRAM
    // Note: For Intel integrated graphics, query CurrentRefreshRate as a sanity check
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }

    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(
        pSvc, L"WQL",
        L"SELECT AdapterRAM,PNPDeviceID,CurrentRefreshRate FROM Win32_VideoController",
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
        VARIANT vtRam, vtPnp, vtRefresh;
        VariantInit(&vtRam);
        VariantInit(&vtPnp);
        VariantInit(&vtRefresh);

        HRESULT hrRam = IWbemClassObject_Get(pObj, L"AdapterRAM", 0, &vtRam, NULL, NULL);
        HRESULT hrPnp = IWbemClassObject_Get(pObj, L"PNPDeviceID", 0, &vtPnp, NULL, NULL);
        HRESULT hrRefresh = IWbemClassObject_Get(pObj, L"CurrentRefreshRate", 0, &vtRefresh, NULL, NULL);

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

        // Check if adapter is active (has refresh rate)
        BOOL isActive = FALSE;
        if (SUCCEEDED(hrRefresh) && (vtRefresh.vt == VT_I4 || vtRefresh.vt == VT_UI4)) {
            if (vtRefresh.uintVal > 0) {
                isActive = TRUE;
            }
        }

        if (bytes > 0) {
            // Prefer active adapters first, then PCI devices, then highest RAM
            bool better =
                !haveBest ||
                (isActive && !haveBest) ||
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
        VariantClear(&vtRefresh);
        IWbemClassObject_Release(pObj);
    }

    if (pEnum) IEnumWbemClassObject_Release(pEnum);
    if (pSvc) IWbemServices_Release(pSvc);
    CoUninitialize();

    if (!haveBest) {
        return false;
    }

    double mb = (double)bestBytes / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        double gb = mb / 1024.0;
        snprintf(buf, buf_size, "%.0f GBytes", gb);
    } else {
        snprintf(buf, buf_size, "%.0f MBytes", mb);
    }
    return true;
}

// ============================================================================
//  NVAPI dynamic loader for VRAM type/vendor (NVIDIA GPUs only)
//  NVML and WMI do not expose memory type (GDDR6X) or chip vendor (Samsung)
// ============================================================================

typedef int NvAPI_Status;
typedef void *NvPhysicalGpuHandle;

typedef void *(__cdecl *NvAPI_QueryInterface_t)(unsigned int offset);
typedef NvAPI_Status (__cdecl *NvAPI_Initialize_t)(void);
typedef NvAPI_Status (__cdecl *NvAPI_EnumPhysicalGPUs_t)(NvPhysicalGpuHandle *handles, int *count);
typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamType_t)(NvPhysicalGpuHandle handle, int *type);
typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamMaker_t)(NvPhysicalGpuHandle handle, int *maker);

#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_INTERFACE_OFFSET_INITIALIZE         0x0150E828u  // NvAPI_Initialize
#define NVAPI_INTERFACE_OFFSET_ENUM_PHYSICAL_GPUS 0xE5AC921Fu  // NvAPI_EnumPhysicalGPUs
#define NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_TYPE   0x57F7CAAcu  // NvAPI_GPU_GetRamType
#define NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_MAKER  0x42AEA16Au  // NvAPI_GPU_GetRamMaker

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

    *outHandle = handles[0];  // use first physical GPU
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

// VRAM Type: NVAPI NvAPI_GPU_GetRamType() -> ADL MemoryInfo.strMemoryType
bool get_vram_type(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return false;

    // Try NVAPI for NVIDIA GPUs
    NvPhysicalGpuHandle gpu = NULL;
    NvAPI_QueryInterface_t query = NULL;
    HMODULE module = NULL;
    if (nvapi_get_primary_gpu(&gpu, &query, &module)) {
        typedef int NvAPI_Status;
        typedef NvAPI_Status (__cdecl *NvAPI_GPU_GetRamType_t)(NvPhysicalGpuHandle, int*);

        NvAPI_GPU_GetRamType_t NvAPI_GPU_GetRamType =
            (NvAPI_GPU_GetRamType_t)query(NVAPI_INTERFACE_OFFSET_GPU_GET_RAM_TYPE);

        if (NvAPI_GPU_GetRamType) {
            int type = 0;
            NvAPI_Status st = NvAPI_GPU_GetRamType(gpu, &type);
            if (st == 0) {
                const char *typeStr = nvapi_ram_type_to_string(type);
                if (typeStr) {
                    snprintf(buf, buf_size, "%s", typeStr);
                    FreeLibrary(module);
                    return true;
                }
            }
        }
        FreeLibrary(module);
    }

    // Try ADL for AMD GPUs
    ADLContext adl;
    if (try_adl(&adl)) {
        if (adl.ADL_Adapter_MemoryInfo_Get) {
            ADLMemoryInfo memInfo = {0};
            if (adl.ADL_Adapter_MemoryInfo_Get(adl.adapterIndex, &memInfo) == ADL_OK) {
                if (memInfo.strMemoryType[0] != '\0') {
                    snprintf(buf, buf_size, "%s", memInfo.strMemoryType);
                    cleanup_adl(&adl);
                    return true;
                }
            }
        }
        cleanup_adl(&adl);
    }

    // Try Intel IGCL
    IntelContext intel;
    if (try_intel(&intel)) {
        if (intel.ctlGetMemProperties) {
            ctl_mem_properties_t memProps = {0};
            memProps.Size = sizeof(ctl_mem_properties_t);
            memProps.Version = 0;
            if (intel.ctlGetMemProperties(intel.device_handle, &memProps) == CTL_RESULT_SUCCESS) {
                const char *typeStr = intel_mem_type_to_string(memProps.memoryType);
                if (typeStr) {
                    snprintf(buf, buf_size, "%s", typeStr);
                    cleanup_intel(&intel);
                    return true;
                }
            }
        }
        cleanup_intel(&intel);
    }

    return false;
}


// VRAM Vendor: NVAPI NvAPI_GPU_GetRamMaker() (AMD ADL and Intel IGCL do not provide memory vendor)
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
    // Note: ADL and Intel IGCL do not provide memory vendor information
}


// VRAM Bus Width: NVML nvmlDeviceGetMemoryBusWidth() -> Intel IGCL MemProperties (AMD ADL does not provide bus width)
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

    // Try Intel IGCL
    IntelContext intel;
    if (try_intel(&intel)) {
        if (intel.ctlGetMemProperties) {
            ctl_mem_properties_t memProps = {0};
            memProps.Size = sizeof(ctl_mem_properties_t);
            memProps.Version = 0;
            if (intel.ctlGetMemProperties(intel.device_handle, &memProps) == CTL_RESULT_SUCCESS) {
                if (memProps.busWidth > 0) {
                    snprintf(buf, buf_size, "%u bits", memProps.busWidth);
                    cleanup_intel(&intel);
                    return true;
                }
            }
        }
        cleanup_intel(&intel);
    }

    // Note: ADL does not provide bus width information directly
    return false;
}
