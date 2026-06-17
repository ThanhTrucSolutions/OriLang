// ============================================================================
//  ori.c — thin C bootstrap for the Ori toolchain CLI.
//
//  This file does ONE thing: find the toolchain root and hand off to the
//  Ori-written CLI (tools/ori.orb) running on the C VM.
//
//    Build:  cl /O2 tools\ori.c /Fe:ori.exe   (Windows MSVC)
//            cc  -O2 tools/ori.c  -o ori       (Linux / macOS)
//
//  At runtime:
//    1.  Finds its own directory  → HOME
//    2.  Sets ORI_HOME=HOME, ORI_WIN=1 (Windows only)
//    3.  Runs:  HOME/core/orivm(.exe)  HOME/tools/ori.orb  [original argv[1..]]
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define EXE_EXT ".exe"
#  define SEP     "\\"
#  define SEPC    '\\'
#else
#  include <unistd.h>
#  include <errno.h>
#  ifdef __APPLE__
#    include <mach-o/dyld.h>
#  endif
#  ifndef PATH_MAX
#    define PATH_MAX 4096
#  endif
#  define MAX_PATH PATH_MAX
#  define EXE_EXT ""
#  define SEP     "/"
#  define SEPC    '/'
#endif

static int path_exists(const char* p){ struct stat st; return stat(p,&st)==0; }

static void find_home(const char* argv0, char* home){
#ifdef _WIN32
    GetModuleFileNameA(NULL, home, MAX_PATH);
#else
    home[0]=0;
#  ifdef __APPLE__
    uint32_t sz=(uint32_t)MAX_PATH;
    if(_NSGetExecutablePath(home,&sz)) home[0]=0;
#  else
    ssize_t n=readlink("/proc/self/exe",home,MAX_PATH-1);
    if(n>0) home[n]=0;
#  endif
    if(!home[0] && argv0){
        if(strchr(argv0,'/')){
            char* rp=realpath(argv0,home);
            (void)rp;
        }
        if(!home[0]) strncpy(home,argv0,MAX_PATH-1);
    }
#endif
    /* strip filename, keep directory */
    char* s1=strrchr(home,'/'); char* s2=strrchr(home,'\\');
    char* sl=s1>s2?s1:s2;
    if(sl) *sl=0; else strcpy(home,".");
}

int main(int argc, char** argv){
    char home[4096]="", vm[4096]="", orb[4096]="";
    find_home(argc>0?argv[0]:NULL, home);

    /* vm = HOME/core/orivm(.exe) */
    snprintf(vm, sizeof vm, "%s%score%sorivm%s", home, SEP, SEP, EXE_EXT);
    /* orb = HOME/tools/ori.orb */
    snprintf(orb, sizeof orb, "%s%stools%sori.orb", home, SEP, SEP);

    if(!path_exists(vm)){
        fprintf(stderr,
            "ori: VM not found at %s\n"
            "     Run build.cmd (Windows) or sh build.sh (Linux/macOS) first.\n", vm);
        return 1;
    }
    if(!path_exists(orb)){
        fprintf(stderr,
            "ori: CLI image not found at %s\n"
            "     Run build.cmd (Windows) or sh build.sh to bootstrap.\n", orb);
        return 1;
    }

#ifdef _WIN32
    /* Set ORI_HOME and ORI_WIN for the Ori CLI to read via env() */
    SetEnvironmentVariableA("ORI_HOME", home);
    SetEnvironmentVariableA("ORI_WIN",  "1");
#else
    setenv("ORI_HOME", home, 1);
#endif

    /* Build new argv: [vm, orb, original argv[1]...] */
    int newc = argc + 1;  /* vm, orb, + original args[1..] */
    char** newv = (char**)calloc((size_t)(newc + 2), sizeof(char*));
    if(!newv){ fputs("ori: out of memory\n", stderr); return 1; }
    newv[0] = vm;
    newv[1] = orb;
    for(int i = 1; i < argc; i++) newv[i + 1] = argv[i];
    newv[newc] = NULL;

#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, vm, (const char**)newv);
    free(newv);
    return (int)rc;
#else
    execvp(vm, newv);
    perror(vm);
    free(newv);
    return 127;
#endif
}
