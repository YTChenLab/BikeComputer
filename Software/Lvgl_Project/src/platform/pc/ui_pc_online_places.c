#include "ui_pc_online_places.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

static char *trim(char *text)
{while(*text&&isspace((unsigned char)*text))text++;size_t n=strlen(text);while(n&&isspace((unsigned char)text[n-1]))text[--n]=0;return text;}
void ui_pc_online_places_load(ui_pc_online_places_config_t *config,const char *path)
{
 if(!config)return;*config=(ui_pc_online_places_config_t){0};if(!path)return;FILE *file=fopen(path,"rb");if(!file)return;
 char line[256];unsigned bytes=0,keys=0,enables=0;bool bad=false,first=true;
 while(fgets(line,sizeof(line),file)){
  bytes+=(unsigned)strlen(line);if(bytes>2048||(!strchr(line,'\n')&&!feof(file))){bad=true;break;}
  char *entry=line;if(first&&strlen(line)>=3&&(unsigned char)line[0]==0xef&&(unsigned char)line[1]==0xbb&&(unsigned char)line[2]==0xbf)entry+=3;
  first=false;entry=trim(entry);if(!*entry||*entry=='#'||*entry==';')continue;char *equal=strchr(entry,'=');
  if(!equal){bad=true;break;}*equal++=0;entry=trim(entry);equal=trim(equal);
  if(!strcmp(entry,"enabled")){if(enables++||(strcmp(equal,"0")&&strcmp(equal,"1"))){bad=true;break;}config->enabled=!strcmp(equal,"1");}
  else if(!strcmp(entry,"amap_key")){if(keys++||strlen(equal)>32){bad=true;break;}memcpy(config->key,equal,strlen(equal)+1);}
  else {bad=true;break;}
 }
 if(ferror(file))bad=true;fclose(file);config->valid=!bad&&strlen(config->key)==32;
 for(unsigned i=0;config->valid&&i<32;i++)if(!isxdigit((unsigned char)config->key[i]))config->valid=false;
 if(bad){config->enabled=true;config->valid=false;} /* Surface invalid config without a request. */
}
static bool stopped(bool (*cancelled)(void *),void *context)
{return cancelled&&cancelled(context);}
#ifdef _WIN32
static bool encode(const char *text,char *out,size_t cap)
{
 const char *hex="0123456789ABCDEF";size_t n=0;
 for(const unsigned char *p=(const unsigned char *)text;*p;p++){
  if(isalnum(*p)&&*p<128){if(n+1>=cap)return false;out[n++]=(char)*p;}
  else{if(n+3>=cap)return false;out[n++]='%';out[n++]=hex[*p>>4];out[n++]=hex[*p&15];}
 }out[n]=0;return true;
}
static bool deadline(ULONGLONG begin,HINTERNET session,bool (*cancelled)(void *),void *context)
{
 ULONGLONG elapsed=GetTickCount64()-begin;if(elapsed>=2200||stopped(cancelled,context))return false;
 int remaining=(int)(2200-elapsed);if(remaining>600)remaining=600;
 return WinHttpSetTimeouts(session,remaining,remaining,remaining,remaining)!=FALSE;
}
static ui_place_online_status_t fetch(const ui_pc_online_places_config_t *config,const ui_nav_search_request_t *request,
 ui_place_results_t *out,bool (*cancelled)(void *),void *context)
{
 ui_place_online_status_t status=UI_PLACE_ONLINE_FAILED;char query[UI_PLACE_QUERY_MAX*3],path[512];WCHAR wide[512];
 if(!encode(request->query,query,sizeof(query)))return status;
 snprintf(path,sizeof(path),"/v3/assistant/inputtips?key=%s&keywords=%s&city=610100&citylimit=true&datatype=poi&output=JSON",config->key,query);
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,512))return status;
 HINTERNET session=WinHttpOpen(L"BikeComputerNavigation/1.5",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
 HINTERNET connection=NULL,command=NULL;char *body=NULL;ULONGLONG begin=GetTickCount64();
 DWORD error=ERROR_SUCCESS;
 if(!session){error=GetLastError();goto done;}
 connection=WinHttpConnect(session,L"restapi.amap.com",INTERNET_DEFAULT_HTTPS_PORT,0);if(!connection){error=GetLastError();goto done;}
 command=WinHttpOpenRequest(connection,L"GET",wide,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!command){error=GetLastError();goto done;}
 DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
 if(!WinHttpSetOption(command,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect))){error=GetLastError();goto done;}
 if(!deadline(begin,command,cancelled,context))goto done;
 if(!WinHttpSendRequest(command,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)){error=GetLastError();goto done;}
 if(!deadline(begin,command,cancelled,context))goto done;
 if(!WinHttpReceiveResponse(command,NULL)){error=GetLastError();goto done;}
 DWORD code=0,code_size=sizeof(code);
 if(!WinHttpQueryHeaders(command,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&code_size,WINHTTP_NO_HEADER_INDEX)){error=GetLastError();goto done;}
 if(code!=200){if(code==401||code==403)status=UI_PLACE_ONLINE_KEY_ERROR;else if(code==408||code==504)status=UI_PLACE_ONLINE_TIMEOUT;goto done;}
 body=malloc(UI_ONLINE_RESPONSE_MAX+1);if(!body)goto done;DWORD length=0;
 for(;;){DWORD available=0,read=0;if(!deadline(begin,command,cancelled,context))goto done;
  if(!WinHttpQueryDataAvailable(command,&available)){error=GetLastError();goto done;}
  if(!available)break;if(available>UI_ONLINE_RESPONSE_MAX-length)goto done;
  if(!WinHttpReadData(command,body+length,available,&read)){error=GetLastError();goto done;}if(!read)goto done;length+=read;
 }
 if(!stopped(cancelled,context))status=ui_online_places_amap_parse(body,length,out);
done:
 if(status==UI_PLACE_ONLINE_FAILED&&(error==ERROR_WINHTTP_TIMEOUT||GetTickCount64()-begin>=2200))status=UI_PLACE_ONLINE_TIMEOUT;
 if(stopped(cancelled,context)){*out=(ui_place_results_t){0};status=UI_PLACE_ONLINE_FAILED;}
 free(body);if(command)WinHttpCloseHandle(command);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);
 memset(path,0,sizeof(path));memset(wide,0,sizeof(wide));return status;
}
#endif
ui_place_online_status_t ui_pc_online_places_query(const ui_pc_online_places_config_t *config,
 const ui_nav_search_request_t *request,ui_place_results_t *out,bool (*cancelled)(void *),void *context)
{
 if(!out)return UI_PLACE_ONLINE_FAILED;*out=(ui_place_results_t){0};if(!config||!config->enabled)return UI_PLACE_ONLINE_OFF;
 if(!config->valid)return UI_PLACE_ONLINE_KEY_ERROR;
 if(!request||!memchr(request->query,0,sizeof(request->query))||!request->query[0]||stopped(cancelled,context))return UI_PLACE_ONLINE_FAILED;
#ifdef _WIN32
 return fetch(config,request,out,cancelled,context);
#else
 return UI_PLACE_ONLINE_FAILED;
#endif
}
