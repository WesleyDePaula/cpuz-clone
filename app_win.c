#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>

#include "cpu/cpu_basic.h"
#include "cpu/cpu_cores.h"
#include "cpu/cpu_cache.h"
#include "cpu/cpu_clock.h"
#include "mainboard/mainboard_basic.h"
#include "mainboard/mainboard_chipset.h"
#include "mainboard/mainboard_bios.h"
#include "memory/memory_general.h"
#include "memory/memory_timings.h"
#include "graphics/graphics.h"

#pragma comment(lib, "comctl32.lib")

enum {
    IDC_TAB = 100,
    IDC_GRP_PROC = 200, IDC_GRP_CLOCK = 201, IDC_GRP_CACHE = 202,

    // Processor (LEFT)
    IDC_LBL_VENDOR = 300, IDC_BOX_VENDOR,
    IDC_LBL_NAME,         IDC_BOX_NAME,
    IDC_LBL_PHYS,         IDC_BOX_PHYS,
    IDC_LBL_LOGI,         IDC_BOX_LOGI,

    // Clocks
    IDC_LBL_CLK_CUR = 350, IDC_BOX_CLK_CUR,
    IDC_LBL_CLK_MAX,       IDC_BOX_CLK_MAX,
    IDC_LBL_CLK_LIM,       IDC_BOX_CLK_LIM,

    // Cache (até 4 linhas): label + SIZE + ASSOC
    DC_LBL_C0 = 400, IDC_BOX_C0_SIZE, IDC_BOX_C0_ASSOC,
    DC_LBL_C1,       IDC_BOX_C1_SIZE, IDC_BOX_C1_ASSOC,
    DC_LBL_C2,       IDC_BOX_C2_SIZE, IDC_BOX_C2_ASSOC,
    DC_LBL_C3,       IDC_BOX_C3_SIZE, IDC_BOX_C3_ASSOC,

    // Mainboard tab groups
    IDC_GRP_MOBO = 500, IDC_GRP_BIOS = 501,

    // Motherboard
    IDC_LBL_MANU = 550, IDC_BOX_MANU,
    IDC_LBL_MODEL,      IDC_BOX_MODEL,
    IDC_LBL_BUS,        IDC_BOX_BUS,

    // Chipset (até 2 linhas): label + VENDOR + MODEL + REVISION
    IDC_LBL_CS0 = 600, IDC_BOX_CS0_VENDOR, IDC_BOX_CS0_MODEL, IDC_BOX_CS0_REV,
    IDC_LBL_CS1,       IDC_BOX_CS1_VENDOR, IDC_BOX_CS1_MODEL, IDC_BOX_CS1_REV,

    // BIOS
    IDC_LBL_BIOS_BRAND = 650, IDC_BOX_BIOS_BRAND,
    IDC_LBL_BIOS_VER,         IDC_BOX_BIOS_VER,
    IDC_LBL_BIOS_DATE,        IDC_BOX_BIOS_DATE,

    // Memory tab groups
    IDC_GRP_MEM_GEN = 700, IDC_GRP_MEM_TIM,

    // General memory fields
    IDC_LBL_MEM_TYPE = 720, IDC_BOX_MEM_TYPE,
    IDC_LBL_MEM_SIZE,        IDC_BOX_MEM_SIZE,
    IDC_LBL_MEM_CHANNEL,     IDC_BOX_MEM_CHANNEL,
    IDC_LBL_MEM_FREQ,        IDC_BOX_MEM_FREQ,

    // Timing fields
    IDC_LBL_DRAM_FREQ = 730, IDC_BOX_DRAM_FREQ,

    // Graphics tab groups
    IDC_GRP_GPU = 800, IDC_GRP_VRAM,

    // GPU fields
    IDC_LBL_GPU_NAME,  IDC_BOX_GPU_NAME,
    IDC_LBL_GPU_BOARD, IDC_BOX_GPU_BOARD,
    IDC_LBL_GPU_TDP,   IDC_BOX_GPU_TDP,
    IDC_LBL_GPU_CLOCK, IDC_BOX_GPU_CLOCK,

    // VRAM fields
    IDC_LBL_VRAM_SIZE,      IDC_BOX_VRAM_SIZE,
    IDC_LBL_VRAM_TYPE,      IDC_BOX_VRAM_TYPE,
    IDC_LBL_VRAM_VENDOR,    IDC_BOX_VRAM_VENDOR,
    IDC_LBL_VRAM_BUS_WIDTH, IDC_BOX_VRAM_BUS_WIDTH,

};

static const wchar_t* APP_TITLE = L"Ultra Mega Blaster Alpha Hardware Info: Ultimate 2025 Edition XYZ";
static const wchar_t* WC_MAIN   = L"CPUZ_DEMO_CLASS";

// Controles globais
static HWND hTab;
// CPU tab
static HWND hGroupProc, hGroupClock, hGroupCache;
static HWND hLblVendor, hBoxVendor, hLblName, hBoxName, hLblPhys, hBoxPhys, hLblLogi, hBoxLogi;
static HWND hLblClkCur, hBoxClkCur, hLblClkMax, hBoxClkMax, hLblClkLim, hBoxClkLim;
static HWND hLblCache[4], hBoxCacheSize[4], hBoxCacheAssoc[4];
// Mainboard tab
static HWND hGroupMobo, hGroupBios;
static HWND hLblManu, hBoxManu, hLblModel, hBoxModel, hLblBus, hBoxBus;
static HWND hLblChipset[2], hBoxChipsetVendor[2], hBoxChipsetModel[2], hBoxChipsetRev[2];
static HWND hLblBiosBrand, hBoxBiosBrand, hLblBiosVer, hBoxBiosVer, hLblBiosDate, hBoxBiosDate;

    // Memory tab
    static HWND hGroupMemGeneral;
    // General memory fields
    static HWND hLblMemType, hBoxMemType;
    static HWND hLblMemSize, hBoxMemSize;
    static HWND hLblMemChannel, hBoxMemChannel;
    static HWND hLblMemFreq, hBoxMemFreq;

// Graphics tab
static HWND hGroupGpu, hGroupVram;
// GPU fields
static HWND hLblGpuName,  hBoxGpuName;
static HWND hLblGpuBoard, hBoxGpuBoard;
static HWND hLblGpuTdp,   hBoxGpuTdp;
static HWND hLblGpuClock, hBoxGpuClock;
// VRAM fields
static HWND hLblVramSize,    hBoxVramSize;
static HWND hLblVramType,    hBoxVramType;
static HWND hLblVramVendor,  hBoxVramVendor;
static HWND hLblVramBusWidth, hBoxVramBusWidth;

static void CreateTabs(HWND hwnd) {
    hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD|WS_CLIPSIBLINGS|WS_VISIBLE,
                           0,0,0,0, hwnd, (HMENU)IDC_TAB, GetModuleHandle(NULL), NULL);
    TCITEMW tie = { .mask = TCIF_TEXT };
    tie.pszText = L"CPU";       TabCtrl_InsertItem(hTab, 0, &tie);
    tie.pszText = L"Mainboard"; TabCtrl_InsertItem(hTab, 1, &tie);
    tie.pszText = L"Memory";    TabCtrl_InsertItem(hTab, 2, &tie);
    tie.pszText = L"Graphics";  TabCtrl_InsertItem(hTab, 3, &tie);
    TabCtrl_SetCurSel(hTab, 0);
}

static void DestroyCpuControls(void) {
    HWND arr[] = {
        hLblVendor, hBoxVendor, hLblName, hBoxName, hLblPhys, hBoxPhys, hLblLogi, hBoxLogi,
        hLblClkCur, hBoxClkCur, hLblClkMax, hBoxClkMax, hLblClkLim, hBoxClkLim,
        hLblCache[0], hBoxCacheSize[0], hBoxCacheAssoc[0],
        hLblCache[1], hBoxCacheSize[1], hBoxCacheAssoc[1],
        hLblCache[2], hBoxCacheSize[2], hBoxCacheAssoc[2],
        hLblCache[3], hBoxCacheSize[3], hBoxCacheAssoc[3],
        hGroupProc, hGroupClock, hGroupCache
    };
    for (int i=0;i<(int)(sizeof(arr)/sizeof(arr[0]));++i) if (arr[i]) DestroyWindow(arr[i]);
    ZeroMemory(&hLblCache, sizeof(hLblCache));
    ZeroMemory(&hBoxCacheSize, sizeof(hBoxCacheSize));
    ZeroMemory(&hBoxCacheAssoc, sizeof(hBoxCacheAssoc));
    hGroupProc=hGroupClock=hGroupCache=NULL;
    hLblVendor=hBoxVendor=hLblName=hBoxName=hLblPhys=hBoxPhys=hLblLogi=hBoxLogi=NULL;
    hLblClkCur=hBoxClkCur=hLblClkMax=hBoxClkMax=hLblClkLim=hBoxClkLim=NULL;
}

static void DestroyMainboardControls(void) {
    HWND arr[] = {
        hLblManu, hBoxManu, hLblModel, hBoxModel, hLblBus, hBoxBus,
        hLblChipset[0], hBoxChipsetVendor[0], hBoxChipsetModel[0], hBoxChipsetRev[0],
        hLblChipset[1], hBoxChipsetVendor[1], hBoxChipsetModel[1], hBoxChipsetRev[1],
        hLblBiosBrand, hBoxBiosBrand, hLblBiosVer, hBoxBiosVer, hLblBiosDate, hBoxBiosDate,
        hGroupMobo, hGroupBios
    };
    for (int i=0;i<(int)(sizeof(arr)/sizeof(arr[0]));++i) if (arr[i]) DestroyWindow(arr[i]);
    ZeroMemory(&hLblChipset, sizeof(hLblChipset));
    ZeroMemory(&hBoxChipsetVendor, sizeof(hBoxChipsetVendor));
    ZeroMemory(&hBoxChipsetModel, sizeof(hBoxChipsetModel));
    ZeroMemory(&hBoxChipsetRev, sizeof(hBoxChipsetRev));
    hGroupMobo=hGroupBios=NULL;
    hLblManu=hBoxManu=hLblModel=hBoxModel=hLblBus=hBoxBus=NULL;
    hLblBiosBrand=hBoxBiosBrand=hLblBiosVer=hBoxBiosVer=hLblBiosDate=hBoxBiosDate=NULL;
}

static void Layout(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int margin=8, tabH=28;
    MoveWindow(hTab, margin, margin, rc.right-2*margin, tabH, TRUE);

    int areaX=margin, areaY=margin+tabH+6;
    int areaW=rc.right-2*margin, areaH=rc.bottom-areaY-margin;
    int grpH=(areaH-2*margin)/3;

    if (hGroupProc)  MoveWindow(hGroupProc,  areaX, areaY, areaW, grpH, TRUE);
    if (hGroupClock) MoveWindow(hGroupClock, areaX, areaY+grpH+margin, areaW, grpH, TRUE);
    if (hGroupCache) MoveWindow(hGroupCache, areaX, areaY+2*(grpH+margin), areaW, grpH, TRUE);

    // grid comum
    int padX=12, padY=22, rowH=24, lblW=130, boxW=320, boxH=20;
    int leftX  = areaX + padX;
    int baseY  = areaY + padY;

    // Processor (4 linhas)
    MoveWindow(hLblVendor, leftX, baseY+0*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxVendor, leftX+lblW+6, baseY+0*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblName,   leftX, baseY+1*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxName,   leftX+lblW+6, baseY+1*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblPhys,   leftX, baseY+2*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxPhys,   leftX+lblW+6, baseY+2*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblLogi,   leftX, baseY+3*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxLogi,   leftX+lblW+6, baseY+3*rowH, boxW, boxH, TRUE);

    // Clocks (3 linhas)
    int clkBaseY = areaY + grpH + margin + padY;
    MoveWindow(hLblClkCur, leftX, clkBaseY+0*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxClkCur, leftX+lblW+6, clkBaseY+0*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblClkMax, leftX, clkBaseY+1*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxClkMax, leftX+lblW+6, clkBaseY+1*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblClkLim, leftX, clkBaseY+2*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxClkLim, leftX+lblW+6, clkBaseY+2*rowH, boxW, boxH, TRUE);

    // Cache (duas caixas por linha)
    int cacheBaseY = areaY + 2*(grpH+margin) + padY;
    int sizeBoxW = 220;
    int assocBoxW = 90;
    for (int i=0;i<4;i++) {
        if (hLblCache[i])
            MoveWindow(hLblCache[i], leftX, cacheBaseY + i*rowH, lblW, boxH, TRUE);
        if (hBoxCacheSize[i])
            MoveWindow(hBoxCacheSize[i], leftX + lblW + 6, cacheBaseY + i*rowH, sizeBoxW, boxH, TRUE);
        if (hBoxCacheAssoc[i])
            MoveWindow(hBoxCacheAssoc[i], leftX + lblW + 6 + sizeBoxW + 10, cacheBaseY + i*rowH, assocBoxW, boxH, TRUE);
    }

    // Se a aba de memória estiver ativa (grupo de memória criado), posiciona controles
    if (hGroupMemGeneral) {
        // Usa um único grupo ocupando toda a área disponível
        int memGrpH = areaH;
        MoveWindow(hGroupMemGeneral, areaX, areaY, areaW, memGrpH, TRUE);

        // Coordenadas comuns
        int memPadX = 12;
        int memPadY = 22;
        int memRowH = 24;
        int memLblW = 130;
        int memBoxW = 320;
        int memBoxH = 20;

        int memLeftX = areaX + memPadX;
        int baseGenY = areaY + memPadY;
        // Linhas gerais: tipo, tamanho, canais, frequência
        if (hLblMemType)
            MoveWindow(hLblMemType,  memLeftX, baseGenY + 0*memRowH, memLblW, memBoxH, TRUE);
        if (hBoxMemType)
            MoveWindow(hBoxMemType,  memLeftX + memLblW + 6, baseGenY + 0*memRowH, memBoxW, memBoxH, TRUE);

        if (hLblMemSize)
            MoveWindow(hLblMemSize,  memLeftX, baseGenY + 1*memRowH, memLblW, memBoxH, TRUE);
        if (hBoxMemSize)
            MoveWindow(hBoxMemSize,  memLeftX + memLblW + 6, baseGenY + 1*memRowH, memBoxW, memBoxH, TRUE);

        if (hLblMemChannel)
            MoveWindow(hLblMemChannel,  memLeftX, baseGenY + 2*memRowH, memLblW, memBoxH, TRUE);
        if (hBoxMemChannel)
            MoveWindow(hBoxMemChannel,  memLeftX + memLblW + 6, baseGenY + 2*memRowH, memBoxW, memBoxH, TRUE);

        if (hLblMemFreq)
            MoveWindow(hLblMemFreq,  memLeftX, baseGenY + 3*memRowH, memLblW, memBoxH, TRUE);
        if (hBoxMemFreq)
            MoveWindow(hBoxMemFreq,  memLeftX + memLblW + 6, baseGenY + 3*memRowH, memBoxW, memBoxH, TRUE);

        // Não há painel separado de Timings — referências removidas

    }

    // Se a aba de gráficos estiver ativa (grupos de gráficos criados), posiciona controles
    if (hGroupGpu) {
        // Duas zonas verticais: GPU e VRAM
        int padX = 12, padY = 22, rowH = 24;
        int lblW = 130, boxW = 320, boxH = 20;
        int grpH = (areaH - margin) / 2;
        // Group boxes
        if (hGroupGpu)
            MoveWindow(hGroupGpu, areaX, areaY, areaW, grpH, TRUE);
        if (hGroupVram)
            MoveWindow(hGroupVram, areaX, areaY + grpH + margin, areaW, grpH, TRUE);

        int leftX = areaX + padX;
        // GPU group fields
        int gpuBaseY = areaY + padY;
        if (hLblGpuName)
            MoveWindow(hLblGpuName,  leftX, gpuBaseY + 0*rowH, lblW, boxH, TRUE);
        if (hBoxGpuName)
            MoveWindow(hBoxGpuName,  leftX + lblW + 6, gpuBaseY + 0*rowH, boxW, boxH, TRUE);
        if (hLblGpuBoard)
            MoveWindow(hLblGpuBoard, leftX, gpuBaseY + 1*rowH, lblW, boxH, TRUE);
        if (hBoxGpuBoard)
            MoveWindow(hBoxGpuBoard, leftX + lblW + 6, gpuBaseY + 1*rowH, boxW, boxH, TRUE);
        if (hLblGpuTdp)
            MoveWindow(hLblGpuTdp,   leftX, gpuBaseY + 2*rowH, lblW, boxH, TRUE);
        if (hBoxGpuTdp)
            MoveWindow(hBoxGpuTdp,   leftX + lblW + 6, gpuBaseY + 2*rowH, boxW, boxH, TRUE);
        if (hLblGpuClock)
            MoveWindow(hLblGpuClock, leftX, gpuBaseY + 3*rowH, lblW, boxH, TRUE);
        if (hBoxGpuClock)
            MoveWindow(hBoxGpuClock, leftX + lblW + 6, gpuBaseY + 3*rowH, boxW, boxH, TRUE);

        // VRAM group fields
        int vramBaseY = areaY + grpH + margin + padY;
        if (hLblVramSize)
            MoveWindow(hLblVramSize,     leftX, vramBaseY + 0*rowH, lblW, boxH, TRUE);
        if (hBoxVramSize)
            MoveWindow(hBoxVramSize,     leftX + lblW + 6, vramBaseY + 0*rowH, boxW, boxH, TRUE);
        if (hLblVramType)
            MoveWindow(hLblVramType,     leftX, vramBaseY + 1*rowH, lblW, boxH, TRUE);
        if (hBoxVramType)
            MoveWindow(hBoxVramType,     leftX + lblW + 6, vramBaseY + 1*rowH, boxW, boxH, TRUE);
        if (hLblVramVendor)
            MoveWindow(hLblVramVendor,   leftX, vramBaseY + 2*rowH, lblW, boxH, TRUE);
        if (hBoxVramVendor)
            MoveWindow(hBoxVramVendor,   leftX + lblW + 6, vramBaseY + 2*rowH, boxW, boxH, TRUE);
        if (hLblVramBusWidth)
            MoveWindow(hLblVramBusWidth, leftX, vramBaseY + 3*rowH, lblW, boxH, TRUE);
        if (hBoxVramBusWidth)
            MoveWindow(hBoxVramBusWidth, leftX + lblW + 6, vramBaseY + 3*rowH, boxW, boxH, TRUE);
    }
}

// Libera e remove todos os controles da aba de memória.
static void DestroyMemoryControls(void) {
    HWND arr[] = {
        hLblMemType, hBoxMemType, hLblMemSize, hBoxMemSize, hLblMemChannel, hBoxMemChannel, hLblMemFreq, hBoxMemFreq,
        hGroupMemGeneral
    };
    for (int i=0; i<(int)(sizeof(arr)/sizeof(arr[0])); ++i) {
        if (arr[i]) DestroyWindow(arr[i]);
    }
    hGroupMemGeneral = NULL;
    hLblMemType=hBoxMemType=hLblMemSize=hBoxMemSize=hLblMemChannel=hBoxMemChannel=hLblMemFreq=hBoxMemFreq=NULL;
}

// Release and remove all controls of the graphics tab.
static void DestroyGraphicsControls(void) {
    HWND arr[] = {
        hLblGpuName, hBoxGpuName, hLblGpuBoard, hBoxGpuBoard,
        hLblGpuTdp, hBoxGpuTdp, hLblGpuClock, hBoxGpuClock,
        hLblVramSize, hBoxVramSize, hLblVramType, hBoxVramType,
        hLblVramVendor, hBoxVramVendor, hLblVramBusWidth, hBoxVramBusWidth,
        hGroupGpu, hGroupVram
    };
    for (int i=0; i<(int)(sizeof(arr)/sizeof(arr[0])); ++i) {
        if (arr[i]) DestroyWindow(arr[i]);
    }
    hGroupGpu = hGroupVram = NULL;
    hLblGpuName = hBoxGpuName = hLblGpuBoard = hBoxGpuBoard = NULL;
    hLblGpuTdp  = hBoxGpuTdp  = hLblGpuClock = hBoxGpuClock = NULL;
    hLblVramSize = hBoxVramSize = hLblVramType = hBoxVramType = NULL;
    hLblVramVendor = hBoxVramVendor = hLblVramBusWidth = hBoxVramBusWidth = NULL;
}

// Cria os controles da aba de memória e preenche os valores consultando o sistema.
static void ShowMemoryTab(HWND hwnd) {
    // Remove outros controles para evitar sobreposição
    DestroyCpuControls();
    DestroyMainboardControls();
    DestroyMemoryControls();

    // Cria grupos
    hGroupMemGeneral = CreateWindowExW(0, L"BUTTON", L"General", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                                       0,0,0,0, hwnd, (HMENU)IDC_GRP_MEM_GEN, GetModuleHandle(NULL), NULL);

    // Campos gerais
    hLblMemType    = CreateWindowExW(0, L"STATIC", L"Type",    WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, hwnd, (HMENU)IDC_LBL_MEM_TYPE, GetModuleHandle(NULL), NULL);
    hBoxMemType    = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY, 0,0,0,0, hwnd, (HMENU)IDC_BOX_MEM_TYPE, GetModuleHandle(NULL), NULL);

    hLblMemSize    = CreateWindowExW(0, L"STATIC", L"Size",    WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, hwnd, (HMENU)IDC_LBL_MEM_SIZE, GetModuleHandle(NULL), NULL);
    hBoxMemSize    = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY, 0,0,0,0, hwnd, (HMENU)IDC_BOX_MEM_SIZE, GetModuleHandle(NULL), NULL);

    hLblMemChannel = CreateWindowExW(0, L"STATIC", L"Channel #", WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, hwnd, (HMENU)IDC_LBL_MEM_CHANNEL, GetModuleHandle(NULL), NULL);
    hBoxMemChannel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY, 0,0,0,0, hwnd, (HMENU)IDC_BOX_MEM_CHANNEL, GetModuleHandle(NULL), NULL);

    hLblMemFreq    = CreateWindowExW(0, L"STATIC", L"DRAM Frequency", WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, hwnd, (HMENU)IDC_LBL_MEM_FREQ, GetModuleHandle(NULL), NULL);
    hBoxMemFreq    = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY, 0,0,0,0, hwnd, (HMENU)IDC_BOX_MEM_FREQ, GetModuleHandle(NULL), NULL);

    // (timings group removed)


    // Ajusta posição inicial
    Layout(hwnd);

    // Preenche campos gerais
    char tmpA[128];
    if (get_memory_type(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[128]; mbstowcs(tmpW, tmpA, 127); tmpW[127] = L'\0';
        SetWindowTextW(hBoxMemType, tmpW);
    } else {
        SetWindowTextW(hBoxMemType, L"Unknown");
    }
    if (get_memory_size(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[128]; mbstowcs(tmpW, tmpA, 127); tmpW[127] = L'\0';
        SetWindowTextW(hBoxMemSize, tmpW);
    } else {
        SetWindowTextW(hBoxMemSize, L"Unknown");
    }
    if (get_memory_channels(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[128]; mbstowcs(tmpW, tmpA, 127); tmpW[127] = L'\0';
        SetWindowTextW(hBoxMemChannel, tmpW);
    } else {
        SetWindowTextW(hBoxMemChannel, L"Unknown");
    }
    if (get_dram_frequency(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[128]; mbstowcs(tmpW, tmpA, 127); tmpW[127] = L'\0';
        SetWindowTextW(hBoxMemFreq, tmpW);
    } else {
        SetWindowTextW(hBoxMemFreq, L"Unknown");
    }

}

// Creates the controls for the graphics tab and populates them with
// information retrieved from the system.  The tab contains two groups:
// one for the GPU itself (name, board manufacturer, TDP, base clock) and
// another for the video memory (size, type, vendor, bus width).  Unknown
// values are displayed as "Unknown".  All controls are created as
// children of the main window (not the group boxes) to simplify layout.
static void ShowGraphicsTab(HWND hwnd) {
    // Destroy other tabs to avoid overlap
    DestroyCpuControls();
    DestroyMainboardControls();
    DestroyMemoryControls();
    DestroyGraphicsControls();

    // Create group boxes
    hGroupGpu  = CreateWindowExW(0, L"BUTTON", L"GPU", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                                 0,0,0,0, hwnd, (HMENU)IDC_GRP_GPU, GetModuleHandle(NULL), NULL);
    hGroupVram = CreateWindowExW(0, L"BUTTON", L"Memory", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                                 0,0,0,0, hwnd, (HMENU)IDC_GRP_VRAM, GetModuleHandle(NULL), NULL);

    // GPU fields
    hLblGpuName  = CreateWindowExW(0, L"STATIC", L"Name", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                   0,0,0,0, hwnd, (HMENU)IDC_LBL_GPU_NAME, GetModuleHandle(NULL), NULL);
    hBoxGpuName  = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                   0,0,0,0, hwnd, (HMENU)IDC_BOX_GPU_NAME, GetModuleHandle(NULL), NULL);
    hLblGpuBoard = CreateWindowExW(0, L"STATIC", L"Board Manuf.", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                   0,0,0,0, hwnd, (HMENU)IDC_LBL_GPU_BOARD, GetModuleHandle(NULL), NULL);
    hBoxGpuBoard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                   0,0,0,0, hwnd, (HMENU)IDC_BOX_GPU_BOARD, GetModuleHandle(NULL), NULL);
    hLblGpuTdp   = CreateWindowExW(0, L"STATIC", L"TDP", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                   0,0,0,0, hwnd, (HMENU)IDC_LBL_GPU_TDP, GetModuleHandle(NULL), NULL);
    hBoxGpuTdp   = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                   0,0,0,0, hwnd, (HMENU)IDC_BOX_GPU_TDP, GetModuleHandle(NULL), NULL);
    hLblGpuClock = CreateWindowExW(0, L"STATIC", L"Clock", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                   0,0,0,0, hwnd, (HMENU)IDC_LBL_GPU_CLOCK, GetModuleHandle(NULL), NULL);
    hBoxGpuClock = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                   0,0,0,0, hwnd, (HMENU)IDC_BOX_GPU_CLOCK, GetModuleHandle(NULL), NULL);

    // VRAM fields
    hLblVramSize      = CreateWindowExW(0, L"STATIC", L"Size", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                        0,0,0,0, hwnd, (HMENU)IDC_LBL_VRAM_SIZE, GetModuleHandle(NULL), NULL);
    hBoxVramSize      = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                        0,0,0,0, hwnd, (HMENU)IDC_BOX_VRAM_SIZE, GetModuleHandle(NULL), NULL);
    hLblVramType      = CreateWindowExW(0, L"STATIC", L"Type", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                        0,0,0,0, hwnd, (HMENU)IDC_LBL_VRAM_TYPE, GetModuleHandle(NULL), NULL);
    hBoxVramType      = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                        0,0,0,0, hwnd, (HMENU)IDC_BOX_VRAM_TYPE, GetModuleHandle(NULL), NULL);
    hLblVramVendor    = CreateWindowExW(0, L"STATIC", L"Vendor", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                        0,0,0,0, hwnd, (HMENU)IDC_LBL_VRAM_VENDOR, GetModuleHandle(NULL), NULL);
    hBoxVramVendor    = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                        0,0,0,0, hwnd, (HMENU)IDC_BOX_VRAM_VENDOR, GetModuleHandle(NULL), NULL);
    hLblVramBusWidth  = CreateWindowExW(0, L"STATIC", L"Bus Width", WS_CHILD|WS_VISIBLE|SS_LEFT,
                                        0,0,0,0, hwnd, (HMENU)IDC_LBL_VRAM_BUS_WIDTH, GetModuleHandle(NULL), NULL);
    hBoxVramBusWidth  = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_READONLY,
                                        0,0,0,0, hwnd, (HMENU)IDC_BOX_VRAM_BUS_WIDTH, GetModuleHandle(NULL), NULL);

    // Layout once to place group boxes before filling contents
    Layout(hwnd);

    // Fill GPU fields
    char tmpA[256];
    if (get_gpu_name(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[256]; mbstowcs(tmpW, tmpA, 255); tmpW[255] = L'\0';
        SetWindowTextW(hBoxGpuName, tmpW);
    } else {
        SetWindowTextW(hBoxGpuName, L"Unknown");
    }
    if (get_gpu_board_manufacturer(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[256]; mbstowcs(tmpW, tmpA, 255); tmpW[255] = L'\0';
        SetWindowTextW(hBoxGpuBoard, tmpW);
    } else {
        SetWindowTextW(hBoxGpuBoard, L"Unknown");
    }
    if (get_gpu_tdp(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxGpuTdp, tmpW);
    } else {
        SetWindowTextW(hBoxGpuTdp, L"Unknown");
    }
    if (get_gpu_base_clock(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxGpuClock, tmpW);
    } else {
        SetWindowTextW(hBoxGpuClock, L"Unknown");
    }

    // Fill VRAM fields
    if (get_vram_size(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxVramSize, tmpW);
    } else {
        SetWindowTextW(hBoxVramSize, L"Unknown");
    }
    if (get_vram_type(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxVramType, tmpW);
    } else {
        SetWindowTextW(hBoxVramType, L"Unknown");
    }
    if (get_vram_vendor(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxVramVendor, tmpW);
    } else {
        SetWindowTextW(hBoxVramVendor, L"Unknown");
    }
    if (get_vram_bus_width(tmpA, sizeof(tmpA))) {
        wchar_t tmpW[64]; mbstowcs(tmpW, tmpA, 63); tmpW[63] = L'\0';
        SetWindowTextW(hBoxVramBusWidth, tmpW);
    } else {
        SetWindowTextW(hBoxVramBusWidth, L"Unknown");
    }

    // Final layout update to position the filled controls
    Layout(hwnd);
}

static void ShowBlankTab(HWND hwnd) {
    DestroyCpuControls();
    DestroyMainboardControls();
    DestroyMemoryControls();
    DestroyGraphicsControls();
}

static void ShowCpuTab(HWND hwnd) {

    // group boxes
    hGroupProc  = CreateWindowExW(0,L"BUTTON",L"Processor",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,0,0,hwnd,(HMENU)IDC_GRP_PROC,GetModuleHandle(NULL),NULL);
    hGroupClock = CreateWindowExW(0,L"BUTTON",L"Clocks",   WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,0,0,hwnd,(HMENU)IDC_GRP_CLOCK,GetModuleHandle(NULL),NULL);
    hGroupCache = CreateWindowExW(0,L"BUTTON",L"Cache",    WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,0,0,hwnd,(HMENU)IDC_GRP_CACHE,GetModuleHandle(NULL),NULL);

    // Processor — labels + caixas
    hLblVendor = CreateWindowExW(0,L"STATIC",L"Vendor",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_VENDOR,GetModuleHandle(NULL),NULL);
    hBoxVendor = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_VENDOR,GetModuleHandle(NULL),NULL);

    hLblName   = CreateWindowExW(0,L"STATIC",L"Name",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_NAME,GetModuleHandle(NULL),NULL);
    hBoxName   = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_NAME,GetModuleHandle(NULL),NULL);

    hLblPhys   = CreateWindowExW(0,L"STATIC",L"Physical cores",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_PHYS,GetModuleHandle(NULL),NULL);
    hBoxPhys   = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_PHYS,GetModuleHandle(NULL),NULL);

    hLblLogi   = CreateWindowExW(0,L"STATIC",L"Logical processors",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_LOGI,GetModuleHandle(NULL),NULL);
    hBoxLogi   = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_LOGI,GetModuleHandle(NULL),NULL);

    // Clocks — labels + caixas
    hLblClkCur = CreateWindowExW(0,L"STATIC",L"Current",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_CLK_CUR,GetModuleHandle(NULL),NULL);
    hBoxClkCur = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_CLK_CUR,GetModuleHandle(NULL),NULL);

    hLblClkMax = CreateWindowExW(0,L"STATIC",L"Max",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_CLK_MAX,GetModuleHandle(NULL),NULL);
    hBoxClkMax = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_CLK_MAX,GetModuleHandle(NULL),NULL);

    hLblClkLim = CreateWindowExW(0,L"STATIC",L"Limit",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_CLK_LIM,GetModuleHandle(NULL),NULL);
    hBoxClkLim = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_CLK_LIM,GetModuleHandle(NULL),NULL);

    // Cache — 4 linhas: label + [SIZE box] + [ASSOC box]
    for (int i=0;i<4;i++) {
        hLblCache[i]      = CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT,
                                            0,0,0,0, hwnd,(HMENU)(DC_LBL_C0 + i*3), GetModuleHandle(NULL),NULL);
        hBoxCacheSize[i]  = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,
                                            0,0,0,0, hwnd,(HMENU)(IDC_BOX_C0_SIZE + i*3), GetModuleHandle(NULL),NULL);
        hBoxCacheAssoc[i] = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,
                                            0,0,0,0, hwnd,(HMENU)(IDC_BOX_C0_ASSOC + i*3), GetModuleHandle(NULL),NULL);
    }


    // Layout
    Layout(hwnd);

    // Preencher Processor
    char vendorA[13]={0}; get_cpu_vendor(vendorA);
    wchar_t vendorW[32]; mbstowcs(vendorW, vendorA, 31); vendorW[31]=L'\0';
    SetWindowTextW(hBoxVendor, vendorW);

    char brandA[49]={0}; get_cpu_brand(brandA);
    wchar_t brandW[128]; mbstowcs(brandW, brandA, 127); brandW[127]=L'\0';
    SetWindowTextW(hBoxName, brandW);

    wchar_t tmp[64];
    _snwprintf(tmp,64,L"%lu",(unsigned long)count_physical_cores()); SetWindowTextW(hBoxPhys,tmp);
    _snwprintf(tmp,64,L"%lu",(unsigned long)count_logical_processors()); SetWindowTextW(hBoxLogi,tmp);

    // Preencher Clocks
    DWORD cur=0,max=0,lim=0;
    if (get_cpu0_clock(&cur,&max,&lim)) {
        _snwprintf(tmp,64,L"%lu MHz",(unsigned long)cur); SetWindowTextW(hBoxClkCur,tmp);
        _snwprintf(tmp,64,L"%lu MHz",(unsigned long)max); SetWindowTextW(hBoxClkMax,tmp);
        _snwprintf(tmp,64,L"%lu MHz",(unsigned long)lim); SetWindowTextW(hBoxClkLim,tmp);
    } else {
        SetWindowTextW(hBoxClkCur,L"N/A");
        SetWindowTextW(hBoxClkMax,L"N/A");
        SetWindowTextW(hBoxClkLim,L"N/A");
    }

    // Preencher Cache
    wchar_t labels[4][32], sizes[4][32], assoc[4][16];
    size_t n = build_cache_rows_kv2(labels, sizes, assoc, 4);
    for (size_t i=0;i<n && i<4;i++) {
        SetWindowTextW(hLblCache[i],      labels[i]);
        SetWindowTextW(hBoxCacheSize[i],  sizes[i]);
        SetWindowTextW(hBoxCacheAssoc[i], assoc[i]);
        ShowWindow(hLblCache[i], SW_SHOW);
        ShowWindow(hBoxCacheSize[i], SW_SHOW);
        ShowWindow(hBoxCacheAssoc[i], SW_SHOW);
    }
    for (size_t i=n;i<4;i++) {
        ShowWindow(hLblCache[i], SW_HIDE);
        ShowWindow(hBoxCacheSize[i], SW_HIDE);
        ShowWindow(hBoxCacheAssoc[i], SW_HIDE);
    }
}

static void ShowMainboardTab(HWND hwnd) {
    DestroyCpuControls();
    DestroyMainboardControls();

    // group boxes
    hGroupMobo = CreateWindowExW(0,L"BUTTON",L"Motherboard",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,0,0,hwnd,(HMENU)IDC_GRP_MOBO,GetModuleHandle(NULL),NULL);
    hGroupBios = CreateWindowExW(0,L"BUTTON",L"BIOS",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,0,0,hwnd,(HMENU)IDC_GRP_BIOS,GetModuleHandle(NULL),NULL);

    // Motherboard — labels + caixas
    hLblManu  = CreateWindowExW(0,L"STATIC",L"Manufacturer",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_MANU,GetModuleHandle(NULL),NULL);
    hBoxManu  = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_MANU,GetModuleHandle(NULL),NULL);

    hLblModel = CreateWindowExW(0,L"STATIC",L"Model",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_MODEL,GetModuleHandle(NULL),NULL);
    hBoxModel = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_MODEL,GetModuleHandle(NULL),NULL);

    hLblBus   = CreateWindowExW(0,L"STATIC",L"Bus Specs.",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_BUS,GetModuleHandle(NULL),NULL);
    hBoxBus   = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_BUS,GetModuleHandle(NULL),NULL);

    // Chipset — 2 linhas: label + [VENDOR box] + [MODEL box] + [REV box]
    for (int i=0;i<2;i++) {
        hLblChipset[i]       = CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT,
                                               0,0,0,0, hwnd,(HMENU)(IDC_LBL_CS0 + i*4), GetModuleHandle(NULL),NULL);
        hBoxChipsetVendor[i] = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,
                                               0,0,0,0, hwnd,(HMENU)(IDC_BOX_CS0_VENDOR + i*4), GetModuleHandle(NULL),NULL);
        hBoxChipsetModel[i]  = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,
                                               0,0,0,0, hwnd,(HMENU)(IDC_BOX_CS0_MODEL + i*4), GetModuleHandle(NULL),NULL);
        hBoxChipsetRev[i]    = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,
                                               0,0,0,0, hwnd,(HMENU)(IDC_BOX_CS0_REV + i*4), GetModuleHandle(NULL),NULL);
    }

    // BIOS — labels + caixas
    hLblBiosBrand = CreateWindowExW(0,L"STATIC",L"Brand",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_BIOS_BRAND,GetModuleHandle(NULL),NULL);
    hBoxBiosBrand = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_BIOS_BRAND,GetModuleHandle(NULL),NULL);

    hLblBiosVer   = CreateWindowExW(0,L"STATIC",L"Version",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_BIOS_VER,GetModuleHandle(NULL),NULL);
    hBoxBiosVer   = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_BIOS_VER,GetModuleHandle(NULL),NULL);

    hLblBiosDate  = CreateWindowExW(0,L"STATIC",L"Date",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,(HMENU)IDC_LBL_BIOS_DATE,GetModuleHandle(NULL),NULL);
    hBoxBiosDate  = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,0,hwnd,(HMENU)IDC_BOX_BIOS_DATE,GetModuleHandle(NULL),NULL);

    // Layout
    RECT rc; GetClientRect(hwnd, &rc);
    int margin=8, tabH=28;
    int areaX=margin, areaY=margin+tabH+6;
    int areaW=rc.right-2*margin, areaH=rc.bottom-areaY-margin;
    int grpH=(areaH-margin)/2;

    // Dois grupos: Motherboard (em cima) e BIOS (embaixo)
    MoveWindow(hGroupMobo, areaX, areaY, areaW, grpH, TRUE);
    MoveWindow(hGroupBios, areaX, areaY+grpH+margin, areaW, grpH, TRUE);

    int padX=12, padY=22, rowH=24, lblW=130, boxW=320, boxH=20;
    int leftX = areaX + padX;
    int baseY = areaY + padY;

    // Motherboard (3 linhas)
    MoveWindow(hLblManu,  leftX, baseY+0*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxManu,  leftX+lblW+6, baseY+0*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblModel, leftX, baseY+1*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxModel, leftX+lblW+6, baseY+1*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblBus,   leftX, baseY+2*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxBus,   leftX+lblW+6, baseY+2*rowH, boxW, boxH, TRUE);

    // Chipset (2 linhas: três caixas por linha - vendor, model, revision)
    int chipBaseY = baseY + 4*rowH; // espaço após as 3 linhas do motherboard + uma linha de separação
    int vendorBoxW = 100;
    int modelBoxW = 140;
    int revBoxW = 70;
    for (int i=0;i<2;i++) {
        MoveWindow(hLblChipset[i],       leftX, chipBaseY + i*rowH, lblW, boxH, TRUE);
        MoveWindow(hBoxChipsetVendor[i], leftX + lblW + 6, chipBaseY + i*rowH, vendorBoxW, boxH, TRUE);
        MoveWindow(hBoxChipsetModel[i],  leftX + lblW + 6 + vendorBoxW + 6, chipBaseY + i*rowH, modelBoxW, boxH, TRUE);
        MoveWindow(hBoxChipsetRev[i],    leftX + lblW + 6 + vendorBoxW + 6 + modelBoxW + 6, chipBaseY + i*rowH, revBoxW, boxH, TRUE);
    }

    // BIOS (3 linhas)
    int biosBaseY = areaY + grpH + margin + padY;
    MoveWindow(hLblBiosBrand, leftX, biosBaseY+0*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxBiosBrand, leftX+lblW+6, biosBaseY+0*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblBiosVer,   leftX, biosBaseY+1*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxBiosVer,   leftX+lblW+6, biosBaseY+1*rowH, boxW, boxH, TRUE);

    MoveWindow(hLblBiosDate,  leftX, biosBaseY+2*rowH, lblW, boxH, TRUE);
    MoveWindow(hBoxBiosDate,  leftX+lblW+6, biosBaseY+2*rowH, boxW, boxH, TRUE);

    // Preencher Motherboard
    char manuA[128]={0};
    if (get_motherboard_manufacturer(manuA, sizeof(manuA))) {
        wchar_t manuW[128];
        mbstowcs(manuW, manuA, 127);
        manuW[127]=L'\0';
        SetWindowTextW(hBoxManu, manuW);
    } else {
        SetWindowTextW(hBoxManu, L"Unknown");
    }

    char modelA[128]={0};
    if (get_motherboard_model(modelA, sizeof(modelA))) {
        wchar_t modelW[128];
        mbstowcs(modelW, modelA, 127);
        modelW[127]=L'\0';
        SetWindowTextW(hBoxModel, modelW);
    } else {
        SetWindowTextW(hBoxModel, L"Unknown");
    }

    char busA[128]={0};
    if (get_motherboard_bus_specs(busA, sizeof(busA))) {
        wchar_t busW[128];
        mbstowcs(busW, busA, 127);
        busW[127]=L'\0';
        SetWindowTextW(hBoxBus, busW);
    } else {
        SetWindowTextW(hBoxBus, L"Unknown");
    }

    // Preencher Chipset
    wchar_t labels[2][32], vendors[2][64], models[2][64], revisions[2][16];
    size_t n = build_chipset_rows(labels, vendors, models, revisions, 2);
    for (size_t i=0;i<n && i<2;i++) {
        SetWindowTextW(hLblChipset[i],       labels[i]);
        SetWindowTextW(hBoxChipsetVendor[i], vendors[i]);
        SetWindowTextW(hBoxChipsetModel[i],  models[i]);
        SetWindowTextW(hBoxChipsetRev[i],    revisions[i]);
        ShowWindow(hLblChipset[i], SW_SHOW);
        ShowWindow(hBoxChipsetVendor[i], SW_SHOW);
        ShowWindow(hBoxChipsetModel[i], SW_SHOW);
        ShowWindow(hBoxChipsetRev[i], SW_SHOW);
    }
    for (size_t i=n;i<2;i++) {
        ShowWindow(hLblChipset[i], SW_HIDE);
        ShowWindow(hBoxChipsetVendor[i], SW_HIDE);
        ShowWindow(hBoxChipsetModel[i], SW_HIDE);
        ShowWindow(hBoxChipsetRev[i], SW_HIDE);
    }

    // Preencher BIOS
    char biosBrandA[128]={0};
    if (get_bios_brand(biosBrandA, sizeof(biosBrandA))) {
        wchar_t biosBrandW[128];
        mbstowcs(biosBrandW, biosBrandA, 127);
        biosBrandW[127]=L'\0';
        SetWindowTextW(hBoxBiosBrand, biosBrandW);
    } else {
        SetWindowTextW(hBoxBiosBrand, L"Unknown");
    }

    char biosVerA[128]={0};
    if (get_bios_version(biosVerA, sizeof(biosVerA))) {
        wchar_t biosVerW[128];
        mbstowcs(biosVerW, biosVerA, 127);
        biosVerW[127]=L'\0';
        SetWindowTextW(hBoxBiosVer, biosVerW);
    } else {
        SetWindowTextW(hBoxBiosVer, L"Unknown");
    }

    char biosDateA[64]={0};
    if (get_bios_date(biosDateA, sizeof(biosDateA))) {
        wchar_t biosDateW[64];
        mbstowcs(biosDateW, biosDateA, 63);
        biosDateW[63]=L'\0';
        SetWindowTextW(hBoxBiosDate, biosDateW);
    } else {
        SetWindowTextW(hBoxBiosDate, L"Unknown");
    }
}

static void SwitchTab(HWND hwnd, int sel) {
    // Sempre destrói controles existentes antes de alternar de aba
    DestroyCpuControls();
    DestroyMainboardControls();
    DestroyMemoryControls();
    DestroyGraphicsControls();

    if (sel == 0) {
        ShowCpuTab(hwnd);
    } else if (sel == 1) {
        ShowMainboardTab(hwnd);
    } else if (sel == 2) {
        ShowMemoryTab(hwnd);
    } else if (sel == 3) {
        ShowGraphicsTab(hwnd);
    } else {
        ShowBlankTab(hwnd);
    }
    Layout(hwnd);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateTabs(hwnd);
        ShowCpuTab(hwnd);
        return 0;
    case WM_SIZE:
        Layout(hwnd);
        return 0;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->idFrom == IDC_TAB && ((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
            SwitchTab(hwnd, TabCtrl_GetCurSel(hTab));
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    INITCOMMONCONTROLSEX icc={sizeof(icc), ICC_TAB_CLASSES}; InitCommonControlsEx(&icc);
    WNDCLASSW wc={0}; wc.hInstance=hInst; wc.lpszClassName=WC_MAIN;
    wc.lpfnWndProc=WndProc; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)) return 0;

    HWND hwnd=CreateWindowW(WC_MAIN, APP_TITLE, WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 860, 640, NULL, NULL, hInst, NULL);
    if(!hwnd) return 0;
    ShowWindow(hwnd,nShow); UpdateWindow(hwnd);

    MSG msg; while(GetMessage(&msg,NULL,0,0)){ TranslateMessage(&msg); DispatchMessage(&msg); }
    return (int)msg.wParam;
}
