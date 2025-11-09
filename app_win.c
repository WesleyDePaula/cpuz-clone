// app_win.c  — GUI com abas (CPU/Mainboard/Memory/Graphics)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>

#include "cpu/cpu_basic.h"
#include "cpu/cpu_cores.h"
#include "cpu/cpu_cache.h"
#include "cpu/cpu_clock.h"

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

};

static const wchar_t* APP_TITLE = L"Mini CPU-Z (demo)";
static const wchar_t* WC_MAIN   = L"CPUZ_DEMO_CLASS";

// Controles globais
static HWND hTab;
static HWND hGroupProc, hGroupClock, hGroupCache;
// Processor
static HWND hLblVendor, hBoxVendor, hLblName, hBoxName, hLblPhys, hBoxPhys, hLblLogi, hBoxLogi;
// Clocks
static HWND hLblClkCur, hBoxClkCur, hLblClkMax, hBoxClkMax, hLblClkLim, hBoxClkLim;
// Cache
static HWND hLblCache[4], hBoxCacheSize[4], hBoxCacheAssoc[4];

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
}

static void ShowBlankTab(HWND hwnd) {
    DestroyCpuControls(); // tela vazia por enquanto
}

static void ShowCpuTab(HWND hwnd) {
    DestroyCpuControls();

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

static void SwitchTab(HWND hwnd, int sel) {
    if (sel == 0) ShowCpuTab(hwnd);
    else          ShowBlankTab(hwnd); // Mainboard/Memory/Graphics por enquanto vazias
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
