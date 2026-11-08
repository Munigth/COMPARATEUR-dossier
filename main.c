#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define SEP '\\'
#define MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define SEP '/'
#define MKDIR(p) mkdir(p,0755)
#endif
#define MP 4096
#define MF 20000
typedef struct {char rel[MP],full[MP]; long long size; time_t mt; int dir;} Entry;
typedef struct {Entry *v;size_t n,cap;} List;
static int sep(char c){return c=='/'||c=='\\';}
static void join(char*out,size_t z,const char*a,const char*b){size_t n=strlen(a);if(n&&sep(a[n-1]))snprintf(out,z,"%s%s",a,b);else snprintf(out,z,"%s%c%s",a,SEP,b);}
static void line(const char*p,char*out,size_t n){printf("%s",p);if(!fgets(out,(int)n,stdin)){*out=0;return;}out[strcspn(out,"\r\n")]=0;}
static int add(List*l,Entry*e){if(l->n>=MF)return 0;if(l->n==l->cap){size_t c=l->cap?l->cap*2:128;Entry*p=realloc(l->v,c*sizeof* p);if(!p)return 0;l->v=p;l->cap=c;}l->v[l->n++]=*e;return 1;}
static void fmt(time_t t,char*out,size_t n){struct tm x;
#ifdef _WIN32
localtime_s(&x,&t);
#else
localtime_r(&t,&x);
#endif
strftime(out,n,"%Y-%m-%d %H:%M:%S",&x);}
#ifdef _WIN32
static int scan(const char*root,const char*rel,List*l){char pat[MP],r[MP],f[MP];WIN32_FIND_DATAA d;HANDLE h;
if(*rel)snprintf(pat,sizeof pat,"%s\\%s\\*",root,rel);else snprintf(pat,sizeof pat,"%s\\*",root);
h=FindFirstFileA(pat,&d);if(h==INVALID_HANDLE_VALUE){fprintf(stderr,"Lecture impossible: %s (erreur %lu)\n",pat,GetLastError());return 0;}
do{if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;if(*rel)snprintf(r,sizeof r,"%s\\%s",rel,d.cFileName);else snprintf(r,sizeof r,"%s",d.cFileName);join(f,sizeof f,root,r);Entry e={0};snprintf(e.rel,sizeof e.rel,"%s",r);snprintf(e.full,sizeof e.full,"%s",f);e.dir=(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;ULARGE_INTEGER s;s.HighPart=d.nFileSizeHigh;s.LowPart=d.nFileSizeLow;e.size=(long long)s.QuadPart;ULARGE_INTEGER t;t.HighPart=d.ftLastWriteTime.dwHighDateTime;t.LowPart=d.ftLastWriteTime.dwLowDateTime;e.mt=(time_t)((t.QuadPart-116444736000000000ULL)/10000000ULL);if(!add(l,&e)){FindClose(h);return 0;}if(e.dir&&!scan(root,r,l)){FindClose(h);return 0;}}while(FindNextFileA(h,&d));FindClose(h);return 1;}
#else
static int scan(const char*root,const char*rel,List*l){char f[MP],r[MP];join(f,sizeof f,root,rel);DIR*d=opendir(f);if(!d){perror(f);return 0;}struct dirent*x;while((x=readdir(d))){if(!strcmp(x->d_name,".")||!strcmp(x->d_name,".."))continue;if(*rel)snprintf(r,sizeof r,"%s/%s",rel,x->d_name);else snprintf(r,sizeof r,"%s",x->d_name);join(f,sizeof f,root,r);struct stat s;if(stat(f,&s))continue;Entry e={0};snprintf(e.rel,sizeof e.rel,"%s",r);snprintf(e.full,sizeof e.full,"%s",f);e.dir=S_ISDIR(s.st_mode);e.size=s.st_size;e.mt=s.st_mtime;if(!add(l,&e)){closedir(d);return 0;}if(e.dir&&!scan(root,r,l)){closedir(d);return 0;}}closedir(d);return 1;}
#endif
static int eqname(const char*a,const char*b){
#ifdef _WIN32
return _stricmp(a,b)==0;
#else
return strcmp(a,b)==0;
#endif
}
static Entry*find(List*l,const char*r){for(size_t i=0;i<l->n;i++)if(eqname(l->v[i].rel,r))return &l->v[i];return NULL;}
static int samebytes(const char*a,const char*b){FILE*x=fopen(a,"rb"),*y=fopen(b,"rb");if(!x||!y){if(x)fclose(x);if(y)fclose(y);return 0;}unsigned char bx[16384],by[16384];size_t nx,ny;int ok=1;do{nx=fread(bx,1,sizeof bx,x);ny=fread(by,1,sizeof by,y);if(nx!=ny||memcmp(bx,by,nx)){ok=0;break;}}while(nx&&ny);fclose(x);fclose(y);return ok;}
static int mkpath(const char*p){char t[MP];snprintf(t,sizeof t,"%s",p);size_t n=strlen(t);for(size_t i=1;i<n;i++)if(sep(t[i])){char c=t[i];t[i]=0;if(*t&&MKDIR(t)!=0&&errno!=EEXIST){
#ifdef _WIN32
if(GetFileAttributesA(t)==INVALID_FILE_ATTRIBUTES){t[i]=c;return 0;}
#else
struct stat s;if(stat(t,&s)||!S_ISDIR(s.st_mode)){t[i]=c;return 0;}
#endif
}t[i]=c;}if(MKDIR(t)!=0&&errno!=EEXIST){
#ifdef _WIN32
if(GetFileAttributesA(t)==INVALID_FILE_ATTRIBUTES)return 0;
#else
struct stat s;if(stat(t,&s)||!S_ISDIR(s.st_mode))return 0;
#endif
}return 1;}
static int copyfile(const char*a,const char*b){FILE*x=fopen(a,"rb");if(!x)return 0;FILE*y=fopen(b,"wb");if(!y){fclose(x);return 0;}char buf[32768];size_t n;int ok=1;while((n=fread(buf,1,sizeof buf,x)))if(fwrite(buf,1,n,y)!=n){ok=0;break;}if(ferror(x))ok=0;fclose(x);if(fclose(y))ok=0;return ok;}
static void show(Entry*e){char t[64];fmt(e->mt,t,sizeof t);printf(" %-55s %10lld octets  %s%s\n",e->rel,e->size,t,e->dir?" [DOSSIER]":"");}
static void freeL(List*l){free(l->v);memset(l,0,sizeof*l);}
int main(void){char aroot[MP],broot[MP],dest[MP],c[64];List a={0},b={0};puts("=== COMPARATEUR DE DOSSIERS ===");line("Chemin du dossier A : ",aroot,sizeof aroot);line("Chemin du dossier B : ",broot,sizeof broot);if(!*aroot||!*broot||eqname(aroot,broot)){puts("Chemins invalides ou identiques.");return 1;}puts("Analyse de A...");if(!scan(aroot,"",&a)){freeL(&a);return 1;}puts("Analyse de B...");if(!scan(broot,"",&b)){freeL(&a);freeL(&b);return 1;}printf("\nA : %zu éléments | B : %zu éléments\n",a.n,b.n);puts("\n--- COMPARAISON PAR CHEMIN RELATIF ---");
for(size_t i=0;i<a.n;i++){Entry*x=&a.v[i],*y=find(&b,x->rel);if(x->dir)continue;if(!y){printf("[UNIQUEMENT A]");show(x);}else if(y->dir){printf("[TYPE DIFFÉRENT]");show(x);}else{int same=x->size==y->size&&samebytes(x->full,y->full);printf("%s",same?"[IDENTIQUE]":"[DIFFÉRENT]");show(x);if(x->size!=y->size)printf("  Taille B : %lld octets\n",y->size);if(x->mt!=y->mt){char t[64];fmt(y->mt,t,sizeof t);printf("  Date B : %s\n",t);}}}
for(size_t i=0;i<b.n;i++)if(!b.v[i].dir&&!find(&a,b.v[i].rel)){printf("[UNIQUEMENT B]");show(&b.v[i]);}
puts("\nActions : 1 copier des fichiers dans un dossier commun (sources conservées)");puts("         2 supprimer un fichier précis");puts("         3 quitter sans modifier");line("Choix : ",c,sizeof c);
if(c[0]=='1'){line("Dossier de rassemblement (choisir un 3e dossier) : ",dest,sizeof dest);if(!*dest||eqname(dest,aroot)||eqname(dest,broot)){puts("Destination invalide.");}else if(!mkpath(dest)){puts("Création de destination impossible.");}else for(;;){char side[16],rel[MP],out[MP],parent[MP];line("Source A/B (vide pour terminer) : ",side,sizeof side);if(!*side)break;line("Chemin relatif exact : ",rel,sizeof rel);List*l=(side[0]=='A'||side[0]=='a')?&a:(side[0]=='B'||side[0]=='b')?&b:NULL;Entry*e=l?find(l,rel):NULL;if(!e||e->dir){puts("Fichier non trouvé.");continue;}join(out,sizeof out,dest,e->rel);FILE*p=fopen(out,"rb");if(p){fclose(p);puts("Destination déjà existante : aucun écrasement.");continue;}snprintf(parent,sizeof parent,"%s",out);for(size_t k=strlen(parent);k>0;k--)if(sep(parent[k-1])){parent[k-1]=0;break;}if(!mkpath(parent)){puts("Impossible de créer les sous-dossiers.");continue;}if(copyfile(e->full,out))printf("Copié vers %s\n",out);else puts("Échec de copie.");}}
else if(c[0]=='2'){char side[16],rel[MP],confirm[32];line("Supprimer depuis A ou B ? ",side,sizeof side);line("Chemin relatif exact du fichier : ",rel,sizeof rel);List*l=(side[0]=='A'||side[0]=='a')?&a:(side[0]=='B'||side[0]=='b')?&b:NULL;Entry*e=l?find(l,rel):NULL;if(!e||e->dir)puts("Fichier non trouvé.");else{printf("Sélection : %s (%lld octets)\n",e->full,e->size);line("Suppression définitive ? taper OUI pour confirmer : ",confirm,sizeof confirm);if(!strcmp(confirm,"OUI")){
#ifdef _WIN32
if(DeleteFileA(e->full))puts("Supprimé.");else puts("Échec de suppression.");
#else
if(remove(e->full)==0)puts("Supprimé.");else perror("Suppression");
#endif
}else puts("Suppression annulée.");}}else puts("Aucune modification.");freeL(&a);freeL(&b);puts("Terminé.");return 0;}
