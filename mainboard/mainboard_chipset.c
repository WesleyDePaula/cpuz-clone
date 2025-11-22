// mainboard_chipset.c - Implementação das funções de chipset e southbridge
#define _CRT_SECURE_NO_WARNINGS

#include "mainboard_chipset.h"

#include <stdio.h>
#include <string.h>
#include <wctype.h>

#include <wbemidl.h>
#include <oleauto.h>
#include <setupapi.h>
#include <devguid.h>
#include <intrin.h>
#include <winreg.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

// -----------------------------------------------------------------------------
// Helpers gerais
// -----------------------------------------------------------------------------

// Extrai "REV_xx" de um hardware ID e formata em "Rev. xx"
static void extract_revision(const wchar_t* hardwareID, wchar_t* revision, size_t cchRevision)
{
    if (!hardwareID || !revision || cchRevision == 0)
        return;

    const wchar_t* revPtr = wcsstr(hardwareID, L"REV_");
    if (revPtr && wcslen(revPtr) >= 6) {
        wchar_t rev[8] = {0};
        swscanf(revPtr + 4, L"%2ls", rev);
        _snwprintf(revision, cchRevision, L"Rev. %s", rev);
    } else {
        wcsncpy(revision, L"Rev. 00", cchRevision - 1);
        revision[cchRevision - 1] = L'\0';
    }
}

// Obtém vendor-id (AuthenticAMD / GenuineIntel / etc.) via CPUID
static BOOL get_cpu_vendor_id(char* out, size_t outSize)
{
    if (!out || outSize < 13) return FALSE;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);

    // vendor id é armazenado em EBX, EDX, ECX
    memcpy(out + 0, &cpuInfo[1], 4);
    memcpy(out + 4, &cpuInfo[3], 4);
    memcpy(out + 8, &cpuInfo[2], 4);
    out[12] = '\0';

    return TRUE;
}

// Obtém a brand string do processador via CPUID
static BOOL get_cpu_brand_string(char* out, size_t outSize)
{
    if (!out || outSize == 0) return FALSE;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = (unsigned int)cpuInfo[0];
    if (nExIds < 0x80000004)
        return FALSE;

    char brand[64] = {0};
    for (unsigned int i = 0; i < 3; ++i) {
        __cpuid(cpuInfo, 0x80000002 + i);
        memcpy(brand + i * 16, cpuInfo, 16);
    }

    strncpy_s(out, outSize, brand, _TRUNCATE);
    return TRUE;
}

// Converte o vendor-id (AuthenticAMD / GenuineIntel) em um nome amigável
static void get_cpu_vendor_name(wchar_t* vendor, size_t cchVendor)
{
    if (!vendor || cchVendor == 0) return;

    char vendorId[13] = {0};
    if (!get_cpu_vendor_id(vendorId, sizeof(vendorId))) {
        wcsncpy(vendor, L"Unknown", cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
        return;
    }

    const wchar_t* name = L"Unknown";

    if (strcmp(vendorId, "AuthenticAMD") == 0) {
        name = L"AMD";
    } else if (strcmp(vendorId, "GenuineIntel") == 0) {
        name = L"Intel";
    } else if (strcmp(vendorId, "CentaurHauls") == 0) {
        name = L"VIA";
    }

    wcsncpy(vendor, name, cchVendor - 1);
    vendor[cchVendor - 1] = L'\0';
}

// Para plataformas AMD modernas, se o CPU brand contiver "Ryzen", retornamos "Ryzen SOC"
static BOOL get_amd_soc_name(wchar_t* out, size_t cchOut)
{
    if (!out || cchOut == 0) return FALSE;

    char brand[64] = {0};
    if (!get_cpu_brand_string(brand, sizeof(brand)))
        return FALSE;

    if (strstr(brand, "AMD") == NULL)
        return FALSE;

    if (strstr(brand, "Ryzen") != NULL) {
        wcsncpy(out, L"Ryzen SOC", cchOut - 1);
        out[cchOut - 1] = L'\0';
        return TRUE;
    }

    // Genérico para outros AMD sem "Ryzen"
    wcsncpy(out, L"AMD SoC", cchOut - 1);
    out[cchOut - 1] = L'\0';
    return TRUE;
}

// Mapeia VEN_xxxx -> nome de vendor (AMD, Intel, etc.). Usado apenas
// para southbridge; não é uma base de dados de chipsets.
static void get_pci_vendor_name(const wchar_t* hardwareID, wchar_t* vendor, size_t cchVendor)
{
    if (!hardwareID || !vendor || cchVendor == 0) {
        return;
    }

    const wchar_t* venPtr = wcsstr(hardwareID, L"VEN_");
    if (!venPtr || wcslen(venPtr) < 8) {
        wcsncpy(vendor, L"Unknown", cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
        return;
    }

    wchar_t venCodeStr[5] = {0};
    wcsncpy(venCodeStr, venPtr + 4, 4);
    venCodeStr[4] = L'\0';

    unsigned int venCode = 0;
    swscanf(venCodeStr, L"%x", &venCode);

    const wchar_t* name = L"Unknown";
    switch (venCode) {
        case 0x1022: name = L"AMD";   break;
        case 0x8086: name = L"Intel"; break;
        case 0x10DE: name = L"NVIDIA";break;
        case 0x1106: name = L"VIA";   break;
        default:
            break;
    }

    if (wcscmp(name, L"Unknown") != 0) {
        wcsncpy(vendor, name, cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
    } else {
        _snwprintf(vendor, cchVendor, L"VEN_%04X", venCode);
        vendor[cchVendor - 1] = L'\0';
    }
}

// Lê BaseBoardProduct do registry do BIOS:
// HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\BIOS
static BOOL get_baseboard_product(wchar_t* product, size_t cchProduct)
{
    if (!product || cchProduct == 0) return FALSE;

    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                             L"HARDWARE\\DESCRIPTION\\System\\BIOS",
                             0,
                             KEY_READ | KEY_WOW64_64KEY,
                             &hKey);
    if (res != ERROR_SUCCESS) {
        return FALSE;
    }

    DWORD type = 0;
    DWORD size = (DWORD)(cchProduct * sizeof(wchar_t));
    res = RegQueryValueExW(hKey, L"BaseBoardProduct", NULL, &type, (LPBYTE)product, &size);
    RegCloseKey(hKey);

    if (res != ERROR_SUCCESS || type != REG_SZ) {
        return FALSE;
    }

    product[cchProduct - 1] = L'\0';
    return TRUE;
}

// Procura um "código de chipset" genérico dentro do nome da placa,
// como A320, B450, B550, X570, etc. Padrão: [A/B/X][0-9][0-9][0-9].
static BOOL extract_generic_chipset_code(const wchar_t* text, wchar_t* outCode, size_t cchOut)
{
    if (!text || !outCode || cchOut < 5) return FALSE;

    size_t len = wcslen(text);
    for (size_t i = 0; i + 3 < len; ++i) {
        wchar_t c0 = text[i];
        if (c0 == L'A' || c0 == L'B' || c0 == L'X') {
            wchar_t c1 = text[i + 1];
            wchar_t c2 = text[i + 2];
            wchar_t c3 = text[i + 3];
            if (iswdigit(c1) && iswdigit(c2) && iswdigit(c3)) {
                outCode[0] = c0;
                outCode[1] = c1;
                outCode[2] = c2;
                outCode[3] = c3;
                outCode[4] = L'\0';
                return TRUE;
            }
        }
    }
    return FALSE;
}

// -----------------------------------------------------------------------------
// Detecção de CHIPSET (SoC) — usamos CPUID para vendor/model
// e SetupAPI apenas para obter a revisão do dispositivo root.
// -----------------------------------------------------------------------------

static BOOL detect_chipset(ChipsetInfo* info)
{
    if (!info) return FALSE;

    // Primeiro, definimos vendor/model com base na CPU
    wchar_t cpuVendor[64] = {0};
    get_cpu_vendor_name(cpuVendor, _countof(cpuVendor));

    wchar_t model[64] = {0};
    wchar_t revision[16] = {0};
    wcsncpy(revision, L"Rev. 00", _countof(revision) - 1);

    if (wcsstr(cpuVendor, L"AMD") != NULL) {
        // Para AMD, usamos "Ryzen SOC" (ou AMD SoC) no estilo do CPU-Z
        if (!get_amd_soc_name(model, _countof(model))) {
            wcsncpy(model, L"AMD SoC", _countof(model) - 1);
        }
    } else if (wcsstr(cpuVendor, L"Intel") != NULL) {
        // Para Intel, tentamos aproveitar a descrição do root complex
        wcsncpy(model, L"Chipset", _countof(model) - 1);
    } else {
        wcsncpy(model, L"Chipset", _countof(model) - 1);
    }

    model[_countof(model) - 1] = L'\0';

    // Agora, usamos SetupAPI para procurar um dispositivo "root complex"
    // apenas para extrair a revisão (REV_xx)
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL,
                                                  DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        int bestScore = 0;
        wchar_t bestHardwareID[512] = {0};
        wchar_t bestDesc[256]       = {0};

        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
            wchar_t hardwareID[512] = {0};
            wchar_t deviceDesc[256] = {0};

            if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                                                   SPDRP_HARDWAREID, NULL,
                                                   (PBYTE)hardwareID, sizeof(hardwareID), NULL)) {
                continue;
            }

            if (wcsstr(hardwareID, L"VEN_") == NULL) {
                continue;
            }

            if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                                                   SPDRP_DEVICEDESC, NULL,
                                                   (PBYTE)deviceDesc, sizeof(deviceDesc), NULL)) {
                continue;
            }

            int score = 0;

            // Palavras-chave de root complex / host bridge
            if (wcsstr(deviceDesc, L"Root Complex"))          score += 8;
            if (wcsstr(deviceDesc, L"Root Port"))             score += 5;
            if (wcsstr(deviceDesc, L"Root Bridge"))           score += 5;
            if (wcsstr(deviceDesc, L"Host Bridge"))           score += 5;
            if (wcsstr(deviceDesc, L"Host CPU bridge"))       score += 5;
            if (wcsstr(deviceDesc, L"PCI Express Root"))      score += 4;
            if (wcsstr(deviceDesc, L"Complexo da Raiz"))      score += 6; // pt-BR
            if (wcsstr(deviceDesc, L"Controlador de raiz"))   score += 4; // pt-BR

            if (score == 0)
                continue;

            if (score > bestScore) {
                bestScore = score;
                wcsncpy(bestHardwareID, hardwareID, _countof(bestHardwareID) - 1);
                bestHardwareID[_countof(bestHardwareID) - 1] = L'\0';

                wcsncpy(bestDesc, deviceDesc, _countof(bestDesc) - 1);
                bestDesc[_countof(bestDesc) - 1] = L'\0';
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        if (bestScore > 0) {
            // Atualiza revisão a partir do melhor hardwareID encontrado
            extract_revision(bestHardwareID, revision, _countof(revision));

            // Para Intel, se não temos nada melhor para mostrar, usamos a descrição
            if (wcsstr(cpuVendor, L"Intel") != NULL && bestDesc[0] != L'\0') {
                wcsncpy(model, bestDesc, _countof(model) - 1);
                model[_countof(model) - 1] = L'\0';
            }
        }
    }

    // Preenche a struct de saída
    wcsncpy(info->vendor, cpuVendor, _countof(info->vendor) - 1);
    info->vendor[_countof(info->vendor) - 1] = L'\0';

    wcsncpy(info->model, model, _countof(info->model) - 1);
    info->model[_countof(info->model) - 1] = L'\0';

    wcsncpy(info->revision, revision, _countof(info->revision) - 1);
    info->revision[_countof(info->revision) - 1] = L'\0';

    return TRUE;
}

// -----------------------------------------------------------------------------
// Detecção de SOUTHBRIDGE (PCH/FCH) via PCI + nome da placa-mãe
// -----------------------------------------------------------------------------

static BOOL detect_southbridge(ChipsetInfo* info)
{
    if (!info) return FALSE;

    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    DWORD i;

    deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL,
                                         DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    int bestScore = 0;
    wchar_t bestVendor[64]   = {0};
    wchar_t bestModel[64]    = {0};
    wchar_t bestRevision[16] = {0};
    wchar_t bestHardwareID[512] = {0};

    for (i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
        wchar_t hardwareID[512] = {0};
        wchar_t deviceDesc[256] = {0};

        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                                               SPDRP_HARDWAREID, NULL,
                                               (PBYTE)hardwareID, sizeof(hardwareID), NULL)) {
            continue;
        }

        if (wcsstr(hardwareID, L"VEN_") == NULL) {
            continue;
        }

        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData,
                                               SPDRP_DEVICEDESC, NULL,
                                               (PBYTE)deviceDesc, sizeof(deviceDesc), NULL)) {
            continue;
        }

        int score = 0;

        // Elementos típicos do hub da placa (ISA/LPC/SMBus/etc.)
        if (wcsstr(deviceDesc, L"Southbridge"))          score += 8;
        if (wcsstr(deviceDesc, L"PCH"))                  score += 6;
        if (wcsstr(deviceDesc, L"FCH"))                  score += 6;
        if (wcsstr(deviceDesc, L"Platform Controller"))  score += 6;

        if (wcsstr(deviceDesc, L"SMBus"))                score += 5;
        if (wcsstr(deviceDesc, L"LPC"))                  score += 5;
        if (wcsstr(deviceDesc, L"ISA bridge"))           score += 5;
        if (wcsstr(deviceDesc, L"ISA Bridge"))           score += 5;

        if (wcsstr(deviceDesc, L"SATA Controller"))      score += 4;
        if (wcsstr(deviceDesc, L"Serial ATA Controller"))score += 4;
        if (wcsstr(deviceDesc, L"USB Controller"))       score += 3;

        if (score == 0)
            continue;

        if (score > bestScore) {
            bestScore = score;

            wcsncpy(bestHardwareID, hardwareID, _countof(bestHardwareID) - 1);
            bestHardwareID[_countof(bestHardwareID) - 1] = L'\0';

            wcsncpy(bestModel, deviceDesc, _countof(bestModel) - 1);
            bestModel[_countof(bestModel) - 1] = L'\0';

            extract_revision(hardwareID, bestRevision, _countof(bestRevision));

            get_pci_vendor_name(hardwareID, bestVendor, _countof(bestVendor));
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (bestScore <= 0) {
        return FALSE;
    }

    // Tenta descobrir o "código" B550/B450/X570 etc pela placa-mãe.
    wchar_t boardProduct[128] = {0};
    if (get_baseboard_product(boardProduct, _countof(boardProduct))) {
        wchar_t code[8] = {0};
        if (extract_generic_chipset_code(boardProduct, code, _countof(code))) {
            // Se achamos algo como B550, usamos isto como "modelo" do southbridge
            wcsncpy(bestModel, code, _countof(bestModel) - 1);
            bestModel[_countof(bestModel) - 1] = L'\0';
        }
    }

    wcsncpy(info->vendor,   bestVendor,   _countof(info->vendor)   - 1);
    info->vendor[_countof(info->vendor) - 1] = L'\0';

    wcsncpy(info->model,    bestModel,    _countof(info->model)    - 1);
    info->model[_countof(info->model) - 1] = L'\0';

    wcsncpy(info->revision, bestRevision, _countof(info->revision) - 1);
    info->revision[_countof(info->revision) - 1] = L'\0';

    return TRUE;
}

// -----------------------------------------------------------------------------
// API pública
// -----------------------------------------------------------------------------

size_t get_chipset_info(ChipsetInfo* info, size_t max_entries)
{
    if (!info || max_entries == 0)
        return 0;

    ZeroMemory(info, sizeof(ChipsetInfo));

    if (detect_chipset(info))
        return 1;

    // Fallback genérico
    wcsncpy(info->vendor,   L"Unknown", _countof(info->vendor) - 1);
    wcsncpy(info->model,    L"Chipset", _countof(info->model) - 1);
    wcsncpy(info->revision, L"Rev. 00", _countof(info->revision) - 1);
    info->vendor[_countof(info->vendor) - 1]   = L'\0';
    info->model[_countof(info->model) - 1]     = L'\0';
    info->revision[_countof(info->revision) - 1] = L'\0';
    return 1;
}

size_t get_southbridge_info(ChipsetInfo* info, size_t max_entries)
{
    if (!info || max_entries == 0)
        return 0;

    ZeroMemory(info, sizeof(ChipsetInfo));

    if (detect_southbridge(info))
        return 1;

    // Fallback genérico
    wcsncpy(info->vendor,   L"Unknown",     _countof(info->vendor) - 1);
    wcsncpy(info->model,    L"Southbridge", _countof(info->model) - 1);
    wcsncpy(info->revision, L"Rev. 00",     _countof(info->revision) - 1);
    info->vendor[_countof(info->vendor) - 1]   = L'\0';
    info->model[_countof(info->model) - 1]     = L'\0';
    info->revision[_countof(info->revision) - 1] = L'\0';
    return 1;
}

size_t build_chipset_rows(wchar_t labels[][32], wchar_t vendors[][64],
                          wchar_t models[][64], wchar_t revisions[][16],
                          size_t max_rows)
{
    if (!labels || !vendors || !models || !revisions || max_rows < 2)
        return 0;

    size_t count = 0;

    // Linha 0 - Chipset
    ChipsetInfo chipset = {0};
    if (get_chipset_info(&chipset, 1) > 0) {
        wcsncpy(labels[count],   L"Chipset", 31);
        labels[count][31] = L'\0';
        wcsncpy(vendors[count],  chipset.vendor,   63);
        vendors[count][63] = L'\0';
        wcsncpy(models[count],   chipset.model,    63);
        models[count][63] = L'\0';
        wcsncpy(revisions[count], chipset.revision, 15);
        revisions[count][15] = L'\0';
        ++count;
    }

    // Linha 1 - Southbridge
    if (count < max_rows) {
        ChipsetInfo south = {0};
        if (get_southbridge_info(&south, 1) > 0) {
            wcsncpy(labels[count],   L"Southbridge", 31);
            labels[count][31] = L'\0';
            wcsncpy(vendors[count],  south.vendor,   63);
            vendors[count][63] = L'\0';
            wcsncpy(models[count],   south.model,    63);
            models[count][63] = L'\0';
            wcsncpy(revisions[count], south.revision, 15);
            revisions[count][15] = L'\0';
            ++count;
        }
    }

    return count;
}
