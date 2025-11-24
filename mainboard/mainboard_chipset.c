// mainboard_chipset.c - Informações de chipset e southbridge
// Detecta o chipset via CPUID (Intel/AMD) e southbridge via dispositivos PCI
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
// Funções auxiliares
// -----------------------------------------------------------------------------

// Pega o número de revisão do hardware ID
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

// Obtém a string do fabricante do processador (AuthenticAMD ou GenuineIntel)
static BOOL get_cpu_vendor_id(char* out, size_t outSize)
{
    if (!out || outSize < 13) return FALSE;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);

    // A string do fabricante vem em 3 registradores
    memcpy(out + 0, &cpuInfo[1], 4);
    memcpy(out + 4, &cpuInfo[3], 4);
    memcpy(out + 8, &cpuInfo[2], 4);
    out[12] = '\0';

    return TRUE;
}

// Obtém o nome completo do processador via CPUID
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

// Converte a string do fabricante para um nome amigável
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

// Para processadores AMD, identifica se é Ryzen ou genérico
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

    // Para outros processadores AMD
    wcsncpy(out, L"AMD SoC", cchOut - 1);
    out[cchOut - 1] = L'\0';
    return TRUE;
}

// Converte o código PCI VEN_xxxx para o nome do fabricante
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

// Tenta identificar o fabricante pelo nome do dispositivo
static void guess_vendor_from_description(const wchar_t* desc,
                                          wchar_t* vendor, size_t cchVendor)
{
    if (!desc || !vendor || cchVendor == 0) return;

    // NVIDIA
    if (wcsstr(desc, L"NVIDIA") != NULL || wcsstr(desc, L"nVidia") != NULL) {
        wcsncpy(vendor, L"NVIDIA", cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
        return;
    }

    // Intel
    if (wcsstr(desc, L"Intel") != NULL || wcsstr(desc, L"INTEL") != NULL) {
        wcsncpy(vendor, L"Intel", cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
        return;
    }

    // AMD
    if (wcsstr(desc, L"AMD") != NULL ||
        wcsstr(desc, L"Advanced Micro Devices") != NULL) {
        wcsncpy(vendor, L"AMD", cchVendor - 1);
        vendor[cchVendor - 1] = L'\0';
        return;
    }
}

// Busca o nome do modelo da placa-mãe no registro do Windows
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

// Identifica a geração/arquitetura do processador Intel via CPUID
static BOOL get_intel_microarch_name(wchar_t* out, size_t cchOut)
{
    if (!out || cchOut == 0) return FALSE;

    char vendorId[13] = {0};
    if (!get_cpu_vendor_id(vendorId, sizeof(vendorId)))
        return FALSE;

    if (strcmp(vendorId, "GenuineIntel") != 0)
        return FALSE;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    unsigned int eax = (unsigned int)cpuInfo[0];

    unsigned int stepping = eax & 0xF;
    unsigned int model = (eax >> 4) & 0xF;
    unsigned int family = (eax >> 8) & 0xF;
    unsigned int extModel = (eax >> 16) & 0xF;
    unsigned int extFamily = (eax >> 20) & 0xFF;

    unsigned int displayFamily = family;
    unsigned int displayModel = model;

    if (family == 0x6) {
        displayModel = (extModel << 4) + model;
    } else if (family == 0xF) {
        displayFamily = family + extFamily;
        displayModel = (extModel << 4) + model;
    }

    const wchar_t* name = NULL;

    // Processadores Intel Core modernos (6ª a 14ª geração)
    if (displayFamily == 0x6) {
        switch (displayModel) {
            // 10ª geração (desktop e notebook)
            case 0xA5:
            case 0xA6:
                name = L"Comet Lake";
                break;

            // 11ª geração (desktop)
            case 0xA7:
                name = L"Rocket Lake";
                break;

            // 12ª geração
            case 0x97:
            case 0x9A:
                name = L"Alder Lake";
                break;

            // 13ª e 14ª geração
            case 0xB7:
            case 0xBA:
            case 0xBF:
                name = L"Raptor Lake";
                break;

            // 10ª geração (notebook)
            case 0x7D:
            case 0x7E:
                name = L"Ice Lake";
                break;

            // 11ª geração (notebook)
            case 0x8C:
            case 0x8D:
                name = L"Tiger Lake";
                break;

            // Kaby Lake (7th gen)
            case 0x8E:
                name = L"Kaby Lake";
                break;

            // 8ª e 9ª geração
            case 0x9E:
                name = L"Coffee Lake";
                break;

            // 6ª geração
            case 0x4E:
            case 0x5E:
                name = L"Skylake";
                break;

            // Broadwell (5th gen)
            case 0x3D:
            case 0x47:
            case 0x4F:
            case 0x56:
                name = L"Broadwell";
                break;

            // Haswell (4th gen)
            case 0x3C:
            case 0x3F:
            case 0x45:
            case 0x46:
                name = L"Haswell";
                break;

            // Ivy Bridge (3rd gen)
            case 0x3A:
            case 0x3E:
                name = L"Ivy Bridge";
                break;

            // Sandy Bridge (2nd gen)
            case 0x2A:
            case 0x2D:
                name = L"Sandy Bridge";
                break;

            default:
                name = L"Intel Core";
                break;
        }
    }

    if (name) {
        wcsncpy(out, name, cchOut - 1);
        out[cchOut - 1] = L'\0';
        return TRUE;
    }

    return TRUE;
}

// Procura códigos de chipset no texto (ex: B550, Z690, X570)
static BOOL extract_generic_chipset_code(const wchar_t* text, wchar_t* outCode, size_t cchOut)
{
    if (!text || !outCode || cchOut < 5) return FALSE;

    size_t len = wcslen(text);
    for (size_t i = 0; i + 3 < len; ++i) {
        wchar_t c0 = text[i];
        if (c0 == L'A' || c0 == L'B' || c0 == L'X' ||
            c0 == L'H' || c0 == L'Z' || c0 == L'Q' || c0 == L'P') {
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

// Extrai o código do chipset da descrição do dispositivo (ex: B460, Z690)
static BOOL extract_chipset_from_description(const wchar_t* desc, wchar_t* outCode, size_t cchOut)
{
    if (!desc || !outCode || cchOut < 5) return FALSE;

    // Procura código entre parênteses primeiro
    const wchar_t* lparen = wcschr(desc, L'(');
    if (lparen) {
        lparen++; // pula o '('
        // Verifica se começa com letra válida seguida de 3 dígitos
        if (wcslen(lparen) >= 4) {
            wchar_t c0 = lparen[0];
            if ((c0 == L'A' || c0 == L'B' || c0 == L'H' || c0 == L'Q' ||
                 c0 == L'X' || c0 == L'Z' || c0 == L'P') &&
                iswdigit(lparen[1]) && iswdigit(lparen[2]) && iswdigit(lparen[3])) {
                outCode[0] = c0;
                outCode[1] = lparen[1];
                outCode[2] = lparen[2];
                outCode[3] = lparen[3];
                outCode[4] = L'\0';
                return TRUE;
            }
        }
    }

    // Se não encontrar, procura no texto completo
    return extract_generic_chipset_code(desc, outCode, cchOut);
}

// -----------------------------------------------------------------------------
// Detecção do CHIPSET principal
// Usa CPUID para identificar a arquitetura e SetupAPI para a revisão
// -----------------------------------------------------------------------------
static BOOL detect_chipset(ChipsetInfo* info)
{
    if (!info) return FALSE;

    // Identifica o fabricante e modelo pelo processador
    wchar_t cpuVendor[64] = {0};
    get_cpu_vendor_name(cpuVendor, _countof(cpuVendor));

    wchar_t model[64]    = {0};
    wchar_t revision[16] = {0};
    wcsncpy(revision, L"Rev. 00", _countof(revision) - 1);

    if (wcsstr(cpuVendor, L"AMD") != NULL) {
        if (!get_amd_soc_name(model, _countof(model))) {
            wcsncpy(model, L"AMD SoC", _countof(model) - 1);
        }
    } else if (wcsstr(cpuVendor, L"Intel") != NULL) {
        // Para Intel, usar microarquitetura (Comet Lake, Alder Lake, etc.)
        if (!get_intel_microarch_name(model, _countof(model))) {
            wcsncpy(model, L"Intel Chipset", _countof(model) - 1);
        }
    } else {
        wcsncpy(model, L"Chipset", _countof(model) - 1);
    }

    model[_countof(model) - 1] = L'\0';

    // Busca dispositivos PCI para encontrar o número de revisão
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL,
                                                  DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        int bestScore = 0;
        wchar_t bestHardwareID[512] = {0};

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

            // Procura por termos que indicam o chipset principal
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
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        if (bestScore > 0) {
            // Extrai o número de revisão do hardware encontrado
            extract_revision(bestHardwareID, revision, _countof(revision));
        }
    }

    // Preenche as informações do chipset
    wcsncpy(info->vendor, cpuVendor, _countof(info->vendor) - 1);
    info->vendor[_countof(info->vendor) - 1] = L'\0';

    wcsncpy(info->model, model, _countof(info->model) - 1);
    info->model[_countof(info->model) - 1] = L'\0';

    wcsncpy(info->revision, revision, _countof(info->revision) - 1);
    info->revision[_countof(info->revision) - 1] = L'\0';

    return TRUE;
}

// -----------------------------------------------------------------------------
// Detecção do SOUTHBRIDGE (PCH/FCH)
// Busca controladores PCI como LPC, SMBus e SATA
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
    wchar_t bestVendor[64]      = {0};
    wchar_t bestModel[64]       = {0};
    wchar_t bestRevision[16]    = {0};
    wchar_t bestHardwareID[512] = {0};
    wchar_t bestDesc[256]       = {0}; // descrição original do dispositivo

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

        // Procura controladores característicos do southbridge
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

            wcsncpy(bestDesc, deviceDesc, _countof(bestDesc) - 1);
            bestDesc[_countof(bestDesc) - 1] = L'\0';

            extract_revision(hardwareID, bestRevision, _countof(bestRevision));

            // vendor inicial via VEN_xxxx
            get_pci_vendor_name(hardwareID, bestVendor, _countof(bestVendor));
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (bestScore <= 0) {
        return FALSE;
    }

    // Se o fabricante não foi identificado, tenta pelo nome do dispositivo
    if (bestVendor[0] == L'\0' ||
        _wcsicmp(bestVendor, L"Unknown") == 0 ||
        wcsstr(bestVendor, L"VEN_") == bestVendor) {
        guess_vendor_from_description(bestDesc, bestVendor, _countof(bestVendor));
    }

    // Tenta pegar o código do chipset da descrição do dispositivo
    wchar_t code[8] = {0};
    if (extract_chipset_from_description(bestDesc, code, _countof(code))) {
        wcsncpy(bestModel, code, _countof(bestModel) - 1);
        bestModel[_countof(bestModel) - 1] = L'\0';
    } else {
        // Se não encontrar, procura no modelo da placa-mãe
        wchar_t boardProduct[128] = {0};
        if (get_baseboard_product(boardProduct, _countof(boardProduct))) {
            if (extract_generic_chipset_code(boardProduct, code, _countof(code))) {
                wcsncpy(bestModel, code, _countof(bestModel) - 1);
                bestModel[_countof(bestModel) - 1] = L'\0';
            }
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
// Funções públicas usadas pela interface
// -----------------------------------------------------------------------------

size_t get_chipset_info(ChipsetInfo* info, size_t max_entries)
{
    if (!info || max_entries == 0)
        return 0;

    ZeroMemory(info, sizeof(ChipsetInfo));

    if (detect_chipset(info))
        return 1;

    // Se falhar, retorna valores padrão
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

    // Se falhar, retorna valores padrão
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

    // Primeira linha: Chipset
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

    // Segunda linha: Southbridge
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
