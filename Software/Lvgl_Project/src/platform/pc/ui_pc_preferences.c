#include "ui_pc_preferences.h"
#include "services/ui_preferences.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
static const char *s_path="runtime/preferences.dat";
static bool read_file(const char *path, uint8_t *blob)
{
    FILE *file=fopen(path,"rb"); if(!file) return false;
    bool ok=fread(blob,1,UI_PREFERENCES_BLOB_SIZE,file)==UI_PREFERENCES_BLOB_SIZE && fgetc(file)==EOF && !ferror(file);
    fclose(file); ui_preferences_t decoded;
    return ok && ui_preferences_decode(blob,&decoded);
}
static bool load(uint8_t *blob, void *context)
{
    (void)context;
    return read_file(s_path,blob) || read_file("runtime/preferences.dat.bak",blob);
}
static bool store(const uint8_t *blob, void *context)
{
    (void)context;
#ifdef _WIN32
    _mkdir("runtime");
#else
    mkdir("runtime",0777);
#endif
    FILE *file=fopen("runtime/preferences.dat.tmp","wb"); if(!file) return false;
    bool ok=fwrite(blob,1,UI_PREFERENCES_BLOB_SIZE,file)==UI_PREFERENCES_BLOB_SIZE && fflush(file)==0;
#ifdef _WIN32
    if(ok) ok=_commit(_fileno(file))==0;
#else
    if(ok) ok=fsync(fileno(file))==0;
#endif
    if(fclose(file)!=0) ok=false;
    if(!ok) { remove("runtime/preferences.dat.tmp"); return false; }
    FILE *existing=fopen(s_path,"rb");
    if(existing) { fclose(existing); remove("runtime/preferences.dat.bak");
        if(rename(s_path,"runtime/preferences.dat.bak")!=0) return false; }
    if(rename("runtime/preferences.dat.tmp",s_path)==0) return true;
    (void)rename("runtime/preferences.dat.bak",s_path); return false;
}
#ifdef _WIN32
static DWORD WINAPI worker(void *context) { (void)context; ui_preferences_worker(); return 0; }
#else
static void *worker(void *context) { (void)context; ui_preferences_worker(); return NULL; }
#endif
static bool request(void *context)
{
    (void)context;
#ifdef _WIN32
    HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL); if(!thread) return false; CloseHandle(thread);
#else
    pthread_t thread; if(pthread_create(&thread,NULL,worker,NULL)!=0) return false; pthread_detach(thread);
#endif
    return true;
}
void ui_pc_preferences_register(void)
{
    ui_preferences_port_t port={.load=load,.store=store,.request_worker=request};
    (void)ui_preferences_register(&port);
}
