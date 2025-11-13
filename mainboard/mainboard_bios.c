// mainboard_bios.c - Implementação das funções de informações da BIOS
#define _CRT_SECURE_NO_WARNINGS
#include "mainboard_bios.h"
#include <stdio.h>
#include <string.h>
#include <wbemidl.h>
#include <oleauto.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// Função auxiliar para executar queries WMI
static BOOL execute_wmi_query(const wchar_t* query, const wchar_t* property, char* buffer, size_t bufsize) {
    HRESULT hr;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    BOOL result = FALSE;

    // Inicializar COM
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return FALSE;
    }

    // Configurar segurança COM
    hr = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        CoUninitialize();
        return FALSE;
    }

    // Obter locator WMI
    hr = CoCreateInstance(
        &CLSID_WbemLocator, 0,
        CLSCTX_INPROC_SERVER,
        &IID_IWbemLocator, (LPVOID*)&pLoc
    );

    if (FAILED(hr)) {
        CoUninitialize();
        return FALSE;
    }

    // Conectar ao namespace WMI
    hr = pLoc->lpVtbl->ConnectServer(pLoc,
        L"ROOT\\CIMV2", NULL, NULL, 0, 0, 0, 0, &pSvc
    );

    if (FAILED(hr)) {
        pLoc->lpVtbl->Release(pLoc);
        CoUninitialize();
        return FALSE;
    }

    // Configurar proxy de segurança
    hr = CoSetProxyBlanket(
        (IUnknown*)pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE
    );

    if (FAILED(hr)) {
        pSvc->lpVtbl->Release(pSvc);
        pLoc->lpVtbl->Release(pLoc);
        CoUninitialize();
        return FALSE;
    }

    // Executar query WMI
    hr = pSvc->lpVtbl->ExecQuery(pSvc,
        L"WQL",
        (BSTR)query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator
    );

    if (FAILED(hr)) {
        pSvc->lpVtbl->Release(pSvc);
        pLoc->lpVtbl->Release(pLoc);
        CoUninitialize();
        return FALSE;
    }

    // Obter resultado
    hr = pEnumerator->lpVtbl->Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);

    if (uReturn > 0) {
        VARIANT vtProp;
        VariantInit(&vtProp);

        hr = pclsObj->lpVtbl->Get(pclsObj, property, 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != NULL) {
            // Converter BSTR para char*
            int len = WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, NULL, 0, NULL, NULL);
            if (len > 0 && (size_t)len <= bufsize) {
                WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, buffer, (int)bufsize, NULL, NULL);
                result = TRUE;
            }
        }
        VariantClear(&vtProp);
        pclsObj->lpVtbl->Release(pclsObj);
    }

    // Cleanup
    pEnumerator->lpVtbl->Release(pEnumerator);
    pSvc->lpVtbl->Release(pSvc);
    pLoc->lpVtbl->Release(pLoc);
    CoUninitialize();

    return result;
}

BOOL get_bios_brand(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    const wchar_t* query = L"SELECT Manufacturer FROM Win32_BIOS";
    if (execute_wmi_query(query, L"Manufacturer", buffer, bufsize)) {
        return TRUE;
    }

    strncpy(buffer, "Unknown", bufsize - 1);
    buffer[bufsize - 1] = '\0';
    return FALSE;
}

BOOL get_bios_version(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    const wchar_t* query = L"SELECT SMBIOSBIOSVersion FROM Win32_BIOS";
    if (execute_wmi_query(query, L"SMBIOSBIOSVersion", buffer, bufsize)) {
        return TRUE;
    }

    strncpy(buffer, "Unknown", bufsize - 1);
    buffer[bufsize - 1] = '\0';
    return FALSE;
}

BOOL get_bios_date(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    const wchar_t* query = L"SELECT ReleaseDate FROM Win32_BIOS";

    // ReleaseDate vem em formato CIM_DATETIME: "YYYYMMDDHHMMSS.MMMMMM+UUU"
    // Conversão para padrão "DD/MM/YYYY"
    char rawDate[64] = {0};
    if (execute_wmi_query(query, L"ReleaseDate", rawDate, sizeof(rawDate))) {
        // Extrair YYYY, MM, DD do formato YYYYMMDD...
        if (strlen(rawDate) >= 8) {
            char year[5] = {0}, month[3] = {0}, day[3] = {0};
            strncpy(year, rawDate, 4);
            strncpy(day, rawDate + 6, 2);
            strncpy(month, rawDate + 4, 2);

            // Formatar como DD/MM/YYYY
            snprintf(buffer, bufsize, "%s/%s/%s", day, month, year);
            return TRUE;
        }
    }

    strncpy(buffer, "Unknown", bufsize - 1);
    buffer[bufsize - 1] = '\0';
    return FALSE;
}
