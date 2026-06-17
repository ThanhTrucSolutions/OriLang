// JNI bridge: embeds the C VM (core/orivm.c) into an Android .so.
#define ORI_AS_LIB
#include "../../core/orivm.c"
#include <jni.h>

static char* capbuf = NULL; static size_t caplen = 0, capcap = 0;
static void cap_hook(const char* line){
    size_t l = strlen(line);
    if(caplen + l + 2 > capcap){ capcap = (capcap ? capcap*2 : 2048) + l + 2; capbuf = realloc(capbuf, capcap); }
    memcpy(capbuf + caplen, line, l); caplen += l;
    capbuf[caplen++] = '\n'; capbuf[caplen] = 0;
}

// Run an in-memory .orb/.orx and return everything it printed via say().
JNIEXPORT jstring JNICALL
Java_ori_app_OriBridge_runImage(JNIEnv* env, jclass cls, jbyteArray orb){
    jsize n = (*env)->GetArrayLength(env, orb);
    jbyte* data = (*env)->GetByteArrayElements(env, orb, NULL);
    caplen = 0; if(capbuf) capbuf[0] = 0;
    ori_say_hook = cap_hook;
    ori_boot_mem((const uint8_t*)data, (size_t)n);
    (*env)->ReleaseByteArrayElements(env, orb, data, JNI_ABORT);
    return (*env)->NewStringUTF(env, capbuf ? capbuf : "");
}

// Call an Ori model function (for interactive apps).
JNIEXPORT jstring JNICALL
Java_ori_app_OriBridge_call(JNIEnv* env, jclass cls, jstring fn, jstring arg){
    const char* f = (*env)->GetStringUTFChars(env, fn, NULL);
    const char* a = (*env)->GetStringUTFChars(env, arg, NULL);
    char* r = ori_call_str(f, a);
    (*env)->ReleaseStringUTFChars(env, fn, f);
    (*env)->ReleaseStringUTFChars(env, arg, a);
    return (*env)->NewStringUTF(env, r ? r : "");
}
