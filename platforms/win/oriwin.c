// ============================================================================
//  oriwin.c — a native Win32 GUI host for Ori.
//
//  Embeds the C VM (core/orivm.c) and runs an Ori "model" (.orb defining
//  add/toggle/remove/view) as a real Windows window: a text box, an Add button,
//  a list of tasks, and Toggle/Delete buttons. The list logic lives in Ori;
//  this file is just the native view, driving it via ori_call_str.
//
//  Build:  cl /O2 oriwin.c /link user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
//  Run:    oriwin.exe <app.orb>
// ============================================================================
#define ORI_AS_LIB
#include "../../core/orivm.c"

#include <windows.h>

#define IDC_EDIT   101
#define IDC_ADD    102
#define IDC_LIST   103
#define IDC_TOGGLE 104
#define IDC_DEL    105

static HWND hEdit, hList, hAdd, hTog, hDel;
static HFONT hFont;

static void set_font(HWND h){ SendMessageA(h, WM_SETFONT, (WPARAM)hFont, TRUE); }

static void refresh(){
    char* v = ori_call_str("view", "");
    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    // v is lines "idx|done|text"
    char* p = v;
    while(*p){
        char* nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if(len > 0){
            char line[1024]; if(len > 1000) len = 1000;
            memcpy(line, p, len); line[len] = 0;
            // parse idx|done|text
            char* b1 = strchr(line, '|');
            char* b2 = b1 ? strchr(b1+1, '|') : NULL;
            char disp[1100];
            if(b2){
                int done = (*(b1+1) == '1');
                snprintf(disp, sizeof disp, "%s  %s", done ? "[x]" : "[ ]", b2+1);
            } else {
                snprintf(disp, sizeof disp, "%s", line);
            }
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)disp);
        }
        if(!nl) break;
        p = nl + 1;
    }
}

static void do_add(){
    char buf[1024];
    GetWindowTextA(hEdit, buf, sizeof buf);
    if(buf[0]){
        ori_call_str("add", buf);
        SetWindowTextA(hEdit, "");
        refresh();
    }
    SetFocus(hEdit);
}
static void do_on_selected(const char* fn){
    int sel = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
    if(sel < 0) return;
    char idx[16]; snprintf(idx, sizeof idx, "%d", sel);
    ori_call_str(fn, idx);
    refresh();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
        case WM_CREATE: {
            hFont = CreateFontA(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
            CreateWindowA("STATIC","Ori Todo - logic runs on the Ori VM (native C)",WS_CHILD|WS_VISIBLE,
                          14,10,420,22,hwnd,0,0,0);
            hEdit = CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
                          14,38,300,28,hwnd,(HMENU)IDC_EDIT,0,0);
            hAdd  = CreateWindowA("BUTTON","Add",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
                          322,38,112,28,hwnd,(HMENU)IDC_ADD,0,0);
            hList = CreateWindowA("LISTBOX","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,
                          14,76,420,360,hwnd,(HMENU)IDC_LIST,0,0);
            hTog  = CreateWindowA("BUTTON","Toggle done",WS_CHILD|WS_VISIBLE,
                          14,444,205,32,hwnd,(HMENU)IDC_TOGGLE,0,0);
            hDel  = CreateWindowA("BUTTON","Delete",WS_CHILD|WS_VISIBLE,
                          229,444,205,32,hwnd,(HMENU)IDC_DEL,0,0);
            set_font(hEdit); set_font(hAdd); set_font(hList); set_font(hTog); set_font(hDel);
            refresh();
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if(id == IDC_ADD) do_add();
            else if(id == IDC_TOGGLE) do_on_selected("toggle");
            else if(id == IDC_DEL) do_on_selected("remove");
            else if(id == IDC_LIST && HIWORD(wp) == LBN_DBLCLK) do_on_selected("toggle");
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow){
    if(__argc < 2){ MessageBoxA(0,"usage: oriwin.exe <app.orb>","Ori",MB_OK); return 1; }
    if(ori_boot(__argv[1], 0, NULL) != 0){ MessageBoxA(0,"failed to load Ori image","Ori",MB_OK); return 1; }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "OriWinClass";
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("OriWinClass","Ori Todo",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,468,536,0,0,hInst,0);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG m;
    while(GetMessage(&m,0,0,0)){
        if(m.message==WM_KEYDOWN && m.wParam==VK_RETURN && GetFocus()==hEdit){ do_add(); continue; }
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
