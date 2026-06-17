// ============================================================================
//  oriwin.c — a GENERIC native Win32 host for Ori apps.
//
//  It embeds the C VM (core/orivm.c) and renders whatever the Ori program
//  describes via render(), forwarding events to dispatch(event, arg). It has
//  NO app-specific logic. Widget protocol (one per line):
//    text|CONTENT
//    edit|PLACEHOLDER
//    btn|EVENT|ARG|CAPTION            (ARG "@edit" = current text-field contents)
//    item|TAP_EVENT|DEL_EVENT|ARG|CAPTION
//
//  Build:  cl /O2 oriwin.c /link user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
//  Run:    oriwin.exe <app.orb>
// ============================================================================
#define ORI_AS_LIB
#include "../../core/orivm.c"
#include <windows.h>

#define MAXACT 256
#define ID_BASE 1000
#define ID_EDIT 900

static HWND g_hwnd, g_edit, g_font;
static char g_ev[MAXACT][64];
static char g_arg[MAXACT][512];
static int  g_argIsEdit[MAXACT];
static int  g_nact;

static void clear_children(HWND hwnd){
    HWND c = GetWindow(hwnd, GW_CHILD);
    while(c){ HWND next = GetWindow(c, GW_HWNDNEXT); DestroyWindow(c); c = next; }
    g_edit = NULL; g_nact = 0;
}

static void mkfont(HWND h){ SendMessageA(h, WM_SETFONT, (WPARAM)g_font, TRUE); }

// next "|" field into out; returns pointer past the separator (or to end).
static const char* field(const char* p, char* out, int cap){
    int i=0; while(*p && *p!='|'){ if(i<cap-1) out[i++]=*p; p++; } out[i]=0;
    if(*p=='|') p++;
    return p;
}

static void build(const char* spec){
    clear_children(g_hwnd);
    int y = 12;
    const char* p = spec;
    while(*p){
        const char* nl = strchr(p, '\n');
        int len = nl ? (int)(nl-p) : (int)strlen(p);
        if(len > 0){
            char line[1024]; if(len>1000) len=1000; memcpy(line,p,len); line[len]=0;
            char type[16]; const char* q = field(line, type, sizeof type);
            if(strcmp(type,"text")==0){
                HWND h=CreateWindowA("STATIC", q, WS_CHILD|WS_VISIBLE, 16,y,440,22, g_hwnd,0,0,0);
                mkfont(h); y+=28;
            } else if(strcmp(type,"edit")==0){
                char ph[256]; field(q, ph, sizeof ph);
                g_edit=CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 16,y,440,28, g_hwnd,(HMENU)(INT_PTR)ID_EDIT,0,0);
                mkfont(g_edit); SetWindowTextA(g_edit, ph); y+=36;
            } else if(strcmp(type,"btn")==0){
                char ev[64], arg[512], cap[512];
                q=field(q,ev,sizeof ev); q=field(q,arg,sizeof arg); field(q,cap,sizeof cap);
                int id=ID_BASE+g_nact; strcpy(g_ev[g_nact],ev); strcpy(g_arg[g_nact],arg); g_argIsEdit[g_nact]=(strcmp(arg,"@edit")==0); g_nact++;
                HWND h=CreateWindowA("BUTTON", cap, WS_CHILD|WS_VISIBLE, 16,y,440,30, g_hwnd,(HMENU)(INT_PTR)id,0,0);
                mkfont(h); y+=38;
            } else if(strcmp(type,"item")==0){
                char tap[64], del[64], arg[512], cap[512];
                q=field(q,tap,sizeof tap); q=field(q,del,sizeof del); q=field(q,arg,sizeof arg); field(q,cap,sizeof cap);
                int idT=ID_BASE+g_nact; strcpy(g_ev[g_nact],tap); strcpy(g_arg[g_nact],arg); g_argIsEdit[g_nact]=0; g_nact++;
                int idD=ID_BASE+g_nact; strcpy(g_ev[g_nact],del); strcpy(g_arg[g_nact],arg); g_argIsEdit[g_nact]=0; g_nact++;
                HWND h1=CreateWindowA("BUTTON", cap, WS_CHILD|WS_VISIBLE|BS_LEFT, 16,y,360,32, g_hwnd,(HMENU)(INT_PTR)idT,0,0);
                HWND h2=CreateWindowA("BUTTON", "X", WS_CHILD|WS_VISIBLE, 384,y,72,32, g_hwnd,(HMENU)(INT_PTR)idD,0,0);
                mkfont(h1); mkfont(h2); y+=40;
            }
        }
        if(!nl) break;
        p = nl+1;
    }
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static void fire(int actIndex){
    char arg[512];
    if(g_argIsEdit[actIndex] && g_edit){ GetWindowTextA(g_edit, arg, sizeof arg); }
    else { strncpy(arg, g_arg[actIndex], sizeof arg-1); arg[sizeof arg-1]=0; }
    char* ui = ori_call2("dispatch", g_ev[actIndex], arg);
    build(ui);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if(id>=ID_BASE && id-ID_BASE < g_nact) fire(id-ID_BASE);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow){
    if(__argc < 2){ MessageBoxA(0,"usage: oriwin.exe <app.orb>","Ori",MB_OK); return 1; }
    if(ori_boot(__argv[1], 0, NULL) != 0){ MessageBoxA(0,"failed to load Ori image","Ori",MB_OK); return 1; }

    g_font = (HWND)CreateFontA(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = "OriWinClass";
    wc.hCursor = LoadCursor(0, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassA(&wc);
    g_hwnd = CreateWindowA("OriWinClass","Ori app",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,492,560,0,0,hInst,0);

    build(ori_call_str("render", ""));   // draw the Ori-described UI
    ShowWindow(g_hwnd, nShow); UpdateWindow(g_hwnd);

    MSG m;
    while(GetMessage(&m,0,0,0)){ TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}
