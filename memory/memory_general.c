// memory_general.c - implementações para informação geral de memória
//
// Este módulo utiliza a API do Windows e consultas WMI para
// recuperar dados básicos sobre os módulos de memória instalados.
// As funções aqui expostas retornam cadeias de caracteres em ANSI
// contendo o valor solicitado. Quando uma consulta falha, as
// funções retornam false e não modificam o buffer.

#include "memory_general.h"

#define COBJMACROS
#include <windows.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Linkar contra wbemuuid.lib é necessário para usar WMI. O link
// está incluído no script de compilação.
#pragma comment(lib, "wbemuuid.lib")

// Função auxiliar para inicializar COM e conectar-se ao serviço WMI.
// Em caso de sucesso, pSvc aponta para um IWbemServices válido. Em
// caso de falha, retorna false.
static bool init_wmi(IWbemServices **pSvcOut) {
    if (!pSvcOut) return false;
    *pSvcOut = NULL;
    HRESULT hr;

    // Inicializa COM se ainda não estiver inicializado para este thread.
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    // Define níveis de segurança para chamadas WMI.
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              NULL, EOAC_NONE, NULL);
    // RPC_E_TOO_LATE indica que a segurança já foi inicializada anteriormente.
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

    // Conecta ao namespace ROOT\CIMV2.
    IWbemServices *pSvc = NULL;
    hr = IWbemLocator_ConnectServer(pLoc, L"ROOT\\CIMV2", NULL, NULL, 0, 0, NULL, NULL, &pSvc);
    IWbemLocator_Release(pLoc);
    if (FAILED(hr) || !pSvc) {
        CoUninitialize();
        return false;
    }

    // Define o nível de proxy.
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

// Interpreta o código SMBIOS de tipo de memória em uma string legível.
static const char *mem_type_from_code(int code) {
    switch (code) {
        case 0x13: return "SDRAM";      // 19
        case 0x14: return "DDR";        // 20
        case 0x15: return "DDR2";       // 21
        case 0x17: return "DDR3";       // 23? mas 0x18 no SMBIOS? utilizar casos comuns
        case 0x18: return "DDR3";       // 24
        case 0x1A: return "DDR4";       // 26
        case 0x1E: return "DDR5";       // 30
        default:
            // Mapear alguns valores conhecidos conforme a especificação SMBIOS.
            if (code == 19) return "SDRAM";
            if (code == 20) return "DDR";
            if (code == 21) return "DDR2";
            if (code == 22) return "DDR2 FB-DIMM";
            if (code == 24) return "DDR3";
            if (code == 26) return "DDR4";
            if (code == 27) return "LPDDR";
            if (code == 28) return "LPDDR2";
            if (code == 29) return "LPDDR3";
            if (code == 30) return "DDR4"; // algumas implementações retornam 30 para DDR4/5
            return "Unknown";
    }
}

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
    // Itera sobre as instâncias; considera a primeira com valor definido.
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

bool get_memory_size(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    ULONGLONG memKB = 0;
    BOOL ok = GetPhysicallyInstalledSystemMemory(&memKB);
    if (!ok || memKB == 0) {
        return false;
    }
    // Converte kbytes para GiB (1 GiB = 1024*1024 KiB)
    double gib = (double)memKB / (1024.0 * 1024.0);
    // Arredonda para o inteiro mais próximo
    unsigned int gigs = (unsigned int)(gib + 0.5);
    snprintf(buf, buf_size, "%u GBytes", gigs);
    return true;
}

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

