// mainboard_chipset.c - Implementação das funções de chipset e southbridge
#define _CRT_SECURE_NO_WARNINGS
#include "mainboard_chipset.h"
#include <stdio.h>
#include <string.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <setupapi.h>
#include <devguid.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "setupapi.lib")

// Função auxiliar para detectar chipset via PCI devices
static BOOL detect_chipset_from_pci(ChipsetInfo* info) {
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    DWORD i;
    BOOL found = FALSE;

    // Obter lista de dispositivos PCI
    deviceInfoSet = SetupDiGetClassDevs(NULL, L"PCI", NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Iterar pelos dispositivos PCI
    for (i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        wchar_t deviceID[256] = {0};
        wchar_t deviceDesc[256] = {0};

        // Obter hardware ID
        if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
            SPDRP_HARDWAREID, NULL, (PBYTE)deviceID, sizeof(deviceID), NULL)) {

            // Procurar por chipset AMD ou Intel
            // Formato típico: PCI\VEN_1022&DEV_XXXX (AMD) ou PCI\VEN_8086&DEV_XXXX (Intel)
            if (wcsstr(deviceID, L"VEN_1022") != NULL) { // AMD
                // Obter descrição do dispositivo
                if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                    SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), NULL)) {

                    // Verificar se é um chipset (procurar por palavras-chave)
                    if (wcsstr(deviceDesc, L"Host Bridge") != NULL ||
                        wcsstr(deviceDesc, L"Ryzen") != NULL ||
                        wcsstr(deviceDesc, L"Root Complex") != NULL) {

                        wcscpy_s(info->vendor, 64, L"AMD");

                        // Extrair número de revisão do device ID
                        wchar_t* revPtr = wcsstr(deviceID, L"REV_");
                        if (revPtr) {
                            wchar_t rev[16];
                            swscanf(revPtr + 4, L"%2s", rev);
                            swprintf(info->revision, 16, L"Rev. %s", rev);
                        } else {
                            wcscpy_s(info->revision, 16, L"Rev. 00");
                        }

                        // Detectar modelo baseado na descrição
                        if (wcsstr(deviceDesc, L"Ryzen") != NULL) {
                            wcscpy_s(info->model, 64, L"Ryzen SOC");
                        } else {
                            wcscpy_s(info->model, 64, L"Chipset");
                        }

                        found = TRUE;
                        break;
                    }
                }
            } else if (wcsstr(deviceID, L"VEN_8086") != NULL) { // Intel
                if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                    SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), NULL)) {

                    if (wcsstr(deviceDesc, L"Host Bridge") != NULL ||
                        wcsstr(deviceDesc, L"Chipset") != NULL) {

                        wcscpy_s(info->vendor, 64, L"Intel");

                        wchar_t* revPtr = wcsstr(deviceID, L"REV_");
                        if (revPtr) {
                            wchar_t rev[16];
                            swscanf(revPtr + 4, L"%2s", rev);
                            swprintf(info->revision, 16, L"Rev. %s", rev);
                        } else {
                            wcscpy_s(info->revision, 16, L"Rev. 00");
                        }

                        wcscpy_s(info->model, 64, L"Chipset");
                        found = TRUE;
                        break;
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return found;
}

// Função auxiliar para detectar southbridge via PCI devices
static BOOL detect_southbridge_from_pci(ChipsetInfo* info) {
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    DWORD i;
    BOOL found = FALSE;

    deviceInfoSet = SetupDiGetClassDevs(NULL, L"PCI", NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        wchar_t deviceID[256] = {0};
        wchar_t deviceDesc[256] = {0};

        if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
            SPDRP_HARDWAREID, NULL, (PBYTE)deviceID, sizeof(deviceID), NULL)) {

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), NULL)) {

                // Procurar por southbridge (USB Controller, SATA Controller, LPC Bridge, etc.)
                if (wcsstr(deviceDesc, L"LPC") != NULL ||
                    wcsstr(deviceDesc, L"SMBus") != NULL ||
                    wcsstr(deviceDesc, L"FCH") != NULL) { // FCH = Fusion Controller Hub (AMD)

                    if (wcsstr(deviceID, L"VEN_1022") != NULL) { // AMD
                        wcscpy_s(info->vendor, 64, L"AMD");

                        // Extrair device ID para identificar o modelo
                        wchar_t* devPtr = wcsstr(deviceID, L"DEV_");
                        if (devPtr) {
                            wchar_t devId[8];
                            swscanf(devPtr + 4, L"%4s", devId);

                            // Mapear device IDs conhecidos para modelos de chipset
                            // A550/B550/X570 etc.
                            if (wcsstr(devId, L"0D20") != NULL || wcsstr(devId, L"0D22") != NULL) {
                                wcscpy_s(info->model, 64, L"B550");
                            } else if (wcsstr(devId, L"0D2C") != NULL) {
                                wcscpy_s(info->model, 64, L"X570");
                            } else if (wcsstr(devId, L"43C6") != NULL || wcsstr(devId, L"43C7") != NULL) {
                                wcscpy_s(info->model, 64, L"A520");
                            } else {
                                wcscpy_s(info->model, 64, L"FCH");
                            }
                        } else {
                            wcscpy_s(info->model, 64, L"FCH");
                        }

                        wchar_t* revPtr = wcsstr(deviceID, L"REV_");
                        if (revPtr) {
                            wchar_t rev[16];
                            swscanf(revPtr + 4, L"%2s", rev);
                            swprintf(info->revision, 16, L"Rev. %s", rev);
                        } else {
                            wcscpy_s(info->revision, 16, L"Rev. 00");
                        }

                        found = TRUE;
                        break;
                    } else if (wcsstr(deviceID, L"VEN_8086") != NULL) { // Intel
                        wcscpy_s(info->vendor, 64, L"Intel");
                        wcscpy_s(info->model, 64, L"PCH");

                        wchar_t* revPtr = wcsstr(deviceID, L"REV_");
                        if (revPtr) {
                            wchar_t rev[16];
                            swscanf(revPtr + 4, L"%2s", rev);
                            swprintf(info->revision, 16, L"Rev. %s", rev);
                        } else {
                            wcscpy_s(info->revision, 16, L"Rev. 00");
                        }

                        found = TRUE;
                        break;
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return found;
}

size_t get_chipset_info(ChipsetInfo* info, size_t max_entries) {
    if (!info || max_entries < 1) return 0;

    memset(info, 0, sizeof(ChipsetInfo));

    if (detect_chipset_from_pci(info)) {
        return 1;
    }

    // Fallback
    wcscpy_s(info->vendor, 64, L"Unknown");
    wcscpy_s(info->model, 64, L"Chipset");
    wcscpy_s(info->revision, 16, L"Rev. 00");
    return 1;
}

size_t get_southbridge_info(ChipsetInfo* info, size_t max_entries) {
    if (!info || max_entries < 1) return 0;

    memset(info, 0, sizeof(ChipsetInfo));

    if (detect_southbridge_from_pci(info)) {
        return 1;
    }

    // Fallback
    wcscpy_s(info->vendor, 64, L"Unknown");
    wcscpy_s(info->model, 64, L"Southbridge");
    wcscpy_s(info->revision, 16, L"Rev. 00");
    return 1;
}

size_t build_chipset_rows(wchar_t labels[][32], wchar_t vendors[][64],
                         wchar_t models[][64], wchar_t revisions[][16], size_t max_rows) {
    if (!labels || !vendors || !models || !revisions || max_rows < 2) return 0;

    size_t count = 0;

    // Chipset
    ChipsetInfo chipset = {0};
    if (get_chipset_info(&chipset, 1) > 0) {
        wcscpy_s(labels[count], 32, L"Chipset");
        wcscpy_s(vendors[count], 64, chipset.vendor);
        wcscpy_s(models[count], 64, chipset.model);
        wcscpy_s(revisions[count], 16, chipset.revision);
        count++;
    }

    // Southbridge
    if (count < max_rows) {
        ChipsetInfo southbridge = {0};
        if (get_southbridge_info(&southbridge, 1) > 0) {
            wcscpy_s(labels[count], 32, L"Southbridge");
            wcscpy_s(vendors[count], 64, southbridge.vendor);
            wcscpy_s(models[count], 64, southbridge.model);
            wcscpy_s(revisions[count], 16, southbridge.revision);
            count++;
        }
    }

    return count;
}
