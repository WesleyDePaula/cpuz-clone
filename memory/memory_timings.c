// memory_timings.c - implementações para parâmetros de temporização da memória
//
// Nas plataformas Windows, não existe uma API pública para ler os
// registros SPD de módulos de memória, de onde se obtêm as
// temporizações (CL, tRCD, tRAS, tRC, CR). Em geral, isso requer
// um driver de nível kernel ou acesso direto ao controlador de
// memória. Este módulo tenta obter a frequência efetiva da DRAM
// usando a classe WMI Win32_PhysicalMemory. Os demais valores são
// retornados como desconhecidos.

#include "memory_timings.h"

#define COBJMACROS
#include <windows.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdint.h>

// Reutiliza a função de inicialização do WMI definida no módulo
// memory_general. Para evitar dependência circular, declaramos um
// protótipo aqui. Essa função inicializa COM e obtém um ponteiro
// para IWbemServices.
static bool init_wmi(IWbemServices **pSvcOut);

// Implementação local de init_wmi replicada de memory_general.c.
static bool init_wmi(IWbemServices **pSvcOut) {
    if (!pSvcOut) return false;
    *pSvcOut = NULL;
    HRESULT hr;
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              NULL, EOAC_NONE, NULL);
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

bool get_dram_frequency(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    IWbemServices *pSvc = NULL;
    if (!init_wmi(&pSvc)) {
        return false;
    }
    IEnumWbemClassObject *pEnum = NULL;
    // Tenta obter Speed ou ConfiguredClockSpeed. O valor de Speed
    // corresponde à taxa de transferência (MT/s); metade disso é
    // a frequência real (MHz) da DRAM.
    HRESULT hr = IWbemServices_ExecQuery(pSvc, L"WQL",
        L"SELECT ConfiguredClockSpeed, Speed FROM Win32_PhysicalMemory", WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnum);
    if (FAILED(hr) || !pEnum) {
        IWbemServices_Release(pSvc);
        CoUninitialize();
        return false;
    }
    unsigned int maxSpeed = 0;
    IWbemClassObject *pObj = NULL;
    ULONG uRet = 0;
    while (S_OK == IEnumWbemClassObject_Next(pEnum, WBEM_INFINITE, 1, &pObj, &uRet)) {
        VARIANT vtConf, vtSpd;
        VariantInit(&vtConf);
        VariantInit(&vtSpd);
        hr = IWbemClassObject_Get(pObj, L"ConfiguredClockSpeed", 0, &vtConf, NULL, NULL);
        hr = IWbemClassObject_Get(pObj, L"Speed", 0, &vtSpd, NULL, NULL);
        unsigned int spd = 0;
        if ((vtConf.vt == VT_I4 || vtConf.vt == VT_UI4 || vtConf.vt == VT_I2 || vtConf.vt == VT_UI2) && vtConf.intVal > 0) {
            spd = (unsigned int)vtConf.intVal;
        } else if ((vtSpd.vt == VT_I4 || vtSpd.vt == VT_UI4 || vtSpd.vt == VT_I2 || vtSpd.vt == VT_UI2) && vtSpd.intVal > 0) {
            spd = (unsigned int)vtSpd.intVal;
        }
        if (spd > maxSpeed) maxSpeed = spd;
        VariantClear(&vtConf);
        VariantClear(&vtSpd);
        IWbemClassObject_Release(pObj);
    }
    IEnumWbemClassObject_Release(pEnum);
    IWbemServices_Release(pSvc);
    CoUninitialize();
    if (maxSpeed == 0) {
        return false;
    }
    double realMHz = ((double)maxSpeed) / 2.0;
    snprintf(buf, buf_size, "%.1f MHz", realMHz);
    return true;
}
