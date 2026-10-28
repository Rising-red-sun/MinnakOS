#include "string.h"

size_t strlen(const char *s){ size_t n=0; while(s[n]) n++; return n; }
int strcmp(const char *a,const char *b){ while(*a&&*a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
int strncmp(const char *a,const char *b,size_t n){ for(size_t i=0;i<n;i++){ if(a[i]!=b[i]) return (unsigned char)a[i]-(unsigned char)b[i]; if(!a[i]) return 0;} return 0; }
char *strcpy(char *dst,const char *src){ char *r=dst; while((*dst++=*src++)); return r; }
char *strncpy(char *dst,const char *src,size_t n){ size_t i; for(i=0;i<n&&src[i];i++) dst[i]=src[i]; for(;i<n;i++) dst[i]=0; return dst; }
char *strcat(char *dst,const char *src){ char *r=dst; while(*dst) dst++; while((*dst++=*src++)); return r; }
char *strncat(char *dst,const char *src,size_t n){ char *r=dst; while(*dst) dst++; while(n-- && *src) *dst++=*src++; *dst=0; return r; }
char *strchr(const char *s,int c){ while(*s){ if(*s==(char)c) return (char*)s; s++; } if(c=='\0') return (char*)s; return 0; }
void *memset(void *dst,int v,size_t n){ unsigned char *p=dst; while(n--) *p++=(unsigned char)v; return dst; }
void *memcpy(void *dst,const void *src,size_t n){ unsigned char *d=dst; const unsigned char *s=src; while(n--) *d++=*s++; return dst; }
int memcmp(const void *a,const void *b,size_t n){ const unsigned char *pa=a,*pb=b; while(n--){ if(*pa!=*pb) return *pa-*pb; pa++; pb++; } return 0; }
char *strtok(char *str,const char *delim){
    static char *save=0;
    if(str) save=str;
    if(!save) return 0;
    while(*save){ int d=0; for(const char *q=delim;*q;q++) if(*save==*q){d=1;break;} if(!d) break; save++; }
    if(!*save){ save=0; return 0; }
    char *tok=save;
    while(*save){ int d=0; for(const char *q=delim;*q;q++) if(*save==*q){d=1;break;} if(d) break; save++; }
    if(*save){ *save=0; save++; } else save=0;
    return tok;
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)last;
}
