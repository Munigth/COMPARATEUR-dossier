#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shell32.lib")

#define CAP 32768
#define IA 101
#define IB 102
#define BA 103
#define BB 104
#define CMP 105
#define LV 106
#define COPY 107
#define DEL 108
#define STAT 109
#define FILTER 110

typedef struct {
    wchar_t *rel, *full;
    ULONGLONG size;
    FILETIME time;
    BOOL dir;
    int state; /* 0 identical, 1 different, 2 only A, 3 only B, 4 type differs */
    BOOL fromA;
} Item;
typedef struct { Item *v; size_t n, cap; } Items;

static HINSTANCE inst;
static HWND mainWnd, editA, editB, listWnd, statusWnd, filterWnd;
static Items results;

static wchar_t *wdup(const wchar_t *s) {
    size_t n = wcslen(s) + 1;
    wchar_t *p = (wchar_t *)malloc(n * sizeof(wchar_t));
    if (p) wcscpy(p, s);
    return p;
}
static void items_free(Items *a) {
    for (size_t i=0;i<a->n;i++) { free(a->v[i].rel); free(a->v[i].full); }
    free(a->v); ZeroMemory(a,sizeof(*a));
}
static BOOL add(Items *a, const Item *x) {
    if (a->n >= 50000) return FALSE;
    if (a->n == a->cap) {
        size_t c = a->cap ? a->cap*2 : 128;
        if (c > 50000) c=50000;
        Item *p=(Item *)realloc(a->v,c*sizeof(*p));
        if (!p) return FALSE;
        a->v=p; a->cap=c;
    }
    a->v[a->n++]=*x; return TRUE;
}
static BOOL join(wchar_t *o,size_t cap,const wchar_t *a,const wchar_t *b) {
    int n;
    size_t z=wcslen(a);
    n=(z && (a[z-1]==L'\\'||a[z-1]==L'/')) ?
        _snwprintf(o,cap,L"%ls%ls",a,b) : _snwprintf(o,cap,L"%ls\\%ls",a,b);
    return n>=0 && (size_t)n<cap;
}
static int findrel(const Items *a,const wchar_t *r) {
    for(size_t i=0;i<a->n;i++) if(_wcsicmp(a->v[i].rel,r)==0) return (int)i;
    return -1;
}
static BOOL scan(const wchar_t *root,const wchar_t *rel,Items *out) {
    wchar_t folder[CAP], pattern[CAP];
    if (*rel) { if(!join(folder,CAP,root,rel)) return FALSE; }
    else { if(wcslen(root)>=CAP) return FALSE; wcscpy(folder,root); }
    if(_snwprintf(pattern,CAP,L"%ls\\*",folder)<0) return FALSE;
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW(pattern,&fd);
    if(h==INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok=TRUE;
    do {
        if(!wcscmp(fd.cFileName,L".")||!wcscmp(fd.cFileName,L"..")) continue;
        /* Ignore les liens symboliques et points de jonction pour éviter les boucles. */
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        wchar_t r[CAP],f[CAP]; int n;
        n=*rel ? _snwprintf(r,CAP,L"%ls\\%ls",rel,fd.cFileName) :
                 _snwprintf(r,CAP,L"%ls",fd.cFileName);
        if(n<0 || (size_t)n>=CAP || !join(f,CAP,root,r)) { ok=FALSE; break; }
        Item x={0}; x.rel=wdup(r); x.full=wdup(f);
        x.dir=(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        x.size=((ULONGLONG)fd.nFileSizeHigh<<32)|fd.nFileSizeLow;
        x.time=fd.ftLastWriteTime;
        if(!x.rel||!x.full||!add(out,&x)) { free(x.rel);free(x.full);ok=FALSE;break; }
        if(x.dir && !scan(root,r,out)) { ok=FALSE;break; }
    } while(FindNextFileW(h,&fd));
    DWORD err=GetLastError(); FindClose(h);
    return ok && (err==ERROR_NO_MORE_FILES);
}
static BOOL samebytes(const wchar_t *a,const wchar_t *b) {
    HANDLE x=CreateFileW(a,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(x==INVALID_HANDLE_VALUE)return FALSE;
    HANDLE y=CreateFileW(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(y==INVALID_HANDLE_VALUE){CloseHandle(x);return FALSE;}
    BYTE bx[32768],by[32768]; BOOL same=TRUE;
    for(;;){DWORD nx=0,ny=0;
        if(!ReadFile(x,bx,sizeof bx,&nx,NULL)||!ReadFile(y,by,sizeof by,&ny,NULL)||
           nx!=ny||memcmp(bx,by,nx)){same=FALSE;break;}
        if(!nx)break;
    }
    CloseHandle(x);CloseHandle(y);return same;
}
static void result_add(const Item *x,int state,BOOL fromA) {
    Item y=*x; y.rel=wdup(x->rel);y.full=wdup(x->full);y.state=state;y.fromA=fromA;
    if(!y.rel||!y.full||!add(&results,&y)){free(y.rel);free(y.full);}
}
static const wchar_t *state_name(int s) {
    static const wchar_t *names[]={L"IDENTIQUE",L"DIFFÉRENT",L"UNIQUEMENT A",L"UNIQUEMENT B",L"TYPE DIFFÉRENT"};
    return names[s];
}
static void date_text(FILETIME ft,wchar_t *out) {
    FILETIME local; SYSTEMTIME st;
    if(FileTimeToLocalFileTime(&ft,&local)&&FileTimeToSystemTime(&local,&st))
        _snwprintf(out,64,L"%02u/%02u/%04u %02u:%02u",st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute);
    else wcscpy(out,L"-");
}
static void add_row(const Item *x,size_t index) {
    wchar_t size[64],date[64],origin[8];
    _snwprintf(size,64,L"%llu",(unsigned long long)x->size);
    date_text(x->time,date); wcscpy(origin,x->fromA?L"A":L"B");
    LVITEMW it={0};it.mask=LVIF_TEXT|LVIF_PARAM;it.iItem=ListView_GetItemCount(listWnd);
    it.pszText=x->rel;it.lParam=(LPARAM)index;
    int row=ListView_InsertItem(listWnd,&it);
    ListView_SetItemText(listWnd,row,1,(LPWSTR)state_name(x->state));
    ListView_SetItemText(listWnd,row,2,size);
    ListView_SetItemText(listWnd,row,3,date);
    ListView_SetItemText(listWnd,row,4,origin);
}
static void show_results(void) {
    ListView_DeleteAllItems(listWnd);
    int f=(int)SendMessageW(filterWnd,CB_GETCURSEL,0,0);
    for(size_t i=0;i<results.n;i++){
        int s=results.v[i].state;
        BOOL show=f==0||(f==1&&s==0)||(f==2&&(s==1||s==4))||(f==3&&s==2)||(f==4&&s==3);
        if(show)add_row(&results.v[i],i);
    }
}
static void compare(void) {
    wchar_t a[CAP],b[CAP],msg[160];
    GetWindowTextW(editA,a,CAP);GetWindowTextW(editB,b,CAP);
    if(!*a||!*b){MessageBoxW(mainWnd,L"Choisis les deux dossiers.",L"Information",MB_ICONINFORMATION);return;}
    items_free(&results);Items aa={0},bb={0};
    SetWindowTextW(statusWnd,L"Analyse en cours...");
    EnableWindow(mainWnd,FALSE);
    BOOL oka=scan(a,L"",&aa),okb=scan(b,L"",&bb);
    EnableWindow(mainWnd,TRUE);SetForegroundWindow(mainWnd);
    if(!oka||!okb){MessageBoxW(mainWnd,L"Lecture impossible. Vérifie les chemins et les autorisations.",L"Erreur",MB_ICONERROR);items_free(&aa);items_free(&bb);SetWindowTextW(statusWnd,L"Analyse interrompue.");return;}
    for(size_t i=0;i<aa.n;i++){
        Item *x=&aa.v[i];int j=findrel(&bb,x->rel);
        if(j<0)result_add(x,2,TRUE);
        else {Item *y=&bb.v[j];
            if(x->dir!=y->dir)result_add(x,4,TRUE);
            else if(x->dir)result_add(x,0,TRUE);
            else result_add(x,(x->size==y->size&&samebytes(x->full,y->full))?0:1,TRUE);
        }
    }
    for(size_t i=0;i<bb.n;i++)if(findrel(&aa,bb.v[i].rel)<0)result_add(&bb.v[i],3,FALSE);
    show_results();
    _snwprintf(msg,160,L"Comparaison terminée : %zu éléments.",results.n);
    SetWindowTextW(statusWnd,msg);items_free(&aa);items_free(&bb);
}
static BOOL browse(HWND owner,wchar_t *path) {
    BROWSEINFOW bi={0};bi.hwndOwner=owner;bi.lpszTitle=L"Sélectionner un dossier";
    bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&bi);if(!id)return FALSE;
    BOOL ok=SHGetPathFromIDListW(id,path);CoTaskMemFree(id);return ok;
}
static BOOL mkdirs(const wchar_t *path) {
    wchar_t t[CAP];if(wcslen(path)>=CAP)return FALSE;wcscpy(t,path);
    for(wchar_t *p=t+3;*p;p++)if(*p==L'\\'||*p==L'/'){
        wchar_t c=*p;*p=0;
        if(!CreateDirectoryW(t,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){*p=c;return FALSE;}
        *p=c;
    }
    if(!CreateDirectoryW(t,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;
    return TRUE;
}
static void copy_selected(void) {
    int row=ListView_GetNextItem(listWnd,-1,LVNI_SELECTED);
    if(row<0){MessageBoxW(mainWnd,L"Sélectionne un fichier.",L"Copie",MB_ICONINFORMATION);return;}
    LVITEMW it={0};it.mask=LVIF_PARAM;it.iItem=row;ListView_GetItem(listWnd,&it);
    Item *x=&results.v[(size_t)it.lParam];
    if(x->dir){MessageBoxW(mainWnd,L"La copie des dossiers entiers n'est pas activée. Choisis un fichier.",L"Copie",MB_ICONINFORMATION);return;}
    wchar_t dest[CAP],target[CAP],parent[CAP];
    if(!browse(mainWnd,dest))return;
    if(!join(target,CAP,dest,x->rel)){MessageBoxW(mainWnd,L"Chemin trop long.",L"Erreur",MB_ICONERROR);return;}
    if(GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES){MessageBoxW(mainWnd,L"Le fichier existe déjà. Aucun écrasement effectué.",L"Copie",MB_ICONWARNING);return;}
    wcscpy(parent,target);wchar_t *p=wcsrchr(parent,L'\\');if(p)*p=0;
    if(!mkdirs(parent)){MessageBoxW(mainWnd,L"Création des sous-dossiers impossible.",L"Erreur",MB_ICONERROR);return;}
    if(!CopyFileW(x->full,target,TRUE)){MessageBoxW(mainWnd,L"Échec de copie.",L"Erreur",MB_ICONERROR);return;}
    MessageBoxW(mainWnd,L"Copie terminée. L'original est conservé.",L"Succès",MB_ICONINFORMATION);
}

/* Boîte de confirmation native : saisir OUI exactement. */
static BOOL ask_oui(void) {
    wchar_t answer[32]={0};
    /* InputBox native simple : fenêtre temporaire avec champ éditable. */
    HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME,L"STATIC",L"Confirmation de suppression",
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE, CW_USEDEFAULT,CW_USEDEFAULT,370,150,
        mainWnd,NULL,GetModuleHandleW(NULL),NULL);
    if(!dlg)return FALSE;
    HWND label=CreateWindowW(L"STATIC",L"Tape OUI pour confirmer la suppression :",WS_CHILD|WS_VISIBLE,
        15,15,330,24,dlg,NULL,inst,NULL);
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
        15,43,325,25,dlg,(HMENU)1,inst,NULL);
    HWND yes=CreateWindowW(L"BUTTON",L"Valider",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
        170,78,80,27,dlg,(HMENU)2,inst,NULL);
    HWND no=CreateWindowW(L"BUTTON",L"Annuler",WS_CHILD|WS_VISIBLE,
        260,78,80,27,dlg,(HMENU)3,inst,NULL);
    (void)label;(void)yes;(void)no;SetFocus(edit);
    EnableWindow(mainWnd,FALSE);
    BOOL done=FALSE,ok=FALSE;MSG m;
    while(!done&&GetMessageW(&m,NULL,0,0)>0){
        if(m.message==WM_COMMAND&&m.hwnd==dlg){
            if(LOWORD(m.wParam)==2){GetWindowTextW(edit,answer,32);ok=wcscmp(answer,L"OUI")==0;done=TRUE;}
            if(LOWORD(m.wParam)==3)done=TRUE;
        }
        if(m.message==WM_CLOSE&&m.hwnd==dlg)done=TRUE;
        if(!IsDialogMessageW(dlg,&m)){TranslateMessage(&m);DispatchMessageW(&m);}
        if(done)DestroyWindow(dlg);
    }
    EnableWindow(mainWnd,TRUE);SetForegroundWindow(mainWnd);
    return ok;
}
static void delete_selected(void) {
    int row=ListView_GetNextItem(listWnd,-1,LVNI_SELECTED);
    if(row<0){MessageBoxW(mainWnd,L"Sélectionne un fichier.",L"Suppression",MB_ICONINFORMATION);return;}
    LVITEMW it={0};it.mask=LVIF_PARAM;it.iItem=row;ListView_GetItem(listWnd,&it);
    Item *x=&results.v[(size_t)it.lParam];
    if(x->dir){MessageBoxW(mainWnd,L"La suppression des dossiers est désactivée dans ce prototype.",L"Suppression",MB_ICONWARNING);return;}
    if(!ask_oui()){MessageBoxW(mainWnd,L"Suppression annulée : il fallait saisir OUI.",L"Suppression",MB_ICONINFORMATION);return;}
    if(DeleteFileW(x->full)){MessageBoxW(mainWnd,L"Fichier supprimé.",L"Suppression",MB_ICONINFORMATION);compare();}
    else MessageBoxW(mainWnd,L"Suppression impossible. Vérifie les droits et si le fichier est ouvert.",L"Erreur",MB_ICONERROR);
}
static HFONT font;
static HWND button(HWND p,const wchar_t *s,int id,int x,int y,int w,int h){
    HWND q=CreateWindowW(L"BUTTON",s,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,y,w,h,p,(HMENU)(INT_PTR)id,inst,NULL);
    SendMessageW(q,WM_SETFONT,(WPARAM)font,TRUE);return q;
}
static void controls(HWND w) {
    font=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HWND t=CreateWindowW(L"STATIC",L"Dossier A :",WS_CHILD|WS_VISIBLE,18,18,85,25,w,NULL,inst,NULL);
    editA=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,105,15,610,28,w,(HMENU)IA,inst,NULL);
    button(w,L"Parcourir...",BA,725,15,120,28);
    HWND u=CreateWindowW(L"STATIC",L"Dossier B :",WS_CHILD|WS_VISIBLE,18,57,85,25,w,NULL,inst,NULL);
    editB=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,105,54,610,28,w,(HMENU)IB,inst,NULL);
    button(w,L"Parcourir...",BB,725,54,120,28);
    button(w,L"COMPARER",CMP,18,96,130,34);
    button(w,L"Copier le fichier sélectionné",COPY,160,96,230,34);
    button(w,L"Supprimer (OUI)",DEL,402,96,170,34);
    button(w,L"Actualiser",111,584,96,100,34);
    filterWnd=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,700,99,145,150,w,(HMENU)FILTER,inst,NULL);
    const wchar_t *opts[]={L"Tous",L"Identiques",L"Différents",L"Uniquement A",L"Uniquement B"};
    for(int i=0;i<5;i++)SendMessageW(filterWnd,CB_ADDSTRING,0,(LPARAM)opts[i]);
    SendMessageW(filterWnd,CB_SETCURSEL,0,0);
    listWnd=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,
        18,145,852,430,w,(HMENU)LV,inst,NULL);
    ListView_SetExtendedListViewStyle(listWnd,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW c={0};c.mask=LVCF_TEXT|LVCF_WIDTH;
    c.pszText=L"Chemin relatif";c.cx=350;ListView_InsertColumn(listWnd,0,&c);
    c.pszText=L"État";c.cx=130;ListView_InsertColumn(listWnd,1,&c);
    c.pszText=L"Taille (octets)";c.cx=120;ListView_InsertColumn(listWnd,2,&c);
    c.pszText=L"Dernière modification";c.cx=165;ListView_InsertColumn(listWnd,3,&c);
    c.pszText=L"Source";c.cx=65;ListView_InsertColumn(listWnd,4,&c);
    statusWnd=CreateWindowW(L"STATIC",L"Prêt. Choisis deux dossiers puis clique sur COMPARER.",
        WS_CHILD|WS_VISIBLE,18,585,850,25,w,(HMENU)STAT,inst,NULL);
    HWND all[]={t,editA,u,editB,filterWnd,listWnd,statusWnd};
    for(size_t i=0;i<sizeof(all)/sizeof(all[0]);i++)SendMessageW(all[i],WM_SETFONT,(WPARAM)font,TRUE);
}
static void browse_edit(HWND e){wchar_t p[CAP];if(browse(mainWnd,p))SetWindowTextW(e,p);}
static LRESULT CALLBACK wndproc(HWND w,UINT m,WPARAM a,LPARAM b) {
    switch(m){
    case WM_CREATE: mainWnd=w;controls(w);return 0;
    case WM_COMMAND:
        switch(LOWORD(a)){
        case BA:browse_edit(editA);return 0;
        case BB:browse_edit(editB);return 0;
        case CMP:case 111:compare();return 0;
        case COPY:copy_selected();return 0;
        case DEL:delete_selected();return 0;
        case FILTER:if(HIWORD(a)==CBN_SELCHANGE)show_results();return 0;
        }break;
    case WM_SIZE:
        if(listWnd)MoveWindow(listWnd,18,145,LOWORD(b)-36,HIWORD(b)-180,TRUE);
        if(statusWnd)MoveWindow(statusWnd,18,HIWORD(b)-28,LOWORD(b)-36,22,TRUE);
        return 0;
    case WM_DESTROY:items_free(&results);if(font)DeleteObject(font);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE prev,PWSTR cmd,int show) {
    (void)prev;(void)cmd;inst=h;
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASSW wc={0};wc.lpfnWndProc=wndproc;wc.hInstance=h;wc.lpszClassName=L"FolderCompareWin32";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);if(!RegisterClassW(&wc))return 1;
    HWND w=CreateWindowW(wc.lpszClassName,L"Comparateur de dossiers - Interface graphique C",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,920,690,NULL,NULL,h,NULL);
    if(!w)return 1;ShowWindow(w,show);UpdateWindow(w);
    MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return (int)msg.wParam;
}
