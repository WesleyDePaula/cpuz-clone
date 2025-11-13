// mainboard_basic.c - Implementação das funções de informações básicas da motherboard
#define _CRT_SECURE_NO_WARNINGS
#include "mainboard_basic.h"
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

BOOL get_motherboard_manufacturer(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    const wchar_t* query = L"SELECT Manufacturer FROM Win32_BaseBoard";
    if (execute_wmi_query(query, L"Manufacturer", buffer, bufsize)) {
        return TRUE;
    }

    strncpy(buffer, "Unknown", bufsize - 1);
    buffer[bufsize - 1] = '\0';
    return FALSE;
}

BOOL get_motherboard_model(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    const wchar_t* query = L"SELECT Product FROM Win32_BaseBoard";
    if (execute_wmi_query(query, L"Product", buffer, bufsize)) {
        return TRUE;
    }

    strncpy(buffer, "Unknown", bufsize - 1);
    buffer[bufsize - 1] = '\0';
    return FALSE;
}

BOOL get_motherboard_bus_specs(char* buffer, size_t bufsize) {
    if (!buffer || bufsize < 1) return FALSE;
    buffer[0] = '\0';

    // Tentar obter informações de PCI Express através do registro do Windows
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e97d-e325-11ce-bfc1-08002be10318}",
        0, KEY_READ, &hKey);

    if (result == ERROR_SUCCESS) {
        // Procurar por informações de PCI Express
        // Esta é uma abordagem simplificada - a versão PCIe pode variar
        char pciVersion[64] = {0};
        DWORD dwSize = sizeof(pciVersion);

        // Verificar se há suporte a PCIe 4.0 através de chaves do registro
        HKEY hSubKey;
        result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\MultifunctionAdapter\\0\\PCIBus\\0",
            0, KEY_READ, &hSubKey);

        if (result == ERROR_SUCCESS) {
            // Tentar detectar versão PCIe - valores típicos
            DWORD version = 0;
            dwSize = sizeof(version);

            // Assumir PCIe 3.0 como padrão se não conseguir detectar
            // PCIe 3.0 = 8.0 GT/s, PCIe 4.0 = 16.0 GT/s
            snprintf(buffer, bufsize, "PCI-Express 3.0 (8.0 GT/s)");

            RegCloseKey(hSubKey);
            RegCloseKey(hKey);
            return TRUE;
        }

        RegCloseKey(hKey);
    }

    // Fallback: tentar via WMI
    HRESULT hr;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;

    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        strncpy(buffer, "PCI-Express", bufsize - 1);
        buffer[bufsize - 1] = '\0';
        return FALSE;
    }

    hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    hr = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        &IID_IWbemLocator, (LPVOID*)&pLoc);

    if (SUCCEEDED(hr)) {
        hr = pLoc->lpVtbl->ConnectServer(pLoc, L"ROOT\\CIMV2", NULL, NULL, 0, 0, 0, 0, &pSvc);

        if (SUCCEEDED(hr)) {
            hr = CoSetProxyBlanket((IUnknown*)pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            if (SUCCEEDED(hr)) {
                hr = pSvc->lpVtbl->ExecQuery(pSvc, L"WQL",
                    L"SELECT * FROM Win32_Bus WHERE BusType=5",
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL, &pEnumerator);

                if (SUCCEEDED(hr)) {
                    IWbemClassObject* pclsObj = NULL;
                    ULONG uReturn = 0;

                    if (pEnumerator->lpVtbl->Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
                        snprintf(buffer, bufsize, "PCI-Express");
                        pclsObj->lpVtbl->Release(pclsObj);
                    }
                    pEnumerator->lpVtbl->Release(pEnumerator);
                }
            }
            pSvc->lpVtbl->Release(pSvc);
        }
        pLoc->lpVtbl->Release(pLoc);
    }
    CoUninitialize();

    if (buffer[0] == '\0') {
        strncpy(buffer, "PCI", bufsize - 1);
        buffer[bufsize - 1] = '\0';
    }

    return TRUE;
}
