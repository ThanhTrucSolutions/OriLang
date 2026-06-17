// ============================================================================
//  ori.c — the Ori toolchain CLI, native (no .NET / PowerShell).
//
//    ori create <name> [web|android|windows]   scaffold (ori/ + <name>.meta)
//    ori run    [path]                          compile + run
//    ori dev    [path]                          hot reload (watch + recompile + rerun)
//    ori build  [path] [release]                build/app.orb  (release: app.orx)
//    ori doctor                                 build the native VM if missing
//    ori version
//
//  Build:  cl /O2 tooling\ori.c  (or any C compiler). Talks to core\orivm.exe
//  and tooling\oric.orb; serves web via node.
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <sys/stat.h>
#include <direct.h>
#include <windows.h>

static char HOME[MAX_PATH];   // toolchain root (parent of this exe's dir)
static char VM[MAX_PATH];     // core\orivm.exe
static char COMPILER[MAX_PATH]; // tooling\oric.orb

static void join(char* out, const char* a, const char* b){ snprintf(out, MAX_PATH, "%s\\%s", a, b); }
static int exists(const char* p){ return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

static void find_home(){
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    // exe is <HOME>\ori.exe (CLI lives at repo root). HOME = dir of exe.
    char* slash = strrchr(exe, '\\'); if(slash) *slash = 0;
    strcpy(HOME, exe);
    join(VM, HOME, "core\\orivm.exe");
    join(COMPILER, HOME, "tooling\\oric.orb");
}

// run a program, wait, return exit code. argv NULL-terminated.
static int run_wait(const char* exe, const char** argv){
    intptr_t r = _spawnvp(_P_WAIT, exe, argv);  // _spawnvp searches PATH (for node/cmd)
    return (int)r;
}

static int build_vm(){
    printf("ori: building the native VM (core/orivm.c)...\n");
    char bat[MAX_PATH]; join(bat, HOME, "core\\build.cmd");
    const char* argv[] = { "cmd", "/c", bat, NULL };
    char cmd[MAX_PATH]; GetEnvironmentVariableA("ComSpec", cmd, MAX_PATH);
    int rc = run_wait(cmd[0]?cmd:"cmd.exe", argv);
    if(!exists(VM)){ fprintf(stderr,"ori: VM build failed (need Visual Studio C++ tools)\n"); return 1; }
    printf("ori: built core/orivm.exe\n");
    return rc;
}

static int doctor(){
    printf("ori doctor\n");
    if(exists(VM)) printf("  [ok] native VM      core/orivm.exe\n");
    else if(build_vm()) return 1;
    if(exists(COMPILER)) printf("  [ok] Ori compiler  tooling/oric.orb (self-hosted)\n");
    else { fprintf(stderr,"  missing tooling/oric.orb\n"); return 1; }
    char wasm[MAX_PATH]; join(wasm, HOME, "tooling\\web\\orivm.wasm");
    printf(exists(wasm) ? "  [ok] web runtime    tooling/web/orivm.wasm\n"
                        : "  [..] web runtime    not built (only for web)\n");
    printf("  Toolchain ready.\n");
    return 0;
}

// ---- meta ----
typedef struct { char name[128]; char entry[256]; char platform[32]; char ui[16]; } Meta;

static void find_meta(const char* proj, char* out){
    char pat[MAX_PATH]; snprintf(pat, MAX_PATH, "%s\\*.meta", proj);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if(h==INVALID_HANDLE_VALUE){ out[0]=0; return; }
    join(out, proj, fd.cFileName);
    FindClose(h);
}

static void trim(char* s){
    char* p=s; while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++;
    if(p!=s) memmove(s,p,strlen(p)+1);
    int n=(int)strlen(s); while(n>0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
}

static int read_meta(const char* proj, Meta* m){
    char path[MAX_PATH]; find_meta(proj, path);
    if(!path[0]){ fprintf(stderr,"ori: no *.meta file in %s (not an Ori project?)\n", proj); return 1; }
    strcpy(m->name,"app"); strcpy(m->entry,"ori/main.ori"); strcpy(m->platform,"windows"); strcpy(m->ui,"console");
    FILE* f=fopen(path,"r"); if(!f){ fprintf(stderr,"ori: cannot read %s\n",path); return 1; }
    char line[512];
    while(fgets(line,sizeof line,f)){
        trim(line);
        if(!line[0] || line[0]=='#') continue;
        char* c=strchr(line,':'); if(!c) continue;
        *c=0; char* k=line; char* v=c+1; trim(k); trim(v);
        if(!*v) continue;
        if(!strcmp(k,"name")) strncpy(m->name,v,sizeof m->name-1);
        else if(!strcmp(k,"entry")) strncpy(m->entry,v,sizeof m->entry-1);
        else if(!strcmp(k,"platform")) strncpy(m->platform,v,sizeof m->platform-1);
        else if(!strcmp(k,"ui")) strncpy(m->ui,v,sizeof m->ui-1);
    }
    fclose(f);
    return 0;
}

static void to_backslash(char* s){ for(;*s;s++) if(*s=='/') *s='\\'; }

// compile <proj>/<entry> -> <proj>/build/app.orb  ; returns path in orbOut
static int compile_orb(const char* proj, Meta* m, char* orbOut){
    char entry[MAX_PATH]; snprintf(entry,MAX_PATH,"%s\\%s",proj,m->entry); to_backslash(entry);
    if(!exists(entry)){ fprintf(stderr,"ori: entry not found: %s\n", m->entry); return 1; }
    char build[MAX_PATH]; join(build, proj, "build"); _mkdir(build);
    join(orbOut, build, "app.orb");
    const char* argv[] = { VM, COMPILER, entry, orbOut, NULL };
    return run_wait(VM, argv);
}

static long long mtime_of(const char* path){
    WIN32_FILE_ATTRIBUTE_DATA d;
    if(!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return 0;
    return ((long long)d.ftLastWriteTime.dwHighDateTime<<32)|d.ftLastWriteTime.dwLowDateTime;
}

static int cmd_run(const char* proj, Meta* m, int hot);

static int run_web(const char* proj, Meta* m){
    char wasm[MAX_PATH]; join(wasm, HOME, "tooling\\web\\orivm.wasm");
    if(!exists(wasm)){ fprintf(stderr,"ori: web runtime missing (see docs/TOOLCHAIN.md)\n"); return 1; }
    char webBuild[MAX_PATH], build[MAX_PATH];
    join(build, proj, "build"); _mkdir(build);
    join(webBuild, build, "web"); _mkdir(webBuild);
    char src[MAX_PATH], dst[MAX_PATH];
    join(src, HOME, "tooling\\web\\orivm.js");   join(dst, webBuild, "orivm.js");   CopyFileA(src,dst,FALSE);
    join(src, HOME, "tooling\\web\\orivm.wasm"); join(dst, webBuild, "orivm.wasm"); CopyFileA(src,dst,FALSE);
    // project may bring its own web/index.html (GUI app); else use default
    char projIndex[MAX_PATH]; join(projIndex, proj, "web\\index.html");
    join(dst, webBuild, "index.html");
    if(exists(projIndex)) CopyFileA(projIndex, dst, FALSE);
    else { join(src, HOME, "tooling\\templates\\web\\index.html"); CopyFileA(src,dst,FALSE); }
    char server[MAX_PATH]; join(server, HOME, "tooling\\templates\\web\\server.mjs");
    printf("ori: serving web app (hot reload) at http://127.0.0.1:5151\n");
    const char* argv[] = { "node", server, webBuild, proj, VM, COMPILER, "5151", NULL };
    return run_wait("node", argv);
}

static int build_android(const char* proj, Meta* m){
    char orb[MAX_PATH]; if(compile_orb(proj,m,orb)){ fprintf(stderr,"ori: compile failed\n"); return 1; }
    char apk[MAX_PATH]; snprintf(apk, MAX_PATH, "%s\\build\\%s.apk", proj, m->name);
    char script[MAX_PATH]; join(script, HOME, "platforms\\android\\build-apk.cmd");
    if(!exists(script)){ fprintf(stderr,"ori: android builder not found at %s\n", script); return 1; }
    printf("ori: building Android APK (NDK + aapt2 + apksigner)...\n");
    const char* argv[] = { "cmd", "/c", script, orb, apk, NULL };
    int rc = run_wait("cmd", argv);
    if(rc==0) printf("ori: android APK -> %s\n      install: adb install -r \"%s\"\n", apk, apk);
    return rc;
}

static int run_window(const char* proj, Meta* m){
    char orb[MAX_PATH]; if(compile_orb(proj,m,orb)){ fprintf(stderr,"ori: compile failed\n"); return 1; }
    char gui[MAX_PATH]; join(gui, HOME, "platforms\\win\\oriwin.exe");
    if(!exists(gui)){ fprintf(stderr,"ori: GUI host not built (run build.cmd)\n"); return 1; }
    printf("ori: launching native window app\n");
    const char* argv[] = { gui, orb, NULL };
    return run_wait(gui, argv);
}

static int cmd_run(const char* proj, Meta* m, int hot){
    if(!strcmp(m->platform,"web")) return run_web(proj, m);
    if(!strcmp(m->platform,"windows") && !strcmp(m->ui,"window")) return run_window(proj, m);
    char entry[MAX_PATH]; snprintf(entry,MAX_PATH,"%s\\%s",proj,m->entry); to_backslash(entry);
    if(!hot){
        char orb[MAX_PATH]; if(compile_orb(proj,m,orb)) { fprintf(stderr,"ori: compile failed\n"); return 1; }
        printf("ori: running on the native C VM\n----------------------------------------\n");
        const char* argv[] = { VM, orb, NULL };
        return run_wait(VM, argv);
    }
    printf("ori dev: hot reload watching %s (Ctrl+C to stop)\n", m->entry);
    long long last=0;
    for(;;){
        long long t=mtime_of(entry);
        if(t!=last){
            last=t;
            printf("\n========== ori dev: reload ==========\n");
            char orb[MAX_PATH];
            if(!compile_orb(proj,m,orb)){
                printf("----------------------------------------\n");
                const char* argv[] = { VM, orb, NULL };
                run_wait(VM, argv);
            }
        }
        Sleep(400);
    }
}

static void write_file(const char* path, const char* content){
    FILE* f=fopen(path,"wb"); if(f){ fwrite(content,1,strlen(content),f); fclose(f); }
}

static int cmd_create(const char* name, const char* platform){
    char dir[MAX_PATH]; _mkdir(name);
    snprintf(dir,MAX_PATH,"%s\\ori",name); _mkdir(dir);
    char path[MAX_PATH];
    snprintf(path,MAX_PATH,"%s\\ori\\main.ori",name);
    write_file(path,
        "// main.ori - run with:  ori run    (or  ori dev  for hot reload)\n"
        "fold fib n {\n"
        "    when n < 2 { give n }\n"
        "    give fib (n - 1) + fib (n - 2)\n"
        "}\n\n"
        "hold i = 0\n"
        "loop i < 12 {\n"
        "    say (\"fib(\" + str i + \") = \" + str (fib i))\n"
        "    i = i + 1\n"
        "}\n"
        "say \"Hello from Ori!\"\n");
    char meta[1024];
    snprintf(meta,sizeof meta,
        "name: %s\nversion: 1.0.0\nentry: ori/main.ori\nplatform: %s\n\n# Dependencies are declared here; `ori run` auto-installs the toolchain.\ndependencies:\n",
        name, platform);
    snprintf(path,MAX_PATH,"%s\\%s.meta",name,name);
    write_file(path, meta);
    printf("Created project '%s' (platform: %s) -> ori/ + %s.meta\n", name, platform, name);
    printf("  cd %s & ori run        (or: ori dev)\n", name);
    return 0;
}

static void usage(){
    printf("Ori toolchain (native) - C VM + self-hosted Ori compiler\n"
           "  ori create <name> [windows|web|android]\n"
           "  ori run [path]            compile and run\n"
           "  ori dev [path]            hot reload (web: live dev server)\n"
           "  ori build [path] [release]  -> build/app.orb  (release: encrypted .orx)\n"
           "  ori doctor / ori version\n");
}

int main(int argc, char** argv){
    setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered: keep ordering with child output
    find_home();
    if(argc<2){ usage(); return 0; }
    const char* cmd=argv[1];
    const char* arg=argc>2?argv[2]:".";

    if(!strcmp(cmd,"version")){ printf("Ori toolchain 0.5 (native: C VM + self-hosted Ori compiler)\n"); return 0; }
    if(!strcmp(cmd,"doctor")) return doctor();
    if(!strcmp(cmd,"create")){
        if(argc<3){ fprintf(stderr,"usage: ori create <name> [windows|web|android]\n"); return 1; }
        const char* plat = argc>3?argv[3]:"windows";
        return cmd_create(argv[2], plat);
    }
    if(!strcmp(cmd,"help")||!strcmp(cmd,"-h")||!strcmp(cmd,"--help")){ usage(); return 0; }

    // commands that need the toolchain + a project
    if(doctor()) return 1;
    char proj[MAX_PATH]; _fullpath(proj, arg, MAX_PATH);
    Meta m; if(read_meta(proj,&m)) return 1;

    if(!strcmp(cmd,"run"))   return cmd_run(proj,&m,0);
    if(!strcmp(cmd,"dev"))   return cmd_run(proj,&m,1);
    if(!strcmp(cmd,"build")){
        int release = (argc>3 && !strcmp(argv[3],"release"));
        if(!strcmp(m.platform,"android")) return build_android(proj,&m);
        char orb[MAX_PATH]; if(compile_orb(proj,&m,orb)){ fprintf(stderr,"ori: compile failed\n"); return 1; }
        if(release){
            char orx[MAX_PATH], build[MAX_PATH]; join(build,proj,"build"); join(orx,build,"app.orx");
            const char* a[] = { VM, "pack", orb, orx, NULL };
            run_wait(VM, a);
            printf("ori: release build -> %s (encrypted, per-build opcode map)\n", orx);
        } else printf("ori: built %s\n", orb);
        return 0;
    }
    usage();
    return 1;
}
