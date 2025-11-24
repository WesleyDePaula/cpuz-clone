// memory_general.c - Informações gerais sobre a memória RAM
// Usa WMI e API do Windows para obter tipo, tamanho e frequência da memória

#include "memory_general.h"

#define COBJMACROS
#include <windows.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#pragma comment(lib, "wbemuuid.lib")

// Conecta ao WMI do Windows para consultar informações de hardware
static bool init_wmi(IWbemServices **pSvcOut) {
    if (!pSvcOut) return false;
    *pSvcOut = NULL;
    HRESULT hr;

    // Inicializa o sistema COM
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    // Configura níveis de segurança
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              NULL, EOAC_NONE, NULL);
    // Se já estiver inicializado, continua normalmente
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

    // Conecta ao namespace do WMI onde ficam as informações de hardware
    IWbemServices *pSvc = NULL;
    hr = IWbemLocator_ConnectServer(pLoc, L"ROOT\\CIMV2", NULL, NULL, 0, 0, NULL, NULL, &pSvc);
    IWbemLocator_Release(pLoc);
    if (FAILED(hr) || !pSvc) {
        CoUninitialize();
        return false;
    }

    // Configura segurança da conexão
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

// Converte o código numérico da SMBIOS para o nome do tipo de memória
static const char *mem_type_from_code(int code) {
    switch (code) {
        case 0x13: return "SDRAM";      // 19
        case 0x14: return "DDR";        // 20
        case 0x15: return "DDR2";       // 21
        case 0x17: return "DDR3";       // 23
        case 0x18: return "DDR3";       // 24
        case 0x1A: return "DDR4";       // 26
        case 0x1E: return "DDR5";       // 30
        default:
            // Verifica códigos em formato decimal também
            if (code == 19) return "SDRAM";
            if (code == 20) return "DDR";
            if (code == 21) return "DDR2";
            if (code == 22) return "DDR2 FB-DIMM";
            if (code == 24) return "DDR3";
            if (code == 26) return "DDR4";
            if (code == 27) return "LPDDR";
            if (code == 28) return "LPDDR2";
            if (code == 29) return "LPDDR3";
            if (code == 30) return "DDR4";
            if (code == 34) return "DDR5";
            if (code == 35) return "LPDDR5";
            return "Unknown";
    }
}

// Obtém o tipo de memória (DDR3, DDR4, DDR5, etc) via WMI
bool get_memory_type(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }

    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(pSvc, L"WQL",
        L"SELECT SMBIOSMemoryType FROM Win32_PhysicalMemory", WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnum);
    if (FAILED(hr) || !pEnum) {
        IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }

    int typeCode = 0;
    ULONG uReturn = 0;
    IWbemClassObject *pObj = NULL;
    // Percorre os módulos de memória e pega o primeiro válido
    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uReturn)) {
        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = IWbemClassObject_Get(pObj, L"SMBIOSMemoryType", 0, &vtProp, NULL, NULL);
        if (SUCCEEDED(hr)) {
            if ((vtProp.vt == VT_I4 || vtProp.vt == VT_UI4 || vtProp.vt == VT_UI1 || vtProp.vt == VT_UI2) && vtProp.intVal != 0) {
                typeCode = vtProp.intVal;
                VariantClear(&vtProp);
                IWbemClassObject_Release(pObj);
                break;
            }
        }
        VariantClear(&vtProp);
        IWbemClassObject_Release(pObj);
    }

    IEnumWbemClassObject_Release(pEnum);
    IWbemServices_Release(pSvc);
    CoUninitialize();

    const char *typeStr = mem_type_from_code(typeCode);
    snprintf(buf, buf_size, "%s", typeStr);
    return true;
}

// Obtém a quantidade total de memória RAM instalada
bool get_memory_size(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    ULONGLONG memKB = 0;
    BOOL ok = GetPhysicallyInstalledSystemMemory(&memKB);
    if (!ok || memKB == 0) {
        return false;
    }
    // Convert KB to GiB (1 GiB = 1024*1024 KB)
    double gib = (double)memKB / (1024.0 * 1024.0);
    // Round to nearest integer
    unsigned int gigs = (unsigned int)(gib + 0.5);
    snprintf(buf, buf_size, "%u GBytes", gigs);
    return true;
}

// Memory Channel Configuration: WMI Win32_PhysicalMemory.DataWidth count
bool get_memory_channels(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr = IWbemServices_ExecQuery(pSvc, L"WQL",
        L"SELECT DataWidth FROM Win32_PhysicalMemory", WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnum);
    if (FAILED(hr) || !pEnum) {
        IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }
    ULONG uReturn = 0;
    IWbemClassObject *pObj = NULL;
    unsigned int count = 0;
    unsigned int widthBits = 0;
    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uReturn)) {
        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = IWbemClassObject_Get(pObj, L"DataWidth", 0, &vtProp, NULL, NULL);
        if (SUCCEEDED(hr)) {
            if ((vtProp.vt == VT_I4 || vtProp.vt == VT_UI4 || vtProp.vt == VT_I2 || vtProp.vt == VT_UI2) && vtProp.intVal > 0) {
                widthBits = (unsigned int)vtProp.intVal;
            }
        }
        VariantClear(&vtProp);
        IWbemClassObject_Release(pObj);
        count++;
    }
    IEnumWbemClassObject_Release(pEnum);
    IWbemServices_Release(pSvc);
    CoUninitialize();
    if (count == 0 || widthBits == 0) {
        return false;
    }
    snprintf(buf, buf_size, "%u x %u-bit", count, widthBits);
    return true;
}

