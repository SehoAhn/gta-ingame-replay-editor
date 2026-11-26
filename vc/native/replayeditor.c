/*
 * Replay Editor for GTA Vice City v1.0 - native module
 *
 * F6 (in game)  menu: OPEN IN REPLAY EDITOR / PLAY REPLAY, then a list of the
 *               .rep and .prj files in the user folder (where F2 saves).
 *               .rep -> new project, .prj -> continue it (it names its source
 *               .rep). PLAY: .rep = game playback, .prj = source played with
 *               the project's trim + camera (incl. FOV) applied live.
 * F7 (editor)   save <base>.edit.<YYYYMONDD-HHMMSS>.prj next to the source,
 *               overwriting without backup. F6 leaves the editor.
 *
 * Loaded by CLEO\ReplayEditor.cs (0AA2) which calls RE_Init once per script
 * start. Freestanding: no CRT, no import libraries. Win32 functions are
 * resolved through the game's own IAT (GetModuleHandleA / GetProcAddress).
 *
 * Hooks (all are E8 rel32 calls, the previous target is kept and chained):
 *   0x4A45C3  CGame::Process      -> CReplay::Update            (keys, enter/exit, free cam)
 *   0x624F20  CReplay::Update     -> CReplay::PlaybackThisFrame (pause/speed/seek)
 *   0x6231B7  PlaybackThisFrame   -> ProcessLookAroundCam       (editor camera)
 *   0x4A61D6  Render2dStuff       -> CReplay::Display           (editor HUD)
 *   0x4A45FA  CGame::Process      -> ShouldStandardCameraBeProcessed (0x61EF70)
 *             During playback the game runs its live camera (CCamera::Process,
 *             after the replay) whenever the player is in a vehicle and the
 *             look-around cam is idle - it overwrites the camera of the file
 *             and of the editor. Forced off in the editor / viewer and for
 *             playback started from the F6 menu.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

int _fltused = 1;

#pragma function(memset, memcpy)
void *__cdecl memset(void *d, int c, unsigned n) { u8 *p = (u8 *)d; while (n--) *p++ = (u8)c; return d; }
void *__cdecl memcpy(void *d, const void *s, unsigned n) { u8 *p = (u8 *)d; const u8 *q = (const u8 *)s; while (n--) *p++ = *q++; return d; }

/* ------------------------------------------------------------------ game */
#define G8(a)   (*(volatile u8 *)(a))
#define G32(a)  (*(volatile u32 *)(a))
#define GI32(a) (*(volatile int *)(a))
#define GF(a)   (*(volatile float *)(a))
#define GPTR(a) (*(u8 *volatile *)(a))

#define IAT_GetModuleHandleA 0x6F2464
#define IAT_GetProcAddress   0x6F2488
#define IAT_timeGetTime      0x6F2594

#define A_MODE          0xA10AF6   /* CReplay::Mode 0 rec 1 play */
#define A_BUFSTATUS     0x94D4E0   /* u8[8] 0 unused 1 full 2 current */
#define A_BUFFERS       0x869CB8   /* 8 x 100000 */
#define BUFSZ           100000
#define A_PB_OFF        0x9B5F58   /* playback position: offset */
#define A_PB_BASE       0x9B5F5C   /*                    buffer pointer */
#define A_PB_SLOT       0x9B5F60   /*                    slot */
#define A_DIVISOR       0xA0D95C   /* interpolation frames per recorded frame */
#define A_INTERP_CNT    0x7869B4
#define A_SCREEN_W      0x9B48E4
#define A_SCREEN_H      0x9B48E8
#define A_MOUSE_X       0x94D790   /* float, this frame */
#define A_MOUSE_Y       0x94D794
#define A_CAM_MATRIX    0x7E4688   /* TheCamera matrix: right, fwd, up, pos (16 B rows) */
#define A_SCENE_CAMERA  0x7E4E90   /* RwCamera *Scene.camera */
#define A_FOV           0x696658   /* CDraw::ms_fFOV */
#define REPLAY_FOV      70.0f      /* TriggerPlayback: CDraw::SetFOV(70) */

#define F_UPDATE        0x624EC0
#define F_PLAYBACK      0x6230A0
#define F_LOOKAROUND    0x61E970
#define F_DISPLAY       0x620BE0
#define F_PLAYFROMHD    0x61F0E0
#define F_FINISH        0x6210E0
#define F_GETPAD        0x4AB060   /* CPad *GetPad(int) */
#define F_CUTSCENE      0x54E100   /* bool, blocks replay keys */
#define F_MAT_UPDATERW  0x46C3B6   /* thiscall on TheCamera matrix */
#define F_RWMATRIXUPD   0x6437B0
#define F_RWFRAMEUPD    0x644D00
#define F_RWFRAMEUPD2   0x645320
#define F_SETDIR_DOCS   0x48E020   /* CFileMgr::SetDirMyDocuments (replay.rep lives there) */
#define F_SETDIR        0x48E030   /* CFileMgr::SetDir(const char *) */
#define A_EMPTY_STR     0x6DB638   /* "" (SaveReplayToHD restores the dir with it) */
#define F_OPENWRITE     0x48DF70   /* int OpenFileForWriting(name), 0 = failed */
#define F_FILEWRITE     0x48DF30   /* Write(fd, buf, len) */
#define F_FILECLOSE     0x48DEA0   /* CloseFile(fd) */
#define A_LOAD_NAME_OPND 0x61F0EE  /* PlayReplayFromHD: push <file name> */
#define A_REP_HEADER    0x6DB610   /* "gtaVC7f" 8 bytes */
#define A_ONMISSION_OFS 0x978748   /* CTheScripts::OnAMissionFlag (offset into ScriptSpace; IsPlayerOnAMission 0x44FE30) */
#define A_SCRIPTSPACE   0x821280   /* CTheScripts::ScriptSpace */
#define A_REPLAY_REP    0x6DB604   /* "replay.rep" (original load operand) */

#define SITE_UPDATE     0x4A45C3
#define SITE_PLAYBACK   0x624F20
#define SITE_LOOKAROUND 0x6231B7
#define SITE_DISPLAY    0x4A61D6
#define SITE_STDCAM     0x4A45FA   /* CGame::Process: if (ShouldStandardCameraBeProcessed()) TheCamera.Process() */

typedef void (__cdecl *vfn)(void);
typedef void (__cdecl *P1Fn)(void *);
typedef void (__fastcall *ThisFn)(void *, void *);
typedef void *(__cdecl *GetPadFn)(int);
typedef char (__cdecl *BoolFn)(void);
typedef void (__cdecl *F2Fn)(float, float);
typedef void (__cdecl *F1Fn)(float);
typedef void (__cdecl *I1Fn)(int);
typedef void (__cdecl *PFn)(const void *);
typedef void (__cdecl *PrintFn)(float, float, const u16 *);
typedef void (__cdecl *RectFn)(const float *, const u8 *);
typedef int (__cdecl *OpenWFn)(const char *);
typedef int (__cdecl *FWriteFn)(int, const void *, int);
typedef void (__cdecl *SFn)(const char *);

#define Font_SetScale      ((F2Fn)0x550230)
#define Font_ResetA        ((vfn)0x550140)   /* clears 0x97F838/0x97F83A */
#define Font_ResetB        ((vfn)0x5500D0)   /* clears 0x97F83B */
#define Font_SetWrapx      ((F1Fn)0x5500F0)
#define Font_ResetC        ((vfn)0x550110)   /* clears 0x97F839 */
#define Font_SetPropOn     ((vfn)0x550020)
#define Font_SetColor      ((PFn)0x550170)
#define Font_SetFontStyle  ((I1Fn)0x54FFE0)
#define Font_PrintString   ((PrintFn)0x551040)
#define Sprite2d_DrawRect  ((RectFn)0x577B00)

/* ----------------------------------------------------------------- win32 */
typedef void *(__stdcall *GetModuleHandleAFn)(const char *);
typedef void *(__stdcall *GetProcAddressFn)(void *, const char *);
typedef int (__stdcall *VirtualProtectFn)(void *, u32, u32, u32 *);
typedef short (__stdcall *GetAsyncKeyStateFn)(int);
typedef void *(__stdcall *CreateFileAFn)(const char *, u32, u32, void *, u32, u32, void *);
typedef int (__stdcall *WriteFileFn)(void *, const void *, u32, u32 *, void *);
typedef int (__stdcall *FlushFileBuffersFn)(void *);
typedef u32 (__stdcall *TimeFn)(void);
typedef int (__stdcall *ReadFileFn)(void *, void *, u32, u32 *, void *);
typedef int (__stdcall *CloseHandleFn)(void *);
typedef int (__stdcall *CopyFileAFn)(const char *, const char *, int);
typedef void *(__stdcall *VirtualAllocFn)(void *, u32, u32, u32);
typedef void *(__stdcall *FindFirstFn)(const char *, void *);
typedef int (__stdcall *FindNextFn)(void *, void *);
typedef int (__stdcall *FindCloseFn)(void *);
typedef int (__stdcall *FtFn)(const void *, void *);
static ReadFileFn pReadFile;
static CloseHandleFn pCloseHandle;
static VirtualAllocFn pVirtualAlloc;
static FindFirstFn pFindFirstFileA;
static FindNextFn pFindNextFileA;
static FindCloseFn pFindClose;
static FtFn pFileTimeToLocalFileTime, pFileTimeToSystemTime;
typedef u32 (__stdcall *GetFileSizeFn)(void *, u32 *);
static GetFileSizeFn pGetFileSize;
typedef int (__stdcall *DeleteFileAFn)(const char *);
static DeleteFileAFn pDeleteFileA;
typedef void (__stdcall *GetLocalTimeFn)(u16 *);   /* SYSTEMTIME */
static GetLocalTimeFn pGetLocalTime;

static VirtualProtectFn pVirtualProtect;
static GetAsyncKeyStateFn pGetAsyncKeyState;
static CreateFileAFn pCreateFileA;
static WriteFileFn pWriteFile;
static FlushFileBuffersFn pFlushFileBuffers;
static void *g_log = (void *)-1;

/* ------------------------------------------------------------------ math */
#define PI 3.14159265f
static float fsin_(float x) { float r; __asm { fld x } __asm { fsin } __asm { fstp r } return r; }
static float fcos_(float x) { float r; __asm { fld x } __asm { fcos } __asm { fstp r } return r; }
static float fsqrt_(float x) { float r; if (x <= 0) return 0; __asm { fld x } __asm { fsqrt } __asm { fstp r } return r; }
static float fatan2_(float y, float x) { float r; __asm { fld y } __asm { fld x } __asm { fpatan } __asm { fstp r } return r; }
static float fclamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float ffloor_(float x) { int i = (int)x; return (float)(x < (float)i ? i - 1 : i); }
static void vnorm(float *v)
{
    float l = fsqrt_(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}
static void vcross(float *o, const float *a, const float *b)
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}
static float vdot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static float wrap_near(float a, float ref)     /* a + 2*pi*k closest to ref */
{
    while (a - ref > PI) a -= 2 * PI;
    while (a - ref < -PI) a += 2 * PI;
    return a;
}
static float catmull(float p0, float p1, float p2, float p3, float u)
{
    float u2 = u * u, u3 = u2 * u;
    return 0.5f * (2 * p1 + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u2 + (-p0 + 3 * p1 - 3 * p2 + p3) * u3);
}

/* interpolation of a segment (key -> next key), stored in the key's 'cut' byte:
   0 / 1 keep their old meaning (SMOOTH / CUT) so older projects load unchanged */
enum { IP_SMOOTH, IP_CUT, IP_LINEAR, IP_EASEIN, IP_EASEOUT, IP_EASEINOUT, IP_CONST, IP_N };
static const char *IP_NAME[IP_N] = { "SMOOTH", "CUT", "LINEAR", "EASE IN", "EASE OUT", "EASE IN-OUT", "CONSTANT SPEED" };
/* B order: camera SMOOTH > LINEAR > EASE IN > EASE OUT > EASE IN-OUT > CONSTANT SPEED > CUT
            weather / time (value 0 = linear there) LINEAR > EASE IN > EASE OUT > EASE IN-OUT > CUT */
static const u8 IP_CAM_ORDER[] = { IP_SMOOTH, IP_LINEAR, IP_EASEIN, IP_EASEOUT, IP_EASEINOUT, IP_CONST, IP_CUT };
static const u8 IP_ENV_ORDER[] = { IP_SMOOTH, IP_EASEIN, IP_EASEOUT, IP_EASEINOUT, IP_CUT };
static int ip_next(int cur, int env, int dir)
{
    const u8 *o = env ? IP_ENV_ORDER : IP_CAM_ORDER;
    int n = env ? (int)sizeof(IP_ENV_ORDER) : (int)sizeof(IP_CAM_ORDER), i;
    for (i = 0; i < n && o[i] != cur; i++) {}
    if (i == n) i = 0;
    return o[(i + dir + n) % n];
}
static const char *ip_name(int ip, int env) { return env && ip == IP_SMOOTH ? "LINEAR" : IP_NAME[ip < IP_N ? ip : 0]; }
/* progress through the segment for the easing types */
static float ease(int ip, float u)
{
    if (ip == IP_EASEIN) return u * u;
    if (ip == IP_EASEOUT) return 1.0f - (1.0f - u) * (1.0f - u);
    if (ip == IP_EASEINOUT) return u * u * (3.0f - 2.0f * u);
    return u;
}
/* its speed (derivative) for the lane's speed bar */
static float ease_speed(int ip, float u)
{
    if (ip == IP_EASEIN) return 2.0f * u;
    if (ip == IP_EASEOUT) return 2.0f * (1.0f - u);
    if (ip == IP_EASEINOUT) return 6.0f * u * (1.0f - u);
    return 1.0f;
}

/* ---------------------------------------------------------------- string */
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static char *sput(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *iput(char *d, int v, int width)
{
    char tmp[12]; int n = 0; u32 u;
    if (v < 0) { *d++ = '-'; u = (u32)(-v); } else u = (u32)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    while (n < width) tmp[n++] = '0';
    while (n) *d++ = tmp[--n];
    *d = 0;
    return d;
}
static char *hput(char *d, u32 v)
{
    int i;
    *d++ = '0'; *d++ = 'x';
    for (i = 28; i >= 0; i -= 4) *d++ = "0123456789ABCDEF"[(v >> i) & 15];
    *d = 0;
    return d;
}
static int lower_(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int ends_with(const char *s, const char *suf)   /* case-insensitive */
{
    int a = slen(s), b = slen(suf), i;
    if (a < b) return 0;
    for (i = 0; i < b; i++) if (lower_(s[a - b + i]) != lower_(suf[i])) return 0;
    return 1;
}
static void scopy(char *d, const char *s, int cap)
{
    int i;
    for (i = 0; i < cap - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
/* printable copy for the game font (no Korean glyphs): a DBCS character -> '?' */
static char *dput(char *d, const char *s, int maxc)
{
    const u8 *p = (const u8 *)s;
    int n = 0;
    while (*p && n < maxc) {
        if (*p >= 0x80) { *d++ = '?'; if (p[1] >= 0x40) p++; p++; }
        else *d++ = (char)*p++;
        n++;
    }
    if (*p) { *d++ = '.'; *d++ = '.'; }
    *d = 0;
    return d;
}
static char *tput(char *d, u32 ms)   /* mm:ss.cc */
{
    d = iput(d, (int)(ms / 60000), 2); *d++ = ':';
    d = iput(d, (int)(ms / 1000 % 60), 2); *d++ = '.';
    return iput(d, (int)(ms / 10 % 100), 2);
}

static void logs(const char *s)
{
    u32 w;
    if (g_log == (void *)-1 || !pWriteFile) return;
    pWriteFile(g_log, s, slen(s), &w, 0);
    pWriteFile(g_log, "\r\n", 2, &w, 0);
    if (pFlushFileBuffers) pFlushFileBuffers(g_log);
}

/* ----------------------------------------------------------------- hooks */
static u32 orig_update, orig_playback, orig_lookaround, orig_display, orig_stdcam;
static int g_filecam;                  /* F6 PLAY of a .rep: always show the file's camera */

static u32 hook_call(u32 site, void *fn, const char *name)
{
    u8 *p = (u8 *)site;
    u32 old, target;
    char line[96], *e;
    if (p[0] != 0xE8) {
        e = sput(line, "hook FAILED (not a call): "); sput(e, name); logs(line);
        return 0;
    }
    target = site + 5 + *(int *)(p + 1);
    if (target == (u32)fn) return 0;               /* already ours */
    if (!pVirtualProtect(p + 1, 4, 0x40, &old)) { logs("VirtualProtect failed"); return 0; }
    *(int *)(p + 1) = (int)((u32)fn - (site + 5));
    pVirtualProtect(p + 1, 4, old, &old);
    e = sput(line, "hooked "); e = sput(e, name); e = sput(e, " prev target "); hput(e, target);
    logs(line);
    return target;
}

/* ----------------------------------------------------------- frame table
   The frame table always describes the source file kept in memory (g_src):
   chunk c = file bytes 8 + c * 100000. Files with up to 8 chunks are played
   by the game's own loader (chunk c -> slot c, "direct"). Bigger files (e.g.
   saved by the "replay longer" mod with 64 buffers) do not fit the game's 8
   slots: a window of frames is re-encoded into the slots and moved along
   while playing / seeking ("window" mode). */
#define MAXF 32768
#define MAXCH 128                          /* chunks of 100000 bytes in a source file */
#define DIRECT_MAX (8 + 8 * BUFSZ)         /* what the game's loader can take */
static const u8 PKT_SIZE[13] = { 0, 0x34, 0x2C, 0x08, 0x2C, 0x58, 0x04, 0x08, 0x04, 0x08, 0x1C, 0x14, 0x10 };
static u8 fr_slot[MAXF];                   /* chunk of the frame's first packet */
static u32 fr_off[MAXF];
static u32 fr_time[MAXF];
static u8 fr_gslot[MAXF];                  /* GENERAL packet of the frame (camera, player pos) */
static u32 fr_goff[MAXF];
static u8 fr_hh[MAXF], fr_mm[MAXF];        /* recorded clock (CLOCK packet) of the frame */
static int nfr, nch;
static int slot_first[MAXCH];
/* The recorder writes PARTICLE packets between frames and switches buffer
   later, so a frame can begin with particles at the tail of one chunk and
   continue in the next. slot_tail_off[c] = where such a frame starts in c. */
static u32 slot_tail_off[MAXCH];
static int slot_tail_frame[MAXCH];

static u8 *g_src;                          /* the source .rep file (header + chunks) */
static u32 g_srclen;

static u8 *game_slot(int s) { return (u8 *)A_BUFFERS + s * BUFSZ; }
static u8 *chunk_base(int c) { return g_src + 8 + c * BUFSZ; }

/* window mode */
static int g_win, w0, w1;                  /* frames w0 .. w1-1 are in the game's slots */
static u8 lw_slot[MAXF];
static u32 lw_off[MAXF];

/* ----------------------------------------------------- sub replay (SUB 1)
   A second .rep merged into the project. Project frames = main frames plus the
   sub frames before the main start / after the main end. Per project frame:
   pm = main frame (-1 = none), pmc = the same clamped to the first / last main
   frame (camera, clock, weather where there is no main), ps = sub frame (-1 =
   none). With a sub the game always plays through the window (encode_merged).
   PlayBackThisFrameInterpolation 0x621230: vehicles (VEHICLE / BIKE share one
   cursor) and peds (PED_UPDATE) must come in ascending pool index; every index
   skipped and everything after the last one at ENDOFFRAME is deleted - so an
   entity missing from a frame disappears. PED_HEADER creates a ped when its slot
   is empty. Without a sub: pm = pmc = identity, ps = -1. */
#define PED_POOL 140
#define VEH_POOL 110
static int mnf;                            /* frames of the main replay (main frame table) */
static u32 mt[MAXF];                       /* main frame times (TIMER) */
static int pm[MAXF], pmc[MAXF];
static int g_pre;                          /* project frame of the main start */
#define MAXSUB 9
typedef struct {
    char name[260];
    int off;                               /* start in ms, relative to the main start */
    u8 *data;                              /* the sub .rep file (header + chunks) */
    u32 len;
    int nf, nch;
    u8 *fslot;                             /* frame starts (chunk, offset) and TIMER, MAXF each */
    u32 *foff, *ftime;
    u8 pmap[256], vmap[256];               /* sub pool index -> project pool index */
    u8 pslot[256], vslot[256];             /* project pool slots used by this sub */
    int npslot, nvslot;
    int scanned;                           /* frames whose PED_HEADERs are in lh_pkt */
    int pal_base, pal_n;                   /* its colours at palette[pal_base ..] */
    int dff_orig[16], dff_clone[16], ndff; /* model -> its Looks\SUBn copy */
    int part_on;                           /* Looks\SUBn\particle.cfg read */
    u32 part_size, part_t0, part_t1;
    u8 *tex[256];                          /* textures of its Looks\SUBn .txd files */
    int ntex;
    u8 *skin;                              /* Looks\SUBn\player.bmp */
    u32 skin_size, skin_t0, skin_t1;
    int skin_on;
    char looks[120];                       /* for the status line */
} Sub;
static Sub subs[MAXSUB];
static int g_nsub;                         /* subs in use: SUB 1 .. SUB g_nsub (also their tracks) */
static int ps[MAXSUB][MAXF];               /* project frame -> sub frame, -1 = none */
static u32 orig_addpart;                   /* hooked particle creation of the playback */
typedef int (__stdcall *VirtualFreeFn)(void *, u32, u32);
static VirtualFreeFn pVirtualFree;

static int sub_build(Sub *S)
{
    int c, open = 0;
    u32 t = 0;
    S->nf = 0; S->nch = 0;
    if (!S->data || S->len <= 8) return 0;
    if (!S->fslot && pVirtualAlloc) {
        u8 *m = (u8 *)pVirtualAlloc(0, MAXF * 9, 0x3000, 4);
        if (!m) return 0;
        S->foff = (u32 *)m; S->ftime = (u32 *)(m + MAXF * 4); S->fslot = m + MAXF * 8;
    }
    if (!S->fslot) return 0;
    S->nch = (int)((S->len - 8 + BUFSZ - 1) / BUFSZ);
    if (S->nch > MAXCH) return 0;
    for (c = 0; c < S->nch; c++) {
        u8 *b = S->data + 8 + c * BUFSZ;
        u32 off = 0;
        while (off < BUFSZ) {
            u8 ty = b[off];
            if (ty == 0) break;
            if (ty > 12) return 0;
            if (!open) { if (S->nf >= MAXF) return 0; S->fslot[S->nf] = (u8)c; S->foff[S->nf] = off; open = 1; }
            if (ty == 9) t = *(u32 *)(b + off + 4);
            off += PKT_SIZE[ty];
            if (ty == 8) { S->ftime[S->nf++] = t; open = 0; }
        }
    }
    return S->nf > 0;
}

static int sub_rel(int k, int j) { Sub *S = &subs[k]; return (int)(S->ftime[j] - S->ftime[0]) + S->off; }

/* project frame table from the main table (mt, mnf) and the subs: the main
   frames plus every sub frame before the main start / after the main end
   (all subs merged by time, equal moments once) */
static int build_project(void)
{
    static int tl[MAXF];
    int i, k, p, n = 0, pre = 0, pass, mend, tmin, js[MAXSUB];
    u32 base;
    if (!g_nsub) {
        nfr = mnf; g_pre = 0;
        for (i = 0; i < mnf; i++) { fr_time[i] = mt[i]; pm[i] = pmc[i] = i; }
        return 1;
    }
    mend = (int)(mt[mnf - 1] - mt[0]);
    for (pass = 0; pass < 2; pass++) {           /* 0: before the main start, 1: after its end */
        for (k = 0; k < g_nsub; k++) {
            int j = 0;
            if (pass) while (j < subs[k].nf && sub_rel(k, j) <= mend) j++;
            js[k] = j;
        }
        for (;;) {
            int bk = -1, bt = 0, t;
            for (k = 0; k < g_nsub; k++) {
                if (js[k] >= subs[k].nf) continue;
                t = sub_rel(k, js[k]);
                if (!pass && t >= 0) continue;
                if (bk < 0 || t < bt) { bk = k; bt = t; }
            }
            if (bk < 0) break;
            js[bk]++;
            if (n > 0 && tl[n - 1] == bt) continue;
            if (n >= MAXF) return 0;
            tl[n++] = bt;
        }
        if (!pass) pre = n;
    }
    if (n + mnf > MAXF) return 0;
    tmin = pre ? tl[0] : 0;
    base = (int)mt[0] + tmin < 1 ? (u32)(1 - tmin) : mt[0];
    p = 0;
    for (i = 0; i < pre; i++, p++) { fr_time[p] = base + (u32)tl[i]; pm[p] = -1; pmc[p] = 0; }
    for (i = 0; i < mnf; i++, p++) { fr_time[p] = base + (mt[i] - mt[0]); pm[p] = pmc[p] = i; }
    for (i = pre; i < n; i++, p++) { fr_time[p] = base + (u32)tl[i]; pm[p] = -1; pmc[p] = mnf - 1; }
    nfr = p; g_pre = pre;
    for (k = 0; k < g_nsub; k++) {
        int j = 0, first = sub_rel(k, 0), last = sub_rel(k, subs[k].nf - 1);
        for (p = 0; p < nfr; p++) {
            int T = (int)(fr_time[p] - base);
            if (T < first || T > last) { ps[k][p] = -1; continue; }
            while (j + 1 < subs[k].nf && sub_rel(k, j + 1) <= T) j++;
            ps[k][p] = j;
        }
    }
    return 1;
}

/* ms of project frame p relative to the main start */
static int rel_ms(int p) { return (int)(fr_time[p] - fr_time[g_pre]); }

/* nearest project frame to T (ms relative to the main start) */
static int frame_at_ms(int T)
{
    int lo = 0, hi = nfr - 1, mid;
    if (nfr <= 0) return 0;
    if (T <= rel_ms(0)) return 0;
    if (T >= rel_ms(nfr - 1)) return nfr - 1;
    while (hi - lo > 1) { mid = (lo + hi) / 2; if (rel_ms(mid) <= T) lo = mid; else hi = mid; }
    return T - rel_ms(lo) <= rel_ms(hi) - T ? lo : hi;
}

static int build_table(void)
{
    int c, i;
    u32 t = 0;
    u8 hh = 12, mm = 0;
    nfr = 0;
    nch = 0;
    for (i = 0; i < MAXCH; i++) { slot_first[i] = -1; slot_tail_off[i] = 0xFFFFFFFF; slot_tail_frame[i] = -1; }
    if (!g_src || g_srclen <= 8) return 0;
    nch = (int)((g_srclen - 8 + BUFSZ - 1) / BUFSZ);
    if (nch > MAXCH) { logs("frame table: too many chunks"); return 0; }
    for (c = 0; c < nch; c++) {
        u8 *b = chunk_base(c);
        u32 off = 0, open_off = 0;
        int open = 0, gen = 0;
        slot_first[c] = nfr;
        while (off < BUFSZ) {
            u8 type = b[off];
            if (type == 0) break;
            if (type > 12) { logs("frame table: bad packet type"); return 0; }
            if (!open) {
                if (nfr >= MAXF) { logs("frame table: too many frames"); return 0; }
                fr_slot[nfr] = (u8)c; fr_off[nfr] = off; open = 1; open_off = off; gen = 0;
            }
            if (type == 5) { fr_gslot[nfr] = (u8)c; fr_goff[nfr] = off; gen = 1; }
            if (type == 9) t = *(u32 *)(b + off + 4);
            if (type == 6) { hh = b[off + 1]; mm = b[off + 2]; }
            off += PKT_SIZE[type];
            if (type == 8) {
                if (!gen) { logs("frame table: frame without GENERAL packet"); return 0; }
                fr_time[nfr] = t; fr_hh[nfr] = hh; fr_mm[nfr] = mm; nfr++; open = 0;
            }
        }
        if (open) {                    /* frame continues in the next chunk */
            slot_tail_off[c] = open_off;
            slot_tail_frame[c] = nfr;  /* its entry is (re)created there */
        }
        if (slot_first[c] == nfr) slot_first[c] = -1;
    }
    return nfr > 0;
}

/* index of the next frame PlaybackThisFrame will process; nfr = at the end */
static int cur_index(void)
{
    u32 off = G32(A_PB_OFF);
    int s = G8(A_PB_SLOT), i;
    u8 *base = GPTR(A_PB_BASE);
    if (!base || off >= BUFSZ) return -1;
    if (g_win) {
        if (base[off] == 0) {
            if (G8(A_BUFSTATUS + s) == 2) return w1;
            if (++s >= 8) return -1;
            off = 0;
        }
        for (i = w0; i < w1; i++) if (lw_slot[i] == s && lw_off[i] == off) return i;
        return -1;
    }
    if (base[off] == 0) {
        if (G8(A_BUFSTATUS + s) == 2) return nfr;
        s = (s + 1) & 7;
        off = 0;
    }
    if (off == slot_tail_off[s]) return slot_tail_frame[s];
    i = slot_first[s];
    if (i < 0) return -1;
    for (; i < nfr && fr_slot[i] == s; i++)
        if (fr_off[i] == off) return i;
    return -1;
}

/* recorded player position (GENERAL +0x4C) at fractional frame t */
static void player_pos(float t, float *out)
{
    int a, b, k;
    float u;
    const float *pa, *pb;
    if (nfr <= 0) { out[0] = out[1] = out[2] = 0; return; }
    t = fclamp(t, 0, (float)(nfr - 1));
    a = (int)ffloor_(t);
    b = a + 1 < nfr ? a + 1 : a;
    u = t - (float)a;
    pa = (const float *)(chunk_base(fr_gslot[pmc[a]]) + fr_goff[pmc[a]] + 0x4C);
    pb = (const float *)(chunk_base(fr_gslot[pmc[b]]) + fr_goff[pmc[b]] + 0x4C);
    for (k = 0; k < 3; k++) out[k] = pa[k] + (pb[k] - pa[k]) * u;
}

static int g_installed;

/* ---------------------------------------------------------------- camera */
typedef struct { float pos[3], yaw, pitch, roll, fov; } Pose;

static void pose_vectors(const Pose *p, float *right, float *fwd, float *up)
{
    static const float Z[3] = { 0, 0, 1 };
    float r0[3], u0[3], cr = fcos_(p->roll), sr = fsin_(p->roll), cp = fcos_(p->pitch);
    int k;
    fwd[0] = cp * fcos_(p->yaw); fwd[1] = cp * fsin_(p->yaw); fwd[2] = fsin_(p->pitch);
    vcross(r0, Z, fwd); vnorm(r0);      /* same construction as ProcessLookAroundCam */
    vcross(u0, fwd, r0); vnorm(u0);
    for (k = 0; k < 3; k++) {
        right[k] = r0[k] * cr + u0[k] * sr;
        up[k] = u0[k] * cr - r0[k] * sr;
    }
}

/* write the camera exactly like ProcessLookAroundCam (0x61EE56..0x61EF57) */
static void apply_pose(const Pose *p)
{
    float right[3], fwd[3], up[3], *m = (float *)A_CAM_MATRIX, *rm;
    u8 *rwcam = GPTR(A_SCENE_CAMERA), *frame;
    int k;
    pose_vectors(p, right, fwd, up);
    for (k = 0; k < 3; k++) { m[k] = right[k]; m[4 + k] = fwd[k]; m[8 + k] = up[k]; m[12 + k] = p->pos[k]; }
    if (rwcam && (frame = *(u8 **)(rwcam + 4)) != 0) {
        rm = (float *)(frame + 0x10);
        for (k = 0; k < 3; k++) { rm[k] = right[k]; rm[4 + k] = up[k]; rm[8 + k] = fwd[k]; rm[12 + k] = p->pos[k]; }
        ((ThisFn)F_MAT_UPDATERW)((void *)A_CAM_MATRIX, 0);
        ((P1Fn)F_RWMATRIXUPD)(rm);
        ((P1Fn)F_RWFRAMEUPD)(frame);
        ((P1Fn)F_RWFRAMEUPD2)(frame);
    }
    GF(A_FOV) = p->fov;
}

static void pose_from_camera(Pose *p)
{
    static const float Z[3] = { 0, 0, 1 };
    const float *m = (const float *)A_CAM_MATRIX;
    float f[3], r0[3], u0[3];
    int k;
    for (k = 0; k < 3; k++) { p->pos[k] = m[12 + k]; f[k] = m[4 + k]; }
    vnorm(f);
    p->yaw = fatan2_(f[1], f[0]);
    p->pitch = fatan2_(f[2], fsqrt_(f[0] * f[0] + f[1] * f[1]));
    vcross(r0, Z, f); vnorm(r0);
    vcross(u0, f, r0); vnorm(u0);
    p->roll = fatan2_(vdot(m, u0), vdot(m, r0));
    p->fov = GF(A_FOV);
    if (p->fov < 5.0f || p->fov > 170.0f) p->fov = REPLAY_FOV;
}

static void look_at(Pose *p, const float *target)
{
    float d[3];
    int k;
    for (k = 0; k < 3; k++) d[k] = target[k] - p->pos[k];
    p->yaw = fatan2_(d[1], d[0]);
    p->pitch = fatan2_(d[2], fsqrt_(d[0] * d[0] + d[1] * d[1]));
}

/* ------------------------------------------------------------- keyframes */
/* values are stored in .prj files: append only */
enum { KF_FREE, KF_LOOK, KF_FOLLOW, KF_RECORDED, KF_FOLLOWLOOK, KF_NTYPES };
static const char *KF_NAME[KF_NTYPES] = { "FREE", "LOOK AT PLAYER", "FOLLOW PLAYER", "RECORDED CAMERA", "FOLLOW & LOOK AT PLAYER" };
static const u8 KF_COL[KF_NTYPES][3] = { { 80, 200, 255 }, { 255, 215, 60 }, { 120, 230, 120 }, { 170, 170, 170 }, { 255, 140, 200 } };
/* T: RECORDED > FOLLOW > LOOK > FOLLOW & LOOK > FREE > RECORDED */
static const u8 KF_NEXT[KF_NTYPES] = { KF_RECORDED, KF_FOLLOWLOOK, KF_LOOK, KF_FOLLOW, KF_FREE };
#define LOOK_Z 0.6f                    /* aim slightly above the recorded player origin */

typedef struct {
    int frame;
    Pose p;
    float off[3];                      /* p.pos - player position at frame (FOLLOW) */
    u8 type, cut;
} Key;

#define MAXKF 64
static Key kf[MAXKF];
static int nkf;
static Key hkf[MAXKF];                     /* camera keys outside the project range (kept) */
static int hkt[MAXKF], nhk;                /* their times, ms rel. to the main start */

/* *added = 1 for a new keyframe, 0 when the one at this frame was updated */
static int add_key(int frame, const Pose *p, int *added)
{
    float pl[3];
    int i, j, k;
    for (i = 0; i < nkf && kf[i].frame < frame; i++) {}
    if (i < nkf && kf[i].frame == frame) {          /* update, keep type / cut */
        kf[i].p = *p;
        *added = 0;
    } else {
        if (nkf + nhk >= MAXKF) return -1;
        for (j = nkf; j > i; j--) kf[j] = kf[j - 1];
        nkf++;
        kf[i].frame = frame; kf[i].p = *p; kf[i].cut = 0;
        /* same type as the previous keyframe in time (FREE when it is the first) */
        kf[i].type = i > 0 ? kf[i - 1].type : (u8)KF_FREE;
        *added = 1;
    }
    player_pos((float)frame, pl);
    for (k = 0; k < 3; k++) kf[i].off[k] = p->pos[k] - pl[k];
    return i;
}

static void delete_key(int i)
{
    int j;
    if (i < 0 || i >= nkf) return;
    for (j = i; j < nkf - 1; j++) kf[j] = kf[j + 1];
    nkf--;
}

/* camera for fractional frame t; 0 = no override (recorded camera) */
/* spline through p1 -> p2, or a straight line (LINEAR) */
static float seg_interp(float p0, float p1, float p2, float p3, float u, int lin)
{
    return lin ? p1 + (p2 - p1) * u : catmull(p0, p1, p2, p3, u);
}

static int eval_path(float t, Pose *out)
{
    int i, i0, i2, i3, type, k, ip, fol, lin;
    float u = 0, pl[3], tgt[3];
    const Key *a, *b, *c, *d;
    if (nkf == 0 || t < (float)kf[0].frame) return 0;
    for (i = 0; i + 1 < nkf && (float)kf[i + 1].frame <= t; i++) {}
    type = kf[i].type;
    if (type == KF_RECORDED) return 0;
    ip = kf[i].cut < IP_N ? kf[i].cut : IP_SMOOTH;
    if (i + 1 < nkf && ip != IP_CUT) u = (t - (float)kf[i].frame) / (float)(kf[i + 1].frame - kf[i].frame);
    i0 = i > 0 ? i - 1 : i;
    i2 = i + 1 < nkf ? i + 1 : i;
    i3 = i + 2 < nkf ? i + 2 : i2;
    if (i + 1 >= nkf || ip == IP_CUT) { i0 = i2 = i3 = i; u = 0; }
    a = &kf[i0]; b = &kf[i]; c = &kf[i2]; d = &kf[i3];
    fol = type == KF_FOLLOW || type == KF_FOLLOWLOOK;
    lin = ip == IP_LINEAR;
    if (ip == IP_CONST && u > 0) {                 /* constant speed along the path: arc-length table */
        float len[17], q[3], prev[3], tot = 0, want;
        int s;
        for (s = 0; s <= 16; s++) {
            float us = (float)s / 16.0f;
            for (k = 0; k < 3; k++)
                q[k] = fol ? catmull(a->off[k], b->off[k], c->off[k], d->off[k], us) : catmull(a->p.pos[k], b->p.pos[k], c->p.pos[k], d->p.pos[k], us);
            if (s) { float dv[3]; dv[0] = q[0] - prev[0]; dv[1] = q[1] - prev[1]; dv[2] = q[2] - prev[2]; tot += fsqrt_(vdot(dv, dv)); }
            len[s] = tot;
            prev[0] = q[0]; prev[1] = q[1]; prev[2] = q[2];
        }
        if (tot > 0.05f) {                         /* hardly moving (only turning): keep u */
            want = u * tot;
            for (s = 1; s < 16 && len[s] < want; s++) {}
            u = ((float)(s - 1) + (len[s] > len[s - 1] ? (want - len[s - 1]) / (len[s] - len[s - 1]) : 0.0f)) / 16.0f;
            u = fclamp(u, 0.0f, 1.0f);
        }
    } else u = ease(ip, u);
    player_pos(t, pl);
    for (k = 0; k < 3; k++) {
        if (fol) out->pos[k] = pl[k] + seg_interp(a->off[k], b->off[k], c->off[k], d->off[k], u, lin);
        else out->pos[k] = seg_interp(a->p.pos[k], b->p.pos[k], c->p.pos[k], d->p.pos[k], u, lin);
    }
    out->roll = seg_interp(a->p.roll, b->p.roll, c->p.roll, d->p.roll, u, lin);
    out->fov = fclamp(seg_interp(a->p.fov, b->p.fov, c->p.fov, d->p.fov, u, lin), 5.0f, 170.0f);
    if (type == KF_FREE || type == KF_FOLLOW) {   /* the keyframes' own direction */
        float y1 = b->p.yaw, y0 = wrap_near(a->p.yaw, y1), y2 = wrap_near(c->p.yaw, y1), y3 = wrap_near(d->p.yaw, y2);
        out->yaw = seg_interp(y0, y1, y2, y3, u, lin);
        out->pitch = fclamp(seg_interp(a->p.pitch, b->p.pitch, c->p.pitch, d->p.pitch, u, lin), -1.55f, 1.55f);
    } else {
        tgt[0] = pl[0]; tgt[1] = pl[1]; tgt[2] = pl[2] + LOOK_Z;
        look_at(out, tgt);
    }
    return 1;
}

/* ---------------------------------------------------------------- editor */
static const int SPEED_DIV[5] = { 4, 2, 1, 1, 1 };
static const int SPEED_REP[5] = { 1, 1, 1, 2, 4 };
static const char *SPEED_TXT[5] = { "X0.25", "X0.5", "X1", "X2", "X4" };
enum { CAM_RECORDED, CAM_PATH, CAM_FREE };

static struct {
    int active, paused, need_step, speed;
    int freecam, sel, help, cam_shown, fov_touched;
    Pose fc;
    float t;
    u32 last_ms;
    int in, out, limit;                /* trim range and where playback stops */
    int viewer;                        /* PLAY of a .prj: no HUD / keys, stops at the end mark */
    int track;                         /* TR_CAM / TR_WEATHER / TR_TIME: what K, DEL, T, B ... edit */
    int dirty;                         /* changes since open / F7 */
    u32 exit_until;                    /* F6 with unsaved changes: second F6 before this exits */
    char src[260];                     /* source .rep (user folder) */
    char base[260];                    /* src without .rep: saves <base>.edit.<date-time>.prj */
    char prj[300];                     /* project file being edited ("" = not saved yet): F7 overwrites it */
    u32 msg_until;
    int msg_toast;
    char msg[160];
} ed;

static TimeFn pTimeGetTime;
static u8 key_now[256], key_prev[256];
static int key_hold[256];

/* [Keys] in ReplayEditor.ini: MENU / SAVE / SAVE_AS = F4 .. F12 (VK 0x73 .. 0x7B).
   Texts use \1 \2 \3 for the configured key names (kput). */
enum { KEY_MENU, KEY_SAVE, KEY_SAVEAS, NKEYS };
static int g_key[NKEYS] = { 0x75, 0x76, 0x77 };            /* F6 / F7 / F8 */
static char g_keyname[NKEYS][4] = { "F6", "F7", "F8" };
static int g_keydup;                                         /* two settings share a key */
static int g_keybad;                                         /* bit per setting: invalid, default used */
#define K_MENU   g_key[KEY_MENU]
#define K_SAVE   g_key[KEY_SAVE]
#define K_SAVEAS g_key[KEY_SAVEAS]

static char *kput(char *d, const char *s)
{
    for (; *s; s++) {
        if (*s >= 1 && *s <= NKEYS) d = sput(d, g_keyname[*s - 1]);
        else *d++ = *s;
    }
    *d = 0;
    return d;
}

/* "F4" .. "F12" (any case, spaces around ignored) -> VK code, 0 = invalid */
static int parse_fkey(const char *v)
{
    int n = 0, digits = 0;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != 'F' && *v != 'f') return 0;
    for (v++; *v >= '0' && *v <= '9'; v++) { n = n * 10 + (*v - '0'); digits++; }
    while (*v == ' ' || *v == '\t') v++;
    if (*v || !digits || digits > 2 || n < 4 || n > 12) return 0;
    return 0x6F + n;                                         /* VK_F1 = 0x70 */
}

static void poll_keys(void)
{
    static const u8 used[] = { 0x08, 0x09, 0x0D, 0x10, 0x11, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
                               0x27, 0x28, 0x2E, 0x31, 0x32, 0x33, 0x34, 0x35, 'A', 'B', 'C', 'D', 'E',
                               'H', 'I', 'K', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'W', 'Z', 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B,
                               0xBB, 0xBC, 0xBD, 0xBE, 0xDB, 0xDD };
    int i;
    for (i = 0; i < (int)sizeof(used); i++) {
        int k = used[i];
        key_prev[k] = key_now[k];
        key_now[k] = pGetAsyncKeyState && (pGetAsyncKeyState(k) & 0x8000) ? 1 : 0;
        key_hold[k] = key_now[k] ? key_hold[k] + 1 : 0;
    }
}
static int pressed(int k) { return key_now[k] && !key_prev[k]; }
/* pressed, then auto-repeat after 12 frames every 2 frames */
static int repeat(int k) { return pressed(k) || (key_hold[k] > 12 && (key_hold[k] & 1)); }

static u32 now_ms(void) { return pTimeGetTime ? pTimeGetTime() : 0; }

/* ms > 0: short feedback box above the editor panel for ms milliseconds;
   ms == 0: large message box for 3 s (errors) */
static void message(const char *s, int ms)
{
    int n = slen(s);
    if (n > 159) n = 159;
    memcpy(ed.msg, s, n); ed.msg[n] = 0;
    ed.msg_toast = ms > 0;
    ed.msg_until = now_ms() + (ms > 0 ? (u32)ms : 3000u);
}
static int message_visible(void) { return ed.msg[0] && (int)(ed.msg_until - now_ms()) > 0; }

static void set_speed(int sp)
{
    ed.speed = sp;
    G32(A_DIVISOR) = SPEED_DIV[sp];
    G32(A_INTERP_CNT) = 0;
}

static void call_playback(void) { ((vfn)orig_playback)(); }

static void window_goto(int f);

/* play frames until frame f has been processed (cur == f + 1) */
static void seek(int f)
{
    int s, guard = 0;
    if (nfr <= 0 || G8(A_MODE) != 1) return;
    if (f < 0) f = 0;
    if (f >= nfr) f = nfr - 1;
    G32(A_DIVISOR) = 1;
    G32(A_INTERP_CNT) = 0;
    if (g_win) {                          /* window: position on f directly, play it */
        window_goto(f);
        call_playback();
        set_speed(ed.speed);
        return;
    }
    if (cur_index() != f) {               /* not simply the next frame */
        s = fr_slot[f];
        G8(A_PB_SLOT) = (u8)s;
        G32(A_PB_OFF) = 0;
        GPTR(A_PB_BASE) = game_slot(s);
    }
    while (G8(A_MODE) == 1 && guard++ < MAXF) {
        int c = cur_index();
        if (c < 0 || c > f) break;
        call_playback();
    }
    set_speed(ed.speed);
}

static int shown_frame(void)
{
    int c = cur_index();
    if (c < 0) return -1;
    return c > 0 ? c - 1 : 0;
}

/* fractional frame on screen, valid right after PlaybackThisFrame updated
   the counter (and while paused): see PlaybackThisFrame 0x623183.. */
static float playback_t(void)
{
    int c = cur_index();
    u32 div = G32(A_DIVISOR), cnt = G32(A_INTERP_CNT);
    float t;
    if (c < 0) return ed.t;
    if (div < 1) div = 1;
    if (cnt == 0) t = (float)(c - 1) + (float)(div - 1) / (float)div;
    else t = (float)c + (float)(cnt - 1) / (float)div;
    return fclamp(t, 0, (float)(nfr - 1));
}

/* ------------------------------------------------- weather / time tracks
   Every recorded frame has a CLOCK and a WEATHER packet; PlaybackThisFrame
   sets the clock and Old/New weather + interpolation from them. While Mode==1
   CWeather::Update (0x57D580, before the replay) only derives rain/fog/wind
   from Old/New/Interp, and CTimeCycle::Update (0x4CEA40, after the replay)
   blends the [hour][weather] tables with (minutes + seconds/60)/60 - so
   overwriting these values right after the packets is all it takes. */
#define A_WEATHER_OLD    0xA10AAA   /* short */
#define A_WEATHER_NEW    0xA10A2E   /* short */
#define A_WEATHER_INTERP 0x9787D8   /* float, 0 = old .. 1 = new */
#define A_CLOCK_HOURS    0xA10B6B   /* u8 */
#define A_CLOCK_MINUTES  0xA10B92   /* u8 */
#define A_CLOCK_SECONDS  0xA10A3C   /* u16 */
#define G16(a) (*(volatile short *)(a))
enum { TR_CAM, TR_WEATHER, TR_TIME, TR_SUB, TR_N };
static const char *TR_NAME[TR_N] = { "CAMERA", "WEATHER", "TIME", "SUB 1" };
static const char *track_name(int tr)                 /* SUB 1 .. SUB 9 for tr >= TR_SUB */
{
    static char nm[MAXSUB][8];
    int k = tr - TR_SUB;
    if (tr < TR_SUB) return TR_NAME[tr];
    nm[k][0] = 'S'; nm[k][1] = 'U'; nm[k][2] = 'B'; nm[k][3] = ' '; nm[k][4] = (char)('1' + k); nm[k][5] = 0;
    return nm[k];
}
static int ntracks(void) { return TR_SUB + g_nsub; }          /* a SUB n track for every sub */
#define NWEATHER 7
static const char *WEATHER_NAME[NWEATHER] = { "SUNNY", "CLOUDY", "RAINY", "FOGGY", "EXTRA SUNNY", "HURRICANE", "EXTRA COLOURS" };
static const u8 WEATHER_COL[NWEATHER][3] = { { 255, 205, 60 }, { 150, 160, 175 }, { 60, 110, 230 }, { 205, 205, 205 },
                                             { 255, 140, 40 }, { 140, 60, 210 }, { 230, 90, 170 } };
typedef struct { int frame; short val; u8 cut, rsv; } EKey;   /* val: weather type / minute of the day */
#define MAXEK 64
static EKey ek[2][MAXEK];                  /* [0] weather, [1] time */
static int nek[2];
static EKey hek[2][MAXEK];                 /* keys outside the project range (kept) */
static int hekt[2][MAXEK], nhek[2];
static int esel[2] = { -1, -1 };

/* segment of env track tr at fractional frame t; 0 = before the first key */
static int env_seg(int tr, float t, int *i0, float *u)
{
    int n = nek[tr], i;
    if (!n || t < (float)ek[tr][0].frame) return 0;
    for (i = 0; i + 1 < n && (float)ek[tr][i + 1].frame <= t; i++) {}
    *i0 = i;
    *u = 0;
    if (i + 1 < n && ek[tr][i].cut != IP_CUT)
        *u = ease(ek[tr][i].cut, (t - (float)ek[tr][i].frame) / (float)(ek[tr][i + 1].frame - ek[tr][i].frame));
    return 1;
}

/* minute of the day (float) between two time keys: always forward in time */
static float time_lerp(int a, int b, float u)
{
    float m = (float)a + (float)((b - a + 1440) % 1440) * u;
    return m >= 1440.0f ? m - 1440.0f : m;
}

static void env_eval(float t, int *w_old, int *w_new, float *w_u, float *minute, int *has_w, int *has_t)
{
    int i;
    float u;
    *has_w = env_seg(0, t, &i, &u);
    if (*has_w) { *w_old = ek[0][i].val; *w_new = u > 0 ? ek[0][i + 1].val : ek[0][i].val; *w_u = u; }
    *has_t = env_seg(1, t, &i, &u);
    if (*has_t) *minute = u > 0 ? time_lerp(ek[1][i].val, ek[1][i + 1].val, u) : (float)ek[1][i].val;
}

static void apply_env(float t)
{
    int wo, wn, hw, ht, sec;
    float wu, m;
    env_eval(t, &wo, &wn, &wu, &m, &hw, &ht);
    if (hw) { G16(A_WEATHER_OLD) = (short)wo; G16(A_WEATHER_NEW) = (short)wn; GF(A_WEATHER_INTERP) = wu; }
    if (!ht && nfr > 0) {
        /* no time key here: the recorded clock of the frame. While paused no
           CLOCK packet is played and CClock::Update would let the time run on. */
        int f = (int)t;
        if (f < 0) f = 0;
        if (f >= nfr) f = nfr - 1;
        m = (float)((fr_hh[pmc[f]] % 24) * 60 + fr_mm[pmc[f]] % 60);
        ht = 1;
    }
    if (ht) {
        sec = (int)(m * 60.0f);
        if (sec < 0) sec = 0;
        if (sec >= 86400) sec = 86399;
        G8(A_CLOCK_HOURS) = (u8)(sec / 3600);
        G8(A_CLOCK_MINUTES) = (u8)(sec / 60 % 60);
        G16(A_CLOCK_SECONDS) = (short)(sec % 60);
    }
}

static int cur_weather(void)
{
    int w = GF(A_WEATHER_INTERP) >= 0.5f ? G16(A_WEATHER_NEW) : G16(A_WEATHER_OLD);
    return w >= 0 && w < NWEATHER ? w : 0;
}
static int cur_minute(void) { return (G8(A_CLOCK_HOURS) % 24) * 60 + G8(A_CLOCK_MINUTES) % 60; }

static char *hhmm(char *d, int m)
{
    m = ((m % 1440) + 1440) % 1440;
    d = iput(d, m / 60, 2); *d++ = ':';
    return iput(d, m % 60, 2);
}

static void editor_camera(void)
{
    Pose p;
    ed.t = playback_t();
    apply_env(ed.t);
    if (ed.freecam) { apply_pose(&ed.fc); ed.cam_shown = CAM_FREE; ed.fov_touched = 1; return; }
    if (eval_path(ed.t, &p)) { apply_pose(&p); ed.cam_shown = CAM_PATH; ed.fov_touched = 1; return; }
    if (ed.fov_touched) { GF(A_FOV) = REPLAY_FOV; ed.fov_touched = 0; }
    ed.cam_shown = CAM_RECORDED;
    ((vfn)orig_lookaround)();          /* recorded camera + mouse look-around */
}

static void update_freecam(float dt)
{
    float right[3], fwd[3], up[3], sp, mv[3] = { 0, 0, 0 };
    int k;
    if (!ed.freecam) return;
    ed.fc.yaw -= GF(A_MOUSE_X) * 0.005f;          /* same sensitivity / signs as the look-around cam */
    ed.fc.pitch = fclamp(ed.fc.pitch - GF(A_MOUSE_Y) * 0.005f, -1.55f, 1.55f);
    sp = 12.0f * dt * (key_now[0x10] ? 5.0f : 1.0f) * (key_now[0x11] ? 0.2f : 1.0f);
    pose_vectors(&ed.fc, right, fwd, up);
    for (k = 0; k < 3; k++) {
        if (key_now['W']) mv[k] += fwd[k];
        if (key_now['S']) mv[k] -= fwd[k];
        if (key_now['A']) mv[k] += right[k];      /* GTA's "right" vector points to the left */
        if (key_now['D']) mv[k] -= right[k];
    }
    if (key_now['E']) mv[2] += 1;
    if (key_now['Q']) mv[2] -= 1;
    for (k = 0; k < 3; k++) ed.fc.pos[k] += mv[k] * sp;
    if (key_now['Z']) ed.fc.roll -= 1.0f * dt;
    if (key_now['C']) ed.fc.roll += 1.0f * dt;
    if (pressed('R')) ed.fc.roll = 0;
    if (key_now[0xBD]) ed.fc.fov += 30.0f * dt;   /* '-' zoom out */
    if (key_now[0xBB]) ed.fc.fov -= 30.0f * dt;   /* '=' zoom in  */
    ed.fc.fov = fclamp(ed.fc.fov, 10.0f, 120.0f);
}

static void select_key(int i)
{
    if (i < 0 || i >= nkf) return;
    ed.sel = i;
    ed.paused = 1;
    seek(kf[i].frame);
    if (ed.freecam) ed.fc = kf[i].p;               /* edit it from where it was taken */
}

static int key_at_frame(int f)
{
    int i;
    for (i = 0; i < nkf; i++) if (kf[i].frame == f) return i;
    return -1;
}

/* ------------------------------------------------- trim, save, project */
static u8 *g_out;                          /* rebuilt replay: 8 x BUFSZ */
static u8 lh_pkt[256][8], lh_ok[256];      /* last PED_HEADER per ped slot */
static u8 emitted[256];                    /* headers already in the current output buffer */
#define MAXPK 16384
static u8 *g_pk[MAXPK];

/* packets of frame f in order, following a frame that starts with particle
   packets at the tail of one slot into the next slot */
static int walk_packets(u8 *data, int nchunks, int s, u32 off, u8 **pk, int max)
{
    int n = 0, guard = 0;
    while (n < max && guard++ < 400000) {
        u8 *b = data + 8 + s * BUFSZ, t;
        if (off >= BUFSZ || b[off] == 0) { if (++s >= nchunks) return -1; off = 0; continue; }
        t = b[off];
        if (t > 12) return -1;
        pk[n++] = b + off;
        off += PKT_SIZE[t];
        if (t == 8) return n;
    }
    return -1;
}

/* f = MAIN frame */
static int frame_packets(int f, u8 **pk, int max)
{
    int s = fr_slot[f], k;
    u32 off = fr_off[f];
    for (k = 0; k < nch; k++)
        if (slot_tail_frame[k] == f && slot_tail_off[k] != 0xFFFFFFFF) { s = k; off = slot_tail_off[k]; break; }
    return walk_packets(g_src, nch, s, off, pk, max);
}

static u8 *g_spk[MAXPK];
static int sub_packets(int k, int j, u8 **pk, int max)
{
    Sub *S = &subs[k];
    return walk_packets(S->data, S->nch, S->fslot[j], S->foff[j], pk, max);
}

/* sub pool indices -> indices the main replay (and earlier subs) never use */
static int build_remap(void)
{
    static u8 mp[256], mv[256], sp[256], sv[256];
    int f, i, n, k, q;
    memset(mp, 0, 256); memset(mv, 0, 256);
    for (f = 0; f < mnf; f++) {
        n = frame_packets(f, g_pk, MAXPK);
        for (i = 0; i < n; i++) {
            u8 t = g_pk[i][0];
            if (t == 1 || t == 2) mv[g_pk[i][1]] = 1; else if (t == 3 || t == 4) mp[g_pk[i][1]] = 1;
        }
    }
    for (q = 0; q < g_nsub; q++) {
        Sub *S = &subs[q];
        memset(sp, 0, 256); memset(sv, 0, 256);
        for (f = 0; f < S->nf; f++) {
            n = sub_packets(q, f, g_spk, MAXPK);
            for (i = 0; i < n; i++) {
                u8 t = g_spk[i][0];
                if (t == 1 || t == 2) sv[g_spk[i][1]] = 1; else if (t == 3 || t == 4) sp[g_spk[i][1]] = 1;
            }
        }
        memset(S->pmap, 0xFF, 256); memset(S->vmap, 0xFF, 256);
        for (i = 0; i < 256; i++) {
            if (sp[i] && !mp[i] && i < PED_POOL) { S->pmap[i] = (u8)i; mp[i] = 1; }
            if (sv[i] && !mv[i] && i < VEH_POOL) { S->vmap[i] = (u8)i; mv[i] = 1; }
        }
        for (i = 0; i < 256; i++) {
            if (sp[i] && S->pmap[i] == 0xFF) {
                for (k = 0; k < PED_POOL && mp[k]; k++) {}
                if (k >= PED_POOL) return 0;
                S->pmap[i] = (u8)k; mp[k] = 1;
            }
            if (sv[i] && S->vmap[i] == 0xFF) {
                for (k = 0; k < VEH_POOL && mv[k]; k++) {}
                if (k >= VEH_POOL) return 0;
                S->vmap[i] = (u8)k; mv[k] = 1;
            }
        }
        S->npslot = S->nvslot = 0;
        for (i = 0; i < 256; i++) {
            if (sp[i]) S->pslot[S->npslot++] = S->pmap[i];
            if (sv[i]) S->vslot[S->nvslot++] = S->vmap[i];
        }
    }
    return 1;
}

/* GENERAL +4 is a CMatrix: right, forward, up, pos rows of 16 bytes */
static void bake_general(u8 *g, int f)
{
    Pose p;
    float r[3], fw[3], up[3], *m = (float *)(g + 4);
    int k;
    if (!eval_path((float)f, &p)) return;          /* recorded camera stays */
    pose_vectors(&p, r, fw, up);
    for (k = 0; k < 3; k++) { m[k] = r[k]; m[4 + k] = fw[k]; m[8 + k] = up[k]; m[12 + k] = p.pos[k]; }
}

static int g_bake;                         /* encode_frame: bake the edited camera (export only) */

/* ---- merged frames (main + SUB 1 .. SUB n) */
static u8 sub_scr[0x40000];                /* remapped sub packets of one frame */
static u8 *g_sall[MAXPK];                  /* kept sub packets of one frame */
static u8 *mv_pk[1024], *mp_pk[1024];
static u8 pres_prev[256];                  /* peds of the previous encoded frame */

static int clone_id(Sub *S, int mi)
{
    int i;
    for (i = 0; i < S->ndff; i++) if (S->dff_orig[i] == mi) return S->dff_clone[i];
    return -1;
}

static void remap_sub(int k, u8 *q)
{
    Sub *S = &subs[k];
    u8 t = q[0];
    if (t == 1 || t == 2 || t == 3) {             /* model with a Looks\SUBn .dff -> its copy */
        int off = t == 3 ? 2 : 0x1A, c = clone_id(S, *(short *)(q + off));
        if (c >= 0) {
            *(short *)(q + off) = (short)c;
            if (t == 3 && q[4] == 0) q[4] = 4;     /* CPlayerPed always uses model 0: civilian ped instead */
        }
    }
    if (t == 11 && S->part_on && orig_addpart) {  /* own particle.cfg: type | 0x80, sub number in the size's low bits */
        q[1] |= 0x80;
        *(u32 *)(q + 0x10) = (*(u32 *)(q + 0x10) & ~0xFu) | (u32)(k + 1);
    }
    if (t == 1 || t == 2) {                        /* colours of its carcols.dat */
        int c0 = t == 1 ? 0x2D : 0x28, j;
        for (j = c0; j < c0 + 2; j++) if (q[j] < S->pal_n) q[j] = (u8)(q[j] + S->pal_base);
    }
    if (t == 1 || t == 2) q[1] = S->vmap[q[1]];
    else if (t == 3) q[1] = S->pmap[q[1]];
    else if (t == 4) { q[1] = S->pmap[q[1]]; if (q[3]) q[3] = (u8)(S->vmap[q[3] - 1] + 1); }
}

static void sub_headers_upto(int k, int j)
{
    Sub *S = &subs[k];
    int n, i;
    u8 h[8];
    while (S->scanned < j) {
        S->scanned++;
        n = sub_packets(k, S->scanned, g_spk, MAXPK);
        for (i = 0; i < n; i++)
            if (g_spk[i][0] == 3) {
                memcpy(h, g_spk[i], 8); remap_sub(k, h);
                if (h[1] != 0xFF) { memcpy(lh_pkt[h[1]], h, 8); lh_ok[h[1]] = 1; }
            }
    }
}

static void sort_by_index(u8 **a, int n)
{
    int i, j;
    u8 *x;
    for (i = 1; i < n; i++) {
        x = a[i];
        for (j = i; j > 0 && a[j - 1][1] > x[1]; j--) a[j] = a[j - 1];
        a[j] = x;
    }
}

#define PUT(q) do { int z_ = PKT_SIZE[(q)[0]]; if (len + z_ > cap) return -1; memcpy(dst + len, (q), z_); len += z_; } while (0)

/* project frame p: main packets (globals from the clamped main frame where
   there is no main) + remapped sub packets; recorded order PARTICLE*, GENERAL,
   CLOCK, WEATHER, TIMER, vehicles, (PED_HEADER) PED_UPDATE, BULLET, MISC, END */
static int encode_merged(int p, u8 *dst, int cap)
{
    int mi = pm[p], nm, ns = 0, i, len = 0, nv = 0, np = 0, used = 0, k;
    u8 cur[256], tp[8];
    nm = frame_packets(pmc[p], g_pk, MAXPK);
    if (nm < 0) return -2;
    for (k = 0; k < g_nsub; k++) if (ps[k][p] >= 0) sub_headers_upto(k, ps[k][p]);
    for (k = 0; k < g_nsub; k++) {
        int si = ps[k][p], n;
        if (si < 0) continue;
        n = sub_packets(k, si, g_spk, MAXPK);
        if (n < 0) return -2;
        for (i = 0; i < n && ns < MAXPK; i++) {
            u8 t = g_spk[i][0];
            int sz = PKT_SIZE[t];
            if ((t != 1 && t != 2 && t != 4 && t != 11) || (t == 11 && p > 0 && ps[k][p - 1] == si)) continue;
            if (used + sz > (int)sizeof(sub_scr)) return -2;
            memcpy(sub_scr + used, g_spk[i], sz);
            remap_sub(k, sub_scr + used);
            g_sall[ns++] = sub_scr + used;
            used += sz;
        }
    }
    if (mi >= 0)
        for (i = 0; i < nm; i++)
            if (g_pk[i][0] == 3) { memcpy(lh_pkt[g_pk[i][1]], g_pk[i], 8); lh_ok[g_pk[i][1]] = 1; }
    if (mi >= 0) for (i = 0; i < nm; i++) if (g_pk[i][0] == 11) PUT(g_pk[i]);
    for (i = 0; i < ns; i++) if (g_sall[i][0] == 11) PUT(g_sall[i]);
    for (i = 0; i < nm; i++) if (g_pk[i][0] >= 5 && g_pk[i][0] <= 7) PUT(g_pk[i]);
    tp[0] = 9; tp[1] = tp[2] = tp[3] = 0; *(u32 *)(tp + 4) = fr_time[p];
    PUT(tp);
    if (mi >= 0) for (i = 0; i < nm && nv < 1024; i++) if (g_pk[i][0] == 1 || g_pk[i][0] == 2) mv_pk[nv++] = g_pk[i];
    for (i = 0; i < ns && nv < 1024; i++) if (g_sall[i][0] == 1 || g_sall[i][0] == 2) mv_pk[nv++] = g_sall[i];
    sort_by_index(mv_pk, nv);
    for (i = 0; i < nv; i++) PUT(mv_pk[i]);
    if (mi >= 0) for (i = 0; i < nm && np < 1024; i++) if (g_pk[i][0] == 4) mp_pk[np++] = g_pk[i];
    for (i = 0; i < ns && np < 1024; i++) if (g_sall[i][0] == 4) mp_pk[np++] = g_sall[i];
    sort_by_index(mp_pk, np);
    memset(cur, 0, sizeof(cur));
    for (i = 0; i < np; i++) {
        u8 idx = mp_pk[i][1];
        if ((!emitted[idx] || !pres_prev[idx]) && lh_ok[idx]) {   /* new in this buffer or (re)appearing */
            if (len + 8 > cap) return -1;
            memcpy(dst + len, lh_pkt[idx], 8); len += 8; emitted[idx] = 1;
        }
        PUT(mp_pk[i]);
        cur[idx] = 1;
    }
    if (mi >= 0) for (i = 0; i < nm; i++) if (g_pk[i][0] == 10) PUT(g_pk[i]);
    for (i = 0; i < nm; i++) if (g_pk[i][0] == 12) PUT(g_pk[i]);
    for (i = 0; i < nm; i++) if (g_pk[i][0] == 8) PUT(g_pk[i]);
    memcpy(pres_prev, cur, sizeof(cur));
    return len;
}
#undef PUT

/* same rules as tools/reptrim.py; -1 = does not fit, -2 = bad data */
static int encode_frame(int f, u8 *dst, int cap)
{
    int n, i, len = 0, sz, idx;
    if (g_nsub) return encode_merged(f, dst, cap);
    n = frame_packets(f, g_pk, MAXPK);
    if (n < 0) return -2;
    for (i = 0; i < n; i++) {
        u8 *p = g_pk[i], t = p[0];
        sz = PKT_SIZE[t];
        idx = p[1];
        if (t == 3) {
            memcpy(lh_pkt[idx], p, 8); lh_ok[idx] = 1;
            if (emitted[idx]) continue;
            emitted[idx] = 1;
        } else if (t == 4 && !emitted[idx] && lh_ok[idx]) {
            if (len + 8 > cap) return -1;
            memcpy(dst + len, lh_pkt[idx], 8); len += 8; emitted[idx] = 1;
        }
        if (len + sz > cap) return -1;
        memcpy(dst + len, p, sz);
        if (t == 5 && g_bake) bake_general(dst + len, f);
        len += sz;
    }
    return len;
}

static int rebuild(int in, int out, int *nbuf)
{
    int f, cur = 0, used = 0, n, i, m;
    if (!g_out && pVirtualAlloc) g_out = (u8 *)pVirtualAlloc(0, 8 * BUFSZ, 0x3000, 4);
    if (!g_out) return 0;
    g_bake = 1;
    memset(g_out, 0, 8 * BUFSZ);
    memset(lh_ok, 0, sizeof(lh_ok));
    memset(emitted, 0, sizeof(emitted));
    for (f = 0; f < in; f++) {                    /* headers recorded before the start */
        m = frame_packets(f, g_pk, MAXPK);
        for (i = 0; i < m; i++)
            if (g_pk[i][0] == 3) { memcpy(lh_pkt[g_pk[i][1]], g_pk[i], 8); lh_ok[g_pk[i][1]] = 1; }
    }
    for (f = in; f <= out; f++) {
        n = encode_frame(f, g_out + cur * BUFSZ + used, BUFSZ - 1 - used);
        if (n == -1) {                            /* next buffer; END = zero padding */
            memset(g_out + cur * BUFSZ + used, 0, BUFSZ - used);
            if (++cur >= 8) return 0;
            used = 0;
            memset(emitted, 0, sizeof(emitted));
            n = encode_frame(f, g_out + cur * BUFSZ, BUFSZ - 1);
        }
        if (n < 0) return 0;
        used += n;
    }
    *nbuf = cur + 1;
    return 1;
}

/* window mode: frames start.. re-encoded into the game's 8 slots (PED_HEADER
   re-injection as for the trim). Then the game streams the models in them. */
#define F_TRIGGER      0x6209E0   /* CReplay::TriggerPlayback(char, float, float, float, char) */
#define F_STREAM_ALL   0x61F050   /* StreamAllNecessaryCarsAndPeds (+ LoadAllRequestedModels) */
#define A_PLAYING_HD   0xA10AF3   /* set by PlayReplayFromHD */
#define A_LOOK_ALLOWED 0xA10B97
typedef void (__cdecl *TrigFn)(int, float, float, float, int);

static int fill_window(int start)
{
    int f, cur = 0, used = 0, n, i, m;
    if (start < 0) start = 0;
    if (start >= nfr) start = nfr - 1;
    g_bake = 0;
    memset(lh_ok, 0, sizeof(lh_ok));
    memset(emitted, 0, sizeof(emitted));
    if (g_nsub) {                                 /* peds announced before the window */
        int last_sub[MAXSUB], k;
        memset(pres_prev, 0, sizeof(pres_prev));
        for (k = 0; k < g_nsub; k++) { subs[k].scanned = -1; last_sub[k] = -1; }
        for (f = 0; f < start; f++) {
            if (pm[f] >= 0) {
                m = frame_packets(pm[f], g_pk, MAXPK);
                for (i = 0; i < m; i++)
                    if (g_pk[i][0] == 3) { memcpy(lh_pkt[g_pk[i][1]], g_pk[i], 8); lh_ok[g_pk[i][1]] = 1; }
            }
            for (k = 0; k < g_nsub; k++) if (ps[k][f] > last_sub[k]) last_sub[k] = ps[k][f];
        }
        for (k = 0; k < g_nsub; k++) if (last_sub[k] >= 0) sub_headers_upto(k, last_sub[k]);
    } else for (f = 0; f < start; f++) {          /* peds announced before the window */
        m = frame_packets(f, g_pk, MAXPK);
        for (i = 0; i < m; i++)
            if (g_pk[i][0] == 3) { memcpy(lh_pkt[g_pk[i][1]], g_pk[i], 8); lh_ok[g_pk[i][1]] = 1; }
    }
    for (f = start; f < nfr; f++) {
        n = encode_frame(f, game_slot(cur) + used, BUFSZ - 1 - used);
        if (n == -1) {                            /* next slot; END = zero padding */
            memset(game_slot(cur) + used, 0, BUFSZ - used);
            if (cur + 1 >= 8) break;
            cur++; used = 0;
            memset(emitted, 0, sizeof(emitted));
            n = encode_frame(f, game_slot(cur), BUFSZ - 1);
        }
        if (n < 0) { logs("window: frame does not fit / bad data"); break; }
        lw_slot[f] = (u8)cur; lw_off[f] = (u32)used;
        used += n;
    }
    memset(game_slot(cur) + used, 0, BUFSZ - used);
    for (i = 0; i < 8; i++) G8(A_BUFSTATUS + i) = (u8)(i < cur ? 1 : i == cur ? 2 : 0);
    w0 = start; w1 = f;
    if (w1 <= w0) { logs("window: empty"); return 0; }
    return 1;
}

/* make frame f (and f + 1) available and point the playback at f */
static void window_goto(int f)
{
    if (f < 0) f = 0;
    if (f >= nfr) f = nfr - 1;
    if (f < w0 || (f >= w1 - 1 && w1 < nfr)) {
        fill_window(f > 30 ? f - 30 : 0);         /* keep a few frames behind for stepping back */
        if (f >= w1 - 1 && w1 < nfr) fill_window(f);
        if (G8(A_MODE) == 1) ((vfn)F_STREAM_ALL)();
    }
    G8(A_PB_SLOT) = lw_slot[f];
    G32(A_PB_OFF) = lw_off[f];
    GPTR(A_PB_BASE) = game_slot(lw_slot[f]);
    G32(A_INTERP_CNT) = 0;
}

/* window mode: start the playback like PlayReplayFromHD does after loading */
static int start_window(void)
{
    float z = GF(0x6DB5E0);                       /* 0.0 */
    if (!fill_window(0)) return 0;
    ((TrigFn)F_TRIGGER)(0, z, z, z, 0);
    G8(A_PLAYING_HD) = 1;
    G8(A_LOOK_ALLOWED) = 1;
    ((vfn)F_STREAM_ALL)();
    if (G8(A_MODE) != 1) return 0;
    window_goto(0);
    return 1;
}

/* project file (user folder, next to the replays):
   v2: header + Key[nkf]
   v3: header + Key[nkf] + u32 size + the source .rep file itself (size bytes,
       0 = not embedded) -> the project loads even without its .rep
   nfr/t0/t1 identify the recording of the source .rep */
#define PRJ_MAGIC 0x4A525052u                     /* "RPRJ" */
#define PRJ_VERSION 5                             /* 4: + weather / time keys before the replay
                                                     5: key / mark positions in ms from the main start
                                                        + SUB 1 (name, offset, the sub .rep) at the end */
#define REP_MAX (8 + MAXCH * BUFSZ)
#define TMP_REP "ReplayEditor.tmp"                /* embedded replay extracted for the game's loader */
typedef struct { u32 magic, version, nfr, t0, t1; int in, out, nkf; char src[260]; } PrjHdr;

static void cwd_docs(void) { ((vfn)F_SETDIR_DOCS)(); }
static void cwd_game(void) { ((SFn)F_SETDIR)((const char *)A_EMPTY_STR); }

/* g_src / g_srclen (frame table section): the source .rep of the session, embedded on F7 */
static int src_alloc(void)
{
    if (!g_src && pVirtualAlloc) g_src = (u8 *)pVirtualAlloc(0, REP_MAX, 0x3000, 4);
    return g_src != 0;
}

/* cwd must be the user folder; file -> g_src. 0 = failed (g_srclen = 0) */
static int read_src_file(const char *name)
{
    void *fh;
    u32 r = 0, size;
    g_srclen = 0;
    if (!src_alloc() || !pCreateFileA || !pReadFile || !pGetFileSize) return 0;
    fh = pCreateFileA(name, 0x80000000, 1, 0, 3, 0x80, 0);
    if (fh == (void *)-1) return 0;
    size = pGetFileSize(fh, 0);
    if (size <= REP_MAX && pReadFile(fh, g_src, size, &r, 0) && r == size) g_srclen = size;
    pCloseHandle(fh);
    if (g_srclen) memset(g_src + g_srclen, 0, REP_MAX - g_srclen);   /* short last chunk: END */
    return g_srclen != 0;
}

/* cwd must be the user folder; keys may be 0. *rep_size = embedded replay
   size (0 = none); with want_rep it is read into g_src */
static EKey ld_ek[2][MAXEK];               /* env keys of the last read_prj (v4) */
static int ld_nek[2];
static u32 ld_version;

/* buffer for a sub .rep of size bytes (whole chunks, zero padded) */
static int sub_alloc(Sub *S, u32 size)
{
    u32 cap = 8 + (size > 8 ? (size - 8 + BUFSZ - 1) / BUFSZ : 0) * BUFSZ;
    if (S->data && pVirtualFree) pVirtualFree(S->data, 0, 0x8000);
    S->data = 0; S->len = 0;
    if (!pVirtualAlloc || size > REP_MAX) return 0;
    S->data = (u8 *)pVirtualAlloc(0, cap, 0x3000, 4);
    return S->data != 0;
}

static int read_prj(const char *name, PrjHdr *h, Key *keys, u32 *rep_size, int want_rep)
{
    static Key scratch[MAXKF];
    void *fh;
    u32 r = 0, size = 0;
    int ok;
    *rep_size = 0;
    if (!pCreateFileA || !pReadFile || !pCloseHandle) return 0;
    if (!keys) keys = scratch;
    fh = pCreateFileA(name, 0x80000000, 1, 0, 3, 0x80, 0);   /* GENERIC_READ, OPEN_EXISTING */
    if (fh == (void *)-1) return 0;
    ok = pReadFile(fh, h, sizeof(*h), &r, 0) && r == sizeof(*h) && h->magic == PRJ_MAGIC
         && h->version >= 2 && h->version <= 5 && h->nfr > 0 && h->nkf >= 0 && h->nkf <= MAXKF
         && (h->version >= 5 || (h->in >= 0 && h->in <= h->out && h->out < (int)h->nfr));
    if (ok) ld_version = h->version;
    if (ok) {
        h->src[259] = 0;
        ok = h->src[0] != 0;
    }
    if (ok && h->nkf) ok = pReadFile(fh, keys, sizeof(Key) * h->nkf, &r, 0) && r == sizeof(Key) * (u32)h->nkf;
    ld_nek[0] = ld_nek[1] = 0;
    if (ok && h->version >= 4) {
        int tr;
        for (tr = 0; ok && tr < 2; tr++) {
            u32 n = 0;
            ok = pReadFile(fh, &n, 4, &r, 0) && r == 4 && n <= MAXEK;
            if (ok && n) ok = pReadFile(fh, ld_ek[tr], sizeof(EKey) * n, &r, 0) && r == sizeof(EKey) * n;
            if (ok) {
                u32 j;
                ld_nek[tr] = (int)n;
                for (j = 0; j < n; j++) {                  /* never trust file values */
                    short v = ld_ek[tr][j].val, mx = tr == 0 ? NWEATHER : 1440;
                    if (v < 0 || v >= mx) ld_ek[tr][j].val = 0;
                    {   /* interpolation: linear / easing / CUT; anything else -> linear */
                        u8 c = ld_ek[tr][j].cut;
                        ld_ek[tr][j].cut = c == IP_CUT || c == IP_EASEIN || c == IP_EASEOUT || c == IP_EASEINOUT ? c : IP_SMOOTH;
                    }
                    if (h->version < 5 && (ld_ek[tr][j].frame < 0 || ld_ek[tr][j].frame >= (int)h->nfr)) ld_ek[tr][j].frame = 0;
                }
            }
        }
    }
    if (ok && h->version >= 3) {
        ok = pReadFile(fh, &size, 4, &r, 0) && r == 4 && size <= REP_MAX;
        if (ok && size && want_rep) {
            g_srclen = 0;
            ok = src_alloc() && pReadFile(fh, g_src, size, &r, 0) && r == size;
            if (ok) { g_srclen = size; memset(g_src + size, 0, REP_MAX - size); }
        }
        if (ok) *rep_size = size;
        if (ok && size && !want_rep) {                        /* skip it to reach the sub */
            /* listing only: the sub part is not needed */
        }
    }
    if (ok && want_rep) g_nsub = 0;
    if (ok && want_rep && h->version >= 5) {                  /* SUB 1 .. SUB n */
        u32 nsub = 0, ssz = 0, q;
        int off = 0;
        static char nm[260];
        ok = pReadFile(fh, &nsub, 4, &r, 0) && r == 4 && nsub <= MAXSUB;
        for (q = 0; ok && q < nsub; q++) {
            Sub *S = &subs[q];
            ok = pReadFile(fh, nm, 260, &r, 0) && r == 260 && pReadFile(fh, &off, 4, &r, 0) && r == 4
                 && pReadFile(fh, &ssz, 4, &r, 0) && r == 4 && ssz > 8 && ssz <= REP_MAX && sub_alloc(S, ssz);
            if (ok) ok = pReadFile(fh, S->data, ssz, &r, 0) && r == ssz;
            if (ok) {
                nm[259] = 0;
                S->len = ssz;
                scopy(S->name, nm, sizeof(S->name));
                S->off = off;
                g_nsub = (int)q + 1;
            }
        }
        if (!ok) g_nsub = 0;
    }
    pCloseHandle(fh);
    return ok;
}

/* ---- key / mark positions as ms from the main start: they stay on the same
   moment of the main replay when the project range changes (sub added, moved,
   removed). Keys outside the range are kept aside (hkf / hek) and come back. */
static int kt[MAXKF], ekt[2][MAXEK];
static int in_t, out_t, in_first, out_last;
static Key allk[MAXKF];
static int allt[MAXKF];
static EKey alle[MAXEK];

static void keys_to_times(void)
{
    int i, tr;
    for (i = 0; i < nkf; i++) kt[i] = rel_ms(kf[i].frame);
    for (tr = 0; tr < 2; tr++) for (i = 0; i < nek[tr]; i++) ekt[tr][i] = rel_ms(ek[tr][i].frame);
    in_t = rel_ms(ed.in); out_t = rel_ms(ed.out);
    in_first = ed.in == 0; out_last = ed.out == nfr - 1;
}

static int in_range(int T) { return nfr > 0 && T >= rel_ms(0) && T <= rel_ms(nfr - 1); }

/* kt / ekt (+ the hidden ones) -> frames of the current project table */
static void times_to_keys(void)
{
    int i, j, n = 0, tr;
    for (i = 0; i < nkf; i++) { allk[n] = kf[i]; allt[n++] = kt[i]; }
    for (i = 0; i < nhk && n < MAXKF; i++) { allk[n] = hkf[i]; allt[n++] = hkt[i]; }
    for (i = 1; i < n; i++) {                              /* by time */
        Key k = allk[i]; int t = allt[i];
        for (j = i; j > 0 && allt[j - 1] > t; j--) { allk[j] = allk[j - 1]; allt[j] = allt[j - 1]; }
        allk[j] = k; allt[j] = t;
    }
    nkf = nhk = 0;
    for (i = 0; i < n; i++) {
        int f = frame_at_ms(allt[i]);
        if (in_range(allt[i]) && (nkf == 0 || kf[nkf - 1].frame < f)) { kf[nkf] = allk[i]; kf[nkf].frame = f; nkf++; }
        else { hkf[nhk] = allk[i]; hkt[nhk++] = allt[i]; }
    }
    for (tr = 0; tr < 2; tr++) {
        static int et[MAXEK];
        n = 0;
        for (i = 0; i < nek[tr]; i++) { alle[n] = ek[tr][i]; et[n++] = ekt[tr][i]; }
        for (i = 0; i < nhek[tr] && n < MAXEK; i++) { alle[n] = hek[tr][i]; et[n++] = hekt[tr][i]; }
        for (i = 1; i < n; i++) {
            EKey k = alle[i]; int t = et[i];
            for (j = i; j > 0 && et[j - 1] > t; j--) { alle[j] = alle[j - 1]; et[j] = et[j - 1]; }
            alle[j] = k; et[j] = t;
        }
        nek[tr] = nhek[tr] = 0;
        for (i = 0; i < n; i++) {
            int f = frame_at_ms(et[i]);
            if (in_range(et[i]) && (nek[tr] == 0 || ek[tr][nek[tr] - 1].frame < f)) { ek[tr][nek[tr]] = alle[i]; ek[tr][nek[tr]].frame = f; nek[tr]++; }
            else { hek[tr][nhek[tr]] = alle[i]; hekt[tr][nhek[tr]++] = et[i]; }
        }
        if (esel[tr] >= nek[tr]) esel[tr] = nek[tr] - 1;
    }
    if (ed.sel >= nkf) ed.sel = nkf - 1;
    ed.in = in_first ? 0 : frame_at_ms(in_t);
    ed.out = out_last ? nfr - 1 : frame_at_ms(out_t);
    if (ed.in > ed.out) { ed.in = 0; ed.out = nfr - 1; }
}

/* cwd must be the user folder. v5: positions in ms from the main start, all
   keys (also the hidden ones), then SUB 1 after the main replay */
static int write_prj(const char *name)
{
    static PrjHdr h;
    void *fh;
    u32 w1 = 0, w2 = 0, w3 = 0, w4 = 0, we = 0, wsum = 0, wsub = 0, n, esum = 0, nsub = (u32)g_nsub, need = 4;
    int tr, i, nall, ok;
    if (!pCreateFileA || !pWriteFile || !pCloseHandle || nfr <= 0) return 0;
    keys_to_times();
    nall = 0;
    for (i = 0; i < nkf; i++) { allk[nall] = kf[i]; allk[nall++].frame = kt[i]; }
    for (i = 0; i < nhk && nall < MAXKF; i++) { allk[nall] = hkf[i]; allk[nall++].frame = hkt[i]; }
    memset(&h, 0, sizeof(h));
    h.magic = PRJ_MAGIC; h.version = PRJ_VERSION; h.nfr = (u32)mnf; h.t0 = mt[0]; h.t1 = mt[mnf - 1];
    h.in = in_t; h.out = out_t; h.nkf = nall;
    scopy(h.src, ed.src, sizeof(h.src));
    fh = pCreateFileA(name, 0x40000000, 0, 0, 2, 0x80, 0);   /* GENERIC_WRITE, CREATE_ALWAYS */
    if (fh == (void *)-1) return 0;
    pWriteFile(fh, &h, sizeof(h), &w1, 0);
    if (nall) pWriteFile(fh, allk, sizeof(Key) * nall, &w2, 0);
    for (tr = 0; tr < 2; tr++) {                            /* weather keys, time keys */
        int m = 0;
        for (i = 0; i < nek[tr]; i++) { alle[m] = ek[tr][i]; alle[m++].frame = ekt[tr][i]; }
        for (i = 0; i < nhek[tr] && m < MAXEK; i++) { alle[m] = hek[tr][i]; alle[m++].frame = hekt[tr][i]; }
        n = (u32)m; esum += n;
        pWriteFile(fh, &n, 4, &we, 0); wsum += we;
        if (n) { pWriteFile(fh, alle, sizeof(EKey) * n, &we, 0); wsum += we; }
    }
    pWriteFile(fh, &g_srclen, 4, &w3, 0);                   /* the source replay itself */
    if (g_srclen) pWriteFile(fh, g_src, g_srclen, &w4, 0);
    pWriteFile(fh, &nsub, 4, &we, 0); wsub += we;           /* SUB 1 .. SUB n */
    for (i = 0; i < g_nsub; i++) {
        static char nm[260];
        Sub *S = &subs[i];
        memset(nm, 0, sizeof(nm)); scopy(nm, S->name, sizeof(nm));
        pWriteFile(fh, nm, 260, &we, 0); wsub += we;
        pWriteFile(fh, &S->off, 4, &we, 0); wsub += we;
        pWriteFile(fh, &S->len, 4, &we, 0); wsub += we;
        pWriteFile(fh, S->data, S->len, &we, 0); wsub += we;
        need += 268 + S->len;
    }
    pCloseHandle(fh);
    ok = w1 == sizeof(h) && w2 == sizeof(Key) * (u32)nall && w3 == 4 && w4 == g_srclen
         && wsum == 8 + sizeof(EKey) * esum && wsub == need;
    return ok;
}

/* <base>.edit.2026NOV17-153045.prj: local date and time, so every save is a new file */
static void out_name(char *d)
{
    static const char MON[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
    u16 st[8];
    d = sput(d, ed.base);
    d = sput(d, ".edit");
    if (pGetLocalTime) {
        pGetLocalTime(st);
        if (st[1] < 1 || st[1] > 12) st[1] = 1;
        *d++ = '.';
        d = iput(d, st[0], 4);
        *d++ = MON[(st[1] - 1) * 3]; *d++ = MON[(st[1] - 1) * 3 + 1]; *d++ = MON[(st[1] - 1) * 3 + 2];
        d = iput(d, st[3], 2); *d++ = '-';
        d = iput(d, st[4], 2); d = iput(d, st[5], 2); d = iput(d, st[6], 2);
    }
    sput(d, ".prj");
}

/* F7 (as_new 0): save into the project being edited (overwrite);
   F8 (as_new 1), or F7 before the first save: a new <base>.edit.<date-time>.prj,
   which then becomes the project being edited. The source replay is inside.
   Playback of the edit = F6 PLAY of the .prj. rebuild() (baked .rep) is kept
   for a possible export option but is no longer written. */
static void save_edit(int as_new)
{
    char prj[300], line[160], *e;
    int pok;
    if (as_new || !ed.prj[0]) out_name(prj);
    else scopy(prj, ed.prj, sizeof(prj));
    cwd_docs();
    pok = write_prj(prj);
    cwd_game();
    if (!pok) {
        e = sput(line, "SAVE FAILED: COULD NOT WRITE "); dput(e, prj, 60);
        message(line, 0);
        logs("save: project write failed");
        return;
    }
    ed.dirty = 0;
    scopy(ed.prj, prj, sizeof(ed.prj));
    e = sput(line, "SAVED "); e = dput(e, prj, 48); e = sput(e, " - ");
    e = iput(e, nkf, 0); e = sput(e, " KEYFRAMES, ");
    e = iput(e, ed.out - ed.in + 1, 0);
    e = sput(e, " FRAMES ("); e = tput(e, fr_time[ed.out] - fr_time[ed.in]); sput(e, ")");
    message(line, 2500);
    logs(line);
}

static void set_trim(int which, int f)
{
    char line[80], *e;
    if (which == 0) { ed.in = f; if (ed.in > ed.out) ed.out = nfr - 1; }
    else { ed.out = f; if (ed.out < ed.in) ed.in = 0; }
    ed.dirty = 1;
    e = sput(line, which == 0 ? "START" : "END");
    e = sput(e, " - FRAME "); e = iput(e, f + 1, 0);
    e = sput(e, " ("); e = tput(e, fr_time[f] - fr_time[0]);
    e = sput(e, ")   LENGTH "); tput(e, fr_time[ed.out] - fr_time[ed.in]);
    message(line, 1000);
}

static void start_playing(int f)
{
    ed.limit = f < ed.out ? ed.out : nfr - 1;     /* stop at the end mark when starting before it */
    ed.paused = 0;
}

static u8 *game_pad(void) { return (u8 *)((GetPadFn)F_GETPAD)(0); }

/* Keys pressed during a replay must not reach the game afterwards.
   Key events go into the pad's PCTemp{Key,Joy,Mouse}State (pad + 0x6C/0x96/0xC0,
   0x2A bytes each), which only CPad::Update (0x4AB0C0) turns into button
   presses and clears - and CPad::UpdatePads skips that during playback
   (0xA10AF3), so presses pile up and fire on the first frame after the replay
   (C = crouch, ...). Cleared every frame of our playbacks and when they end. */
static void swallow_keys(void);
static void clear_pad_input(void)
{
    u8 *pad = game_pad();
    if (pad) {
        memset(pad, 0, 0x54);              /* NewState, OldState */
        memset(pad + 0x6C, 0, 0x7E);       /* PCTempKeyState, PCTempJoyState, PCTempMouseState */
    }
}

static int can_enter(void)
{
    u8 *pad;
    if (G8(A_MODE) != 0) return 0;
    if (G8(0xA10B63) || G8(0x869668) || G8(0xA10B76) || G8(0xA10B36) || G8(0xA10B16)) return 0;
    if (!G8(0x6DB5D8)) return 0;
    pad = game_pad();
    if (pad && *(short *)(pad + 0xF0)) return 0;
    if (((BoolFn)F_CUTSCENE)()) return 0;
    return 1;
}

/* game playback of <name> in the user folder: PlayReplayFromHD with its
   'push "replay.rep"' operand pointed at our name for the duration of the call */
static char g_loadname[260];
static int set_load_operand(u32 v)
{
    u32 old;
    if (!pVirtualProtect((void *)A_LOAD_NAME_OPND, 4, 0x40, &old)) return 0;
    G32(A_LOAD_NAME_OPND) = v;
    pVirtualProtect((void *)A_LOAD_NAME_OPND, 4, old, &old);
    return 1;
}
static void error_name(const char *what, const char *name)
{
    char line[160], *e = sput(line, what);
    dput(e, name, 60);
    message(line, 0);
    logs(line);
}

/* the game's loader does not validate anything: check the file first.
   cwd must be the user folder. 0 = ok, else the reason */
static const char *check_rep(const char *name)
{
    static const u8 HDR[8] = { 'g', 't', 'a', 'V', 'C', '7', 'f', 0 };
    u8 h[8];
    u32 r = 0, size;
    void *fh;
    int i, ok;
    if (!pCreateFileA || !pReadFile || !pCloseHandle) return 0;
    fh = pCreateFileA(name, 0x80000000, 1, 0, 3, 0x80, 0);
    if (fh == (void *)-1) return "FILE NOT FOUND: ";
    ok = pReadFile(fh, h, 8, &r, 0) && r == 8;
    for (i = 0; ok && i < 8; i++) if (h[i] != HDR[i]) ok = 0;
    size = pGetFileSize ? pGetFileSize(fh, 0) : 0;
    pCloseHandle(fh);
    if (!ok) return "NOT A VICE CITY REPLAY FILE: ";
    if (pGetFileSize && (size < 8 + 64 || size > 8 + 8 * BUFSZ)) return "REPLAY FILE HAS A BAD SIZE: ";
    return 0;
}

/* game playback of name (user folder); shows the error itself, 1 = playing */
static int play_file(const char *name)
{
    const char *bad;
    cwd_docs();
    bad = check_rep(name);
    cwd_game();
    if (bad) { error_name(bad, name); return 0; }
    scopy(g_loadname, name, sizeof(g_loadname));
    if (!set_load_operand((u32)g_loadname)) { error_name("COULD NOT LOAD REPLAY ", name); return 0; }
    ((vfn)F_PLAYFROMHD)();
    set_load_operand(A_REPLAY_REP);
    if (G8(A_MODE) != 1) { error_name("COULD NOT LOAD REPLAY ", name); return 0; }
    return 1;
}

/* load src and prepare the editor state (not active yet); 0 = failed */
/* source missing in memory (could not be read): take the slots the game loaded */
static void src_from_game(void)
{
    int i, n = 0;
    if (!src_alloc()) return;
    for (i = 0; i < 8; i++) if (G8(A_BUFSTATUS + i)) n = i + 1;
    memcpy(g_src, (const void *)A_REP_HEADER, 8);
    memcpy(g_src + 8, (const void *)A_BUFFERS, n * BUFSZ);
    g_srclen = 8 + n * BUFSZ;
}

static void looks_load(void);
static void looks_unload(void);
static void dff_unregister(void);
/* load = file for the game's loader (NULL: g_src only); g_src = the source.
   Files bigger than 8 buffers are played through the window. */
static int start_session(const char *load, const char *src)
{
    static const u8 HDR[8] = { 'g', 't', 'a', 'V', 'C', '7', 'f', 0 };
    char line[96], *e;
    int n;
    g_win = 0;
    if (load && g_srclen <= DIRECT_MAX && !g_nsub) {
        if (!play_file(load)) return 0;
        if (!g_srclen) src_from_game();
    } else {
        for (n = 0; n < 8 && g_srclen > 8 && g_src[n] == HDR[n]; n++) {}
        if (n < 8) { error_name("NOT A VICE CITY REPLAY FILE: ", src); return 0; }
        g_win = 1;
    }
    if (!build_table()) {
        if (G8(A_MODE) == 1) { ((vfn)F_FINISH)(); dff_unregister(); }
        g_win = 0;
        error_name("REPLAY DATA NOT RECOGNISED: ", src);
        return 0;
    }
    mnf = nfr;
    memcpy(mt, fr_time, sizeof(u32) * mnf);
    if (g_nsub) {
        int k, ok = 1;
        for (k = 0; k < g_nsub && ok; k++) if (!sub_build(&subs[k])) { ok = 0; error_name("SUB REPLAY NOT USABLE (SUBS DROPPED): ", subs[k].name); }
        if (ok && !build_remap()) { ok = 0; error_name("TOO MANY VEHICLES / PEDS IN THE SUB REPLAYS (DROPPED): ", subs[0].name); }
        if (!ok) g_nsub = 0;
    }
    if (g_nsub) looks_load(); else looks_unload();
    if (!build_project()) { g_nsub = 0; build_project(); }
    if (g_win && !start_window()) {
        if (G8(A_MODE) == 1) { ((vfn)F_FINISH)(); dff_unregister(); }
        g_win = 0;
        error_name("COULD NOT START THE REPLAY ", src);
        return 0;
    }
    scopy(ed.src, src, sizeof(ed.src));
    scopy(ed.base, src, sizeof(ed.base));
    ed.prj[0] = 0;
    n = slen(ed.base);
    if (ends_with(ed.base, ".rep")) ed.base[n - 4] = 0;
    ed.paused = 1;
    ed.need_step = 1;
    ed.freecam = 0;
    ed.sel = -1;
    ed.t = 0;
    ed.fov_touched = 0;
    ed.in = 0;
    ed.out = nfr - 1;
    ed.limit = nfr - 1;
    ed.viewer = 0;
    ed.dirty = 0;
    ed.exit_until = 0;
    nkf = 0;
    nek[0] = nek[1] = 0;
    esel[0] = esel[1] = -1;
    ed.track = TR_CAM;
    nhk = nhek[0] = nhek[1] = 0;
    set_speed(2);
    ed.last_ms = now_ms();
    e = sput(line, "open: frames "); e = iput(e, nfr, 0); e = sput(e, " duration ms ");
    iput(e, (int)(fr_time[nfr - 1] - fr_time[0]), 0);
    logs(line);
    return 1;
}

static int is_long(const char *name)
{
    void *fh;
    u32 size = 0;
    if (!pCreateFileA || !pGetFileSize) return 0;
    cwd_docs();
    fh = pCreateFileA(name, 0x80000000, 1, 0, 3, 0x80, 0);
    if (fh != (void *)-1) { size = pGetFileSize(fh, 0); pCloseHandle(fh); }
    cwd_game();
    return size > DIRECT_MAX;
}

static void open_rep_in_editor(const char *name)
{
    char line[160], *e;
    g_nsub = 0;
    cwd_docs();
    read_src_file(name);                       /* kept for embedding into the project */
    cwd_game();
    if (!g_srclen && is_long(name)) { error_name("COULD NOT READ ", name); return; }
    if (!start_session(name, name)) return;
    ed.active = 1;
    e = sput(line, "NEW PROJECT - "); dput(e, name, 60);
    message(line, 1500);
}

/* viewer = 1: play the project (trim + camera) instead of editing it */
static void open_project(const char *name, int viewer)
{
    static PrjHdr h;
    static Key keys[MAXKF];
    char line[160], *e;
    u32 rsz, w = 0;
    int ok;
    cwd_docs();
    g_nsub = 0;
    ok = read_prj(name, &h, keys, &rsz, 1);
    if (ok && rsz && (g_srclen > DIRECT_MAX || g_nsub)) {   /* long replay / sub inside: played from memory */
    } else if (ok && rsz) {                    /* replay inside: extract it for the game's loader */
        void *fh = pCreateFileA(TMP_REP, 0x40000000, 0, 0, 2, 0x80, 0);
        if (fh != (void *)-1) { pWriteFile(fh, g_src, g_srclen, &w, 0); pCloseHandle(fh); }
        if (w != g_srclen) {
            if (pDeleteFileA) pDeleteFileA(TMP_REP);
            cwd_game();
            error_name("COULD NOT EXTRACT THE REPLAY OF ", name);
            return;
        }
    } else if (ok) read_src_file(h.src);       /* old project: its .rep must exist */
    cwd_game();
    if (!ok) { error_name("NOT A VALID PROJECT FILE: ", name); return; }
    {
        int from_mem = rsz && (g_srclen > DIRECT_MAX || g_nsub);
        ok = start_session(rsz ? (from_mem ? 0 : TMP_REP) : h.src, h.src);
        if (rsz && !from_mem && pDeleteFileA) { cwd_docs(); pDeleteFileA(TMP_REP); cwd_game(); }
    }
    if (!ok) return;
    if (h.nfr != (u32)mnf || h.t0 != mt[0] || h.t1 != mt[mnf - 1]) {
        { ((vfn)F_FINISH)(); dff_unregister(); }
        error_name("PROJECT DOES NOT MATCH ITS REPLAY (RE-RECORDED?): ", h.src);
        return;
    }
    memcpy(kf, keys, sizeof(Key) * h.nkf);
    { int i; for (i = 0; i < (int)h.nkf; i++) {
        if (kf[i].type >= KF_NTYPES) kf[i].type = KF_FREE;                     /* unknown type */
        if (kf[i].cut >= IP_N) kf[i].cut = IP_SMOOTH;                          /* unknown interpolation */
    } }
    memcpy(ek, ld_ek, sizeof(ek));
    nek[0] = ld_nek[0]; nek[1] = ld_nek[1];
    nkf = h.nkf;
    if (ld_version >= 5) {                     /* positions are ms from the main start */
        int i, tr;
        for (i = 0; i < nkf; i++) kt[i] = kf[i].frame;
        for (tr = 0; tr < 2; tr++) for (i = 0; i < nek[tr]; i++) ekt[tr][i] = ek[tr][i].frame;
        in_t = h.in; out_t = h.out; in_first = out_last = 0;
        times_to_keys();
    } else { ed.in = h.in; ed.out = h.out; }
    scopy(ed.prj, name, sizeof(ed.prj));       /* F7 saves back into this file */
    ed.active = 1;
    if (viewer) {
        ed.viewer = 1;
        ed.need_step = 0;
        set_speed(2);
        seek(ed.in);
        start_playing(ed.in);
        return;
    }
    if (ed.in > 0) {                           /* trimmed start: open on the start mark */
        ed.need_step = 0;
        seek(ed.in);
    }
    e = sput(line, "PROJECT LOADED - "); e = iput(e, nkf, 0); e = sput(e, " KEYFRAMES - "); dput(e, h.src, 50);
    message(line, 1500);
}

/* PLAY of a .rep bigger than 8 buffers: the window player, no edits */
static void play_long(const char *name)
{
    g_nsub = 0;
    cwd_docs();
    read_src_file(name);
    cwd_game();
    if (!g_srclen) { error_name("COULD NOT READ ", name); return; }
    if (!start_session(0, name)) return;
    ed.active = 1;
    ed.viewer = 1;
    ed.need_step = 0;
    set_speed(2);
    seek(0);
    start_playing(0);
}

static void exit_editor(void)
{
    g_win = 0;
    g_nsub = 0;
    looks_unload();
    ed.active = 0;
    ed.viewer = 0;
    G32(A_DIVISOR) = 1;
    G32(A_INTERP_CNT) = 0;
    GF(A_FOV) = REPLAY_FOV;
    if (G8(A_MODE) == 1) ((vfn)F_FINISH)();
    dff_unregister();                              /* no entity uses the model copies any more */
    clear_pad_input();
    swallow_keys();
    logs("exit");
}

/* ------------------------------------------------ SUB 1 looks (appearance)
   Files in <game>\CLEO\ReplayEditor\Looks\SUB1\ apply to the sub replay's
   entities, read whenever a project with a sub is edited / played:
   - player.bmp: the player model's atomics are drawn by 0x581AE0, which sets
     every material to Players[0].m_pSkinTexture (0x94AE94) first. Its first
     instruction (mov eax,[0x94AE94], 5 bytes) is patched into a call that
     returns the sub skin when the atomic belongs to a sub ped (clump = ped +0x4C).
   - <model>.txd: loaded like CTxdStore::LoadTxd 0x580CD0 does. The render
     callback (atomic +0x48) of every atomic of the sub's vehicles / peds is
     wrapped: textures with the same name are swapped in for the draw and put
     back after it (parts drawn later from the alpha list keep the originals).
   - carcols.dat: the 'col' section goes into free entries of the 256-colour
     palette 0x939228 after the game's own; sub vehicle colours point there. */
#define LOOKS_BASE       "CLEO\\ReplayEditor\\Looks\\SUB"   /* + n + "\\" */
#define LOOKS_DIAG       0                 /* 1: log swap counters every 3 s */
#define A_SKIN_TEX       0x94AE94
#define SITE_SKIN_CB     0x581AE0
#define A_CARCOLS        0x939228
#define A_PEDPOOL        0x97F2AC
#define A_VEHPOOL        0xA0FDE4
#define F_TXD_MODE       0x63EFD0
#define F_STREAM_OPEN    0x6459C0
#define F_STREAM_FIND    0x64FAC0
#define F_TXD_READ       0x61E710
#define F_STREAM_CLOSE   0x6458F0
#define F_IMAGE_READ     0x657870
#define F_IMAGE_RFMT     0x6602E0
#define F_RASTER_CREATE  0x655490
#define F_RASTER_SETIMG  0x6602B0
#define F_TEX_CREATE     0x64DE60
#define F_TEX_DESTROY    0x64DEC0
#define F_IMAGE_DESTROY  0x6512B0
#define F_CLUMP_ATOMICS  0x640D00
#define F_GEOM_MATERIALS 0x64CC90
#define F_ATOMIC_DEFAULT 0x6405A0
typedef u8 *(__cdecl *AtomCB)(u8 *);
typedef u8 *(__cdecl *ForAllFn)(u8 *, void *, void *);
typedef void (__cdecl *I1cFn)(int);
typedef u8 *(__cdecl *StreamOpenFn)(int, int, const char *);
typedef int (__cdecl *StreamFindFn)(u8 *, int, void *, void *);
typedef u8 *(__cdecl *RwP1Fn)(u8 *);
typedef int (__cdecl *StreamCloseFn)(u8 *, void *);
typedef u8 *(__cdecl *ImageReadFn)(const char *);
typedef int (__cdecl *RFmtFn)(u8 *, int, int *, int *, int *, int *);
typedef u8 *(__cdecl *RasterCreateFn)(int, int, int, int);
typedef u8 *(__cdecl *SetImgFn)(u8 *, u8 *);

typedef struct { char name[64]; u32 size, t0, t1; u8 *txd; } LookTxd;
static LookTxd lk_txd[64];                 /* loaded .txd files "SUBn\file" (kept, reloaded when changed) */
static int nlk_txd;
static u8 pal_backup[256 * 4];
static int pal_game = -1;                  /* colours of the game (backup taken), -1 = none */
static int looks_any;                      /* some sub has .txd looks */

/* "CLEO\ReplayEditor\Looks\SUBn\" + file */
static char *looks_path(char *d, int k, const char *file)   /* returns d */
{
    char *e = sput(d, LOOKS_BASE);
    *e++ = (char)('1' + k); *e++ = '\\';
    sput(e, file);
    return d;
}

static u8 *ent_at(u32 pool_ptr_addr, int idx, int size)
{
    u32 *pool = *(u32 **)pool_ptr_addr;
    if (!pool || idx < 0 || idx >= (int)pool[2]) return 0;
    if (((u8 *)pool[1])[idx] & 0x80) return 0;
    return (u8 *)pool[0] + idx * size;
}

static u32 __cdecl pick_skin(u8 *atomic)
{
    u32 main_skin = G32(A_SKIN_TEX);
    int i, k;
    if (!g_nsub || !ed.active || !atomic) return main_skin;
    for (k = 0; k < g_nsub; k++) {
        Sub *S = &subs[k];
        if (!S->skin_on) continue;
        for (i = 0; i < S->npslot; i++) {
            u8 *ped = ent_at(A_PEDPOOL, S->pslot[i], 0x6D8);
            if (ped && *(u32 *)(ped + 0x4C) == *(u32 *)(atomic + 0x3C)) return (u32)S->skin;
        }
    }
    return main_skin;
}

/* replaces "mov eax, [A_SKIN_TEX]" at the start of the callback: [esp+8] = atomic */
__declspec(naked) static void skin_stub(void)
{
    __asm {
        push dword ptr [esp + 8]
        call pick_skin
        add esp, 4
        ret
    }
}

static void install_skin_patch(void)
{
    static const u8 ORIG[5] = { 0xA1, 0x94, 0xAE, 0x94, 0x00 };
    u8 *p = (u8 *)SITE_SKIN_CB;
    u32 old;
    int i;
    for (i = 0; i < 5; i++) if (p[i] != ORIG[i]) { logs("skin patch: unexpected bytes, skipped"); return; }
    if (!pVirtualProtect(p, 5, 0x40, &old)) return;
    p[0] = 0xE8;
    *(int *)(p + 1) = (int)((u32)skin_stub - (SITE_SKIN_CB + 5));
    pVirtualProtect(p, 5, old, &old);
    logs("skin patch installed");
}

/* ---- atomic render callback wrapper (sub entities with .txd looks) */
#define WRAPN 2048
static u32 wr_atom[WRAPN], wr_cb[WRAPN];
static u8 wr_sub[WRAPN];
static int wr_count;
static u32 *wr_slot(u32 a)
{
    u32 h = (a >> 4) & (WRAPN - 1);
    int n;
    for (n = 0; n < WRAPN; n++, h = (h + 1) & (WRAPN - 1))
        if (wr_atom[h] == a || wr_atom[h] == 0) return &wr_atom[h];
    return 0;
}

static u8 *sw_mat[96], *sw_tex[96];
static int sw_n;
static u32 dg_ent, dg_clump, dg_wrapped, dg_render, dg_mats, dg_swaps, dg_last, dg_defer;   /* diagnostics (log) */
static u8 *sub_clumps[256];                /* clumps of the sub entities this frame */
static u8 sub_clump_k[256];                /* ... and their sub */
static int n_sub_clumps;
static char dg_names[200];
static u8 *__cdecl swap_cb(u8 *mat, void *data)
{
    u8 *tex = *(u8 **)mat;
    Sub *S = &subs[(int)data];
    int i;
    if (!tex || sw_n >= 96) return mat;
    dg_mats++;
    if (slen(dg_names) < 150) { char *e = dg_names + slen(dg_names); e = dput(e, (const char *)tex + 0x10, 24); sput(e, " "); }   /* <= 178 of 200 */
    for (i = 0; i < S->ntex; i++) {
        const char *a = (const char *)tex + 0x10, *b = (const char *)S->tex[i] + 0x10;
        int k = 0;
        while (k < 32 && a[k] && (a[k] | 0x20) == (b[k] | 0x20)) k++;
        if (k == 32 || (a[k] == 0 && b[k] == 0)) {
            if (S->tex[i] != tex) { sw_mat[sw_n] = mat; sw_tex[sw_n++] = tex; *(u8 **)mat = S->tex[i]; dg_swaps++; }
            break;
        }
    }
    return mat;
}

static u8 *__cdecl looks_render(u8 *atomic)
{
    u32 *s = wr_slot((u32)atomic), cb = F_ATOMIC_DEFAULT;
    u8 *geo = *(u8 **)(atomic + 0x18), *r;
    int i, n0 = sw_n, k = -1;
    if (s && *s == (u32)atomic && wr_cb[s - wr_atom]) { cb = wr_cb[s - wr_atom]; k = wr_sub[s - wr_atom]; }
    dg_render++;
    if (geo && k >= 0 && k < g_nsub && subs[k].ntex) ((ForAllFn)F_GEOM_MATERIALS)(geo, (void *)swap_cb, (void *)k);
    r = ((AtomCB)cb)(atomic);
    for (i = sw_n - 1; i >= n0; i--) *(u8 **)sw_mat[i] = sw_tex[i];
    sw_n = n0;
    return r;
}

static u8 *__cdecl wrap_cb(u8 *atomic, void *data)
{
    u32 cb = *(u32 *)(atomic + 0x48), *s;
    if (cb == (u32)looks_render || cb == SITE_SKIN_CB) return atomic;   /* the player skin has its own way */
    s = wr_slot((u32)atomic);
    if (!s) return atomic;
    dg_wrapped++;
    if (*s == 0) wr_count++;
    *s = (u32)atomic;
    wr_cb[s - wr_atom] = cb;
    wr_sub[s - wr_atom] = (u8)(int)data;
    *(u32 *)(atomic + 0x48) = (u32)looks_render;
    return atomic;
}
static u8 *__cdecl unwrap_cb(u8 *atomic, void *data)
{
    u32 *s;
    (void)data;
    if (*(u32 *)(atomic + 0x48) != (u32)looks_render) return atomic;
    s = wr_slot((u32)atomic);
    *(u32 *)(atomic + 0x48) = s && *s == (u32)atomic && wr_cb[s - wr_atom] ? wr_cb[s - wr_atom] : F_ATOMIC_DEFAULT;
    return atomic;
}

static void for_sub_clumps(void *cb)
{
    int i, k;
    for (k = 0; k < g_nsub; k++) {
        Sub *S = &subs[k];
        if (!S->ntex) continue;
        for (i = 0; i < S->nvslot + S->npslot; i++) {
            u8 *e = i < S->nvslot ? ent_at(A_VEHPOOL, S->vslot[i], 0x5DC) : ent_at(A_PEDPOOL, S->pslot[i - S->nvslot], 0x6D8);
            u8 *clump = e ? *(u8 **)(e + 0x4C) : 0;
            if (e) dg_ent++;
            if (clump && clump[0] == 2) {                   /* rpCLUMP */
                dg_clump++;
                if (cb == (void *)wrap_cb && n_sub_clumps < 256) { sub_clump_k[n_sub_clumps] = (u8)k; sub_clumps[n_sub_clumps++] = clump; }
                ((ForAllFn)F_CLUMP_ATOMICS)(clump, cb, (void *)k);
            }
        }
    }
}

/* every frame after the playback: wrap atomics of sub entities (re)created by it */
static void looks_wrap(void)
{
    if (!ed.active || !g_nsub || !looks_any || G8(A_MODE) != 1) return;
    if (wr_count > WRAPN * 3 / 4) {                /* stale entries of deleted entities: start over */
        for_sub_clumps((void *)unwrap_cb);
        memset(wr_atom, 0, sizeof(wr_atom)); wr_count = 0;
    }
    n_sub_clumps = 0;
    for_sub_clumps((void *)wrap_cb);
    if (LOOKS_DIAG && (int)(now_ms() - dg_last) > 3000) {   /* diagnostics every 3 s (debug) */
        static char line[1200];                              /* static: never on the stack (was overflowing) */
        char *e = sput(line, "looks diag: sub slots v");
        int i;
        e = iput(e, subs[0].nvslot, 0); e = sput(e, " p"); e = iput(e, subs[0].npslot, 0);
        e = sput(e, " entities "); e = iput(e, (int)dg_ent, 0); e = sput(e, " clumps "); e = iput(e, (int)dg_clump, 0);
        e = sput(e, " wrapped "); e = iput(e, (int)dg_wrapped, 0); e = sput(e, " renders "); e = iput(e, (int)dg_render, 0);
        e = sput(e, " materials "); e = iput(e, (int)dg_mats, 0); e = sput(e, " swaps "); e = iput(e, (int)dg_swaps, 0);
        e = sput(e, " deferred "); e = iput(e, (int)dg_defer, 0);
        e = sput(e, " | drawn: "); e = sput(e, dg_names); e = sput(e, "| txd: ");
        for (i = 0; i < subs[0].ntex && i < 8; i++) { e = dput(e, (const char *)subs[0].tex[i] + 0x10, 24); e = sput(e, " "); }
        if (subs[0].nvslot) {
            u8 *v = ent_at(A_VEHPOOL, subs[0].vslot[0], 0x5DC);
            e = sput(e, "| veh0 "); e = hput(e, (u32)v);
            if (v) { u8 *c = *(u8 **)(v + 0x4C); e = sput(e, " obj "); e = hput(e, (u32)c); if (c) { e = sput(e, " type "); e = iput(e, c[0], 0); } }
        }
        logs(line);
        dg_last = now_ms(); dg_ent = dg_clump = dg_wrapped = dg_render = dg_mats = dg_swaps = dg_defer = 0; dg_names[0] = 0;
    }
}

/* atomics queued by the render callbacks (e.g. alpha parts of vehicles) are drawn
   later by CVisibilityPlugins loops 0x581310 (list 0x97F2F8) and 0x581410 (list
   0x983B3C) with AtomicDefaultRenderCallBack: those two calls come here */
#define SITE_DEFER1 0x581338
#define SITE_DEFER2 0x581423
static u8 *__cdecl deferred_render(u8 *atomic)
{
    u8 *cl = atomic ? *(u8 **)(atomic + 0x3C) : 0, *geo, *r;
    int i, n0;
    if (cl && looks_any && g_nsub && ed.active)
        for (i = 0; i < n_sub_clumps; i++)
            if (sub_clumps[i] == cl && sub_clump_k[i] < g_nsub) {
                geo = *(u8 **)(atomic + 0x18);
                n0 = sw_n;
                dg_defer++;
                if (geo) ((ForAllFn)F_GEOM_MATERIALS)(geo, (void *)swap_cb, (void *)(int)sub_clump_k[i]);
                r = ((AtomCB)F_ATOMIC_DEFAULT)(atomic);
                for (i = sw_n - 1; i >= n0; i--) *(u8 **)sw_mat[i] = sw_tex[i];
                sw_n = n0;
                return r;
            }
    return ((AtomCB)F_ATOMIC_DEFAULT)(atomic);
}

/* ---- <model>.dff: a copy of the model info in a free model number, its clump
   read from Looks\SUB1 (textures from the model's own txd) and set up by the
   game's SetClump (vtable +0x24), as CStreaming::ConvertBufferToObject 0x40C1E0
   / CFileLoader::LoadClumpFile 0x48D5E0 do. Model info: name +4, type +0x19,
   refs +0x24, txd slot +0x26, clump +0x28; pointers 0x92D4C8[6500];
   streaming state 0x94DDD8 + id * 0x14 (1 = loaded). */
#define A_MODELINFO      0x92D4C8
#define A_STREAMINFO     0x94DDD8
#define NUM_MODELS       6500
#define F_REQUEST_MODEL  0x40E310
#define F_LOAD_REQUESTED 0x40B5F0
#define F_TXD_PUSH       0x580AC0
#define F_TXD_POP        0x580AA0
#define F_TXD_SETCUR     0x580AD0
#define F_TXD_ADDREF     0x580A60
#define F_CLUMP_READ     0x641A10
#define F_SETATOMCB_CB   0x5412A0      /* RpClumpForAllAtomics callback: set render callback */
typedef void (__cdecl *VFnc)(void);
typedef void (__cdecl *I2cFn)(int, int);
typedef struct { char name[64]; u32 size, t0, t1; int orig, clone, ready, sub; } LookDff;
#define MAXDFF 32
static LookDff lk_dff[MAXDFF];
static int nlk_dff;
static u8 clone_mem[MAXDFF][0x400];

static int name_eq(const char *a, const char *b, int n)     /* case-insensitive, b may end with '.' */
{
    int k;
    for (k = 0; k < n; k++) {
        char x = a[k], y = b[k];
        if (y == '.') y = 0;
        if ((x | 0x20) != (y | 0x20) && !(x == 0 && y == 0)) return 0;
        if (!x) return 1;
    }
    return 1;
}

static int find_model(const char *stem)
{
    u8 **mi = (u8 **)A_MODELINFO;
    int id;
    for (id = 0; id < NUM_MODELS; id++) if (mi[id] && name_eq((const char *)mi[id] + 4, stem, 24)) return id;
    return -1;
}

static int free_model_id(void)
{
    u8 **mi = (u8 **)A_MODELINFO;
    int id;
    int i;
    for (id = NUM_MODELS - 1; id > 0x200; id--) {
        if (mi[id] || G8(A_STREAMINFO + id * 0x14) || G32(A_STREAMINFO + 8 + id * 0x14)) continue;
        for (i = 0; i < nlk_dff && lk_dff[i].clone != id; i++) {}
        if (i == nlk_dff) return id;
    }
    return -1;
}

static void mi_setclump(u8 *info, u8 *clump)
{
    u32 fn = (*(u32 **)info)[9];
    __asm {
        mov ecx, info
        push clump
        call fn
    }
}

/* the copies leave the model list once no entity can use them (playback over):
   the game's own clean-up (e.g. when it quits) must never delete them, they
   share data with the original model info */
#define F_CLUMP_DESTROY 0x640900
/* RenderWare destroys every clump and atomic still allocated when its clump
   module closes at game exit (0x640860: FreeListForAllUsed with RpClumpDestroy /
   RpAtomicDestroy), after MatFX etc. are gone -> the copies' RW objects must be
   destroyed here. Vehicles: the model's own DeleteRwObject (vtable +8; parts +
   clump). Peds: only the clump (CPedModelInfo::DeleteRwObject would also free the
   hit col model the copy shares with the original). */
static void dff_unregister(void)
{
    u8 **mi = (u8 **)A_MODELINFO;
    int i;
    for (i = 0; i < nlk_dff; i++) {
        u8 *info = clone_mem[i];
        if (lk_dff[i].clone <= 0) continue;
        if (lk_dff[i].ready && *(u8 **)(info + 0x28)) {
            if (info[0x19] == 6) {
                u32 fn = (*(u32 **)info)[2];
                __asm {
                    mov ecx, info
                    call fn
                }
            } else {
                ((RwP1Fn)F_CLUMP_DESTROY)(*(u8 **)(info + 0x28));
            }
            *(u8 **)(info + 0x28) = 0;
        }
        lk_dff[i].ready = 0;
        if (mi[lk_dff[i].clone] == info) mi[lk_dff[i].clone] = 0;
        G8(A_STREAMINFO + lk_dff[i].clone * 0x14) = 0;
    }
}

static u8 *read_clump(const char *path, int txdslot)
{
    u8 *st, *clump = 0;
    ((VFnc)F_TXD_PUSH)();
    ((I1cFn)F_TXD_SETCUR)(txdslot);
    st = ((StreamOpenFn)F_STREAM_OPEN)(2, 1, path);
    if (st) {
        if (((StreamFindFn)F_STREAM_FIND)(st, 0x10, 0, 0)) clump = ((RwP1Fn)F_CLUMP_READ)(st);   /* rwID_CLUMP */
        ((StreamCloseFn)F_STREAM_CLOSE)(st, 0);
    }
    ((VFnc)F_TXD_POP)();
    return clump;
}

/* one Looks\SUBn\<model>.dff (sub k); returns the copy's index in lk_dff, -1 = none */
static int load_dff(int k, const char *fname, u32 sz, u32 t0, u32 t1)
{
    static char path[300];
    char line[160];
    LookDff *L = 0;
    u8 **mi = (u8 **)A_MODELINFO, *clump;
    int i, orig, type;
    for (i = 0; i < nlk_dff; i++) if (lk_dff[i].sub == k && name_eq(lk_dff[i].name, fname, 64)) { L = &lk_dff[i]; break; }
    if (L && L->clone > 0 && L->ready && G8(A_STREAMINFO + L->clone * 0x14) == 1) return (int)(L - lk_dff);   /* already set up */
    orig = find_model(fname);
    if (orig < 0) { sput(sput(line, "looks: no model named like "), fname); logs(line); return -1; }
    type = mi[orig][0x19];
    if (type != 6 && type != 7) { sput(sput(line, "looks: not a vehicle / ped model: "), fname); logs(line); return -1; }
    if (!L) {
        if (nlk_dff >= MAXDFF) return -1;
        L = &lk_dff[nlk_dff++];
        scopy(L->name, fname, sizeof(L->name));
        L->sub = k;
        L->clone = free_model_id();
        if (L->clone < 0) { logs("looks: no free model number"); nlk_dff--; return -1; }
        L->ready = 0;
    }
    L->orig = orig; L->size = sz; L->t0 = t0; L->t1 = t1;
    if (G8(A_STREAMINFO + orig * 0x14) != 1) {                       /* model + txd needed for the textures */
        ((I2cFn)F_REQUEST_MODEL)(orig, 0);
        ((I1cFn)F_LOAD_REQUESTED)(0);
    }
    if (G8(A_STREAMINFO + orig * 0x14) != 1) { sput(sput(line, "looks: model not loaded: "), fname); logs(line); return -1; }
    L->ready = 0;
    looks_path(path, k, fname);
    clump = read_clump(path, *(short *)(mi[orig] + 0x26));
    if (!clump) { sput(sput(line, "looks: could not read "), fname); logs(line); return -1; }
    {
        u8 *info = clone_mem[L - lk_dff];
        memcpy(info, mi[orig], sizeof(clone_mem[0]));
        *(u32 *)(info + 0x28) = 0;                                   /* no clump yet */
        *(short *)(info + 0x24) = 1000;                              /* never "unused" */
        mi[L->clone] = info;
        mi_setclump(info, clump);
        if (orig == 0)                                               /* player: the skin callback (sub skin) */
            ((ForAllFn)F_CLUMP_ATOMICS)(clump, (void *)F_SETATOMCB_CB, (void *)SITE_SKIN_CB);
        G8(A_STREAMINFO + L->clone * 0x14) = 1;
        L->ready = 1;
    }
    {
        char *e = sput(line, "looks SUB"); *e++ = (char)('1' + k); e = sput(e, ": "); e = sput(e, fname);
        e = sput(e, " -> model "); e = iput(e, L->clone, 0); e = sput(e, " (copy of "); e = iput(e, orig, 0); sput(e, ")");
        logs(line);
    }
    return (int)(L - lk_dff);
}

/* ---- particle.cfg: read by the game's loader 0x565B00 (thiscall, table) into a
   copy of the 83-entry table 0x975428 (0x94 each); the file name pointer 0x698780
   is pointed at Looks\SUB1 for the call (relative to DATA). Sub particle packets
   carry type | 0x80; their creation (0x563CC0, called at 0x621CD0) uses the sub
   values of the fields particle.cfg sets. Particles already alive are drawn with
   the game's table, so only what is decided at creation can differ. */
#define A_PARTSYS       0x975428
#define N_PARTSYS       83
#define PS_SIZE         0x94
#define F_PART_LOAD     0x565B00
#define A_PART_CFGNAME  0x698780
#define SITE_ADDPART    0x621CD0
static u8 part_tab[MAXSUB][256 * PS_SIZE];  /* each sub's particle table (game table + its particle.cfg) */
static const u8 PS_FIELDS[][2] = {           /* offset, size of every value particle.cfg sets */
    { 0x18, 4 }, { 0x1C, 4 }, { 0x20, 4 }, { 0x24, 2 }, { 0x26, 2 }, { 0x28, 2 }, { 0x2A, 2 }, { 0x2C, 4 },
    { 0x30, 4 }, { 0x34, 2 }, { 0x36, 1 }, { 0x38, 2 }, { 0x3A, 1 }, { 0x3C, 2 }, { 0x3E, 2 }, { 0x40, 1 },
    { 0x41, 1 }, { 0x42, 2 }, { 0x44, 2 }, { 0x48, 4 }, { 0x4C, 4 }, { 0x50, 4 }, { 0x54, 4 }, { 0x58, 4 },
    { 0x5C, 4 }, { 0x60, 4 }, { 0x64, 4 }, { 0x68, 4 }, { 0x6C, 4 }, { 0x70, 1 }, { 0x71, 1 }, { 0x72, 1 },
    { 0x74, 1 }, { 0x75, 1 }, { 0x76, 1 }, { 0x77, 1 }, { 0x7C, 4 }, { 0x80, 4 }, { 0x84, 4 }, { 0x88, 4 } };

static void part_swap(int k, int type)
{
    u8 *a = (u8 *)A_PARTSYS + type * PS_SIZE, *b = part_tab[k] + type * PS_SIZE, t;
    int i, q;
    for (i = 0; i < (int)(sizeof(PS_FIELDS) / 2); i++)
        for (q = 0; q < PS_FIELDS[i][1]; q++) { t = a[PS_FIELDS[i][0] + q]; a[PS_FIELDS[i][0] + q] = b[PS_FIELDS[i][0] + q]; b[PS_FIELDS[i][0] + q] = t; }
}

typedef u32 (__cdecl *AddPartFn)(int, void *, void *, void *, u32, void *, int, int, int, int);
#define PART_SUBMARK 0xA1                 /* CParticle +0x53 = 0xA1 + sub (0xA1 .. 0xA9) */

/* CParticle::Update 0x561E10 recolours every particle each frame from the table
   (ebp = its system entry, ebx = the particle): fade end time p+0x1C; while it
   runs colour = initial (+0x70) + (fade colour (+0x75) - initial) * (1 - left /
   fade time (+0x7C)), afterwards the fade colour. That block (0x562BF1 ..
   0x562D88) is entered through part_fade_stub: SUB 1 particles get the same
   formula with the sub particle.cfg values. */
#define SITE_PARTFADE    0x562BF1
#define PARTFADE_RESUME  0x562BF6
#define PARTFADE_END     0x562D88
static void __cdecl sub_fade(u8 *pt, u8 *sys)
{
    u32 endt = *(u32 *)(pt + 0x1C), now = G32(0x974B2C), ft;
    int n = pt[0x53] - PART_SUBMARK, k, v;
    u8 *tab = n >= 0 && n < g_nsub && subs[n].part_on ? part_tab[n] + (sys - (u8 *)A_PARTSYS) : sys;
    if (!endt) return;
    ft = *(u32 *)(tab + 0x7C);
    if (endt <= now || !ft) { for (k = 0; k < 3; k++) pt[0x4F + k] = tab[0x75 + k]; return; }
    {   /* integers only: the FPU stack is in use here (5 values); the game truncates too */
        int done = (int)ft - (int)(endt - now);
        for (k = 0; k < 3; k++) {
            v = tab[0x70 + k] + (tab[0x75 + k] - tab[0x70 + k]) * done / (int)ft;
            pt[0x4F + k] = (u8)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }
}
__declspec(naked) static void part_fade_stub(void)
{
    __asm {
        cmp byte ptr [ebx + 0x53], PART_SUBMARK
        jb main_particle
        cmp byte ptr [ebx + 0x53], PART_SUBMARK + MAXSUB - 1
        jbe sub_particle
    main_particle:
        mov edi, [ebx + 0x1C]                   ; the replaced instructions
        test edi, edi
        push PARTFADE_RESUME                    ; back to the original jz
        ret
    sub_particle:
        pushad
        push ebp
        push ebx
        call sub_fade
        add esp, 8
        popad
        push PARTFADE_END
        ret
    }
}

static void install_partfade(void)
{
    static const u8 ORIG[5] = { 0x8B, 0x7B, 0x1C, 0x85, 0xFF };
    u8 *p = (u8 *)SITE_PARTFADE;
    u32 old;
    int i;
    for (i = 0; i < 5; i++) if (p[i] != ORIG[i]) { logs("particle fade patch: unexpected bytes, skipped"); return; }
    if (!pVirtualProtect(p, 5, 0x40, &old)) return;
    p[0] = 0xE9;
    *(int *)(p + 1) = (int)((u32)part_fade_stub - (SITE_PARTFADE + 5));
    pVirtualProtect(p, 5, old, &old);
    logs("particle fade patch installed");
}
static u32 __cdecl hk_addpart(int type, void *pos, void *vel, void *ent, u32 size, void *col, int a6, int a7, int a8, int a9)
{
    u32 r;
    if (!(type & 0x80)) {
        r = ((AddPartFn)orig_addpart)(type, pos, vel, ent, size, col, a6, a7, a8, a9);
        if (r) ((u8 *)r)[0x53] = 0;
        return r;
    }
    {
        int k = (int)(size & 0xF) - 1;                  /* sub number from the size's low bits */
        type &= 0x7F;
        size &= ~0xFu;
        if (k < 0 || k >= g_nsub || !subs[k].part_on || type >= N_PARTSYS)
            return ((AddPartFn)orig_addpart)(type, pos, vel, ent, size, col, a6, a7, a8, a9);
        part_swap(k, type);
        r = ((AddPartFn)orig_addpart)(type, pos, vel, ent, size, col, a6, a7, a8, a9);
        part_swap(k, type);
        if (r) ((u8 *)r)[0x53] = (u8)(PART_SUBMARK + k);   /* CParticle padding byte: a SUB n particle */
        return r;
    }
}

static void load_particles(int k, u32 sz, u32 t0, u32 t1)
{
    static char rel[80], path[120];
    static char buf[60000];
    Sub *S = &subs[k];
    u32 r = 0, old, oldp;
    void *h;
    u8 *tab = part_tab[k];
    int ok = 0, i;
    if (!orig_addpart) return;
    if (S->part_size == sz && S->part_t0 == t0 && S->part_t1 == t1 && S->part_size) { S->part_on = 1; return; }
    if (sz == 0 || sz >= 55000) { logs("looks: particle.cfg too big"); return; }
    looks_path(sput(rel, "..\\"), k, "particle.cfg");
    looks_path(path, k, "particle.cfg");
    h = pCreateFileA(path, 0x80000000, 1, 0, 3, 0x80, 0);
    if (h == (void *)-1) return;
    pReadFile(h, buf, sizeof(buf) - 1, &r, 0);
    pCloseHandle(h);
    buf[r] = 0;
    for (i = 0; buf[i]; i++)                               /* the loader stops at ";the end" */
        if (buf[i] == ';' && buf[i + 1] == 't' && buf[i + 2] == 'h' && buf[i + 3] == 'e' && buf[i + 4] == ' '
            && buf[i + 5] == 'e' && buf[i + 6] == 'n' && buf[i + 7] == 'd') { ok = 1; break; }
    if (!ok) { logs("looks: particle.cfg has no ';the end' line - not used"); return; }
    memcpy(part_tab[k], (void *)A_PARTSYS, N_PARTSYS * PS_SIZE);
    if (!pVirtualProtect((void *)A_PART_CFGNAME, 4, 0x40, &old)) return;
    oldp = G32(A_PART_CFGNAME);
    G32(A_PART_CFGNAME) = (u32)rel;
    __asm {
        mov ecx, tab
        mov eax, F_PART_LOAD
        call eax
    }
    G32(A_PART_CFGNAME) = oldp;
    pVirtualProtect((void *)A_PART_CFGNAME, 4, old, &old);
    cwd_game();
    S->part_size = sz; S->part_t0 = t0; S->part_t1 = t1;
    S->part_on = 1;
    logs("looks: particle.cfg read");
}

static int find_file(const char *path, u32 *size, u32 *t0, u32 *t1)
{
    static u8 fd[320];
    void *fh;
    if (!pFindFirstFileA) return 0;
    fh = pFindFirstFileA(path, fd);
    if (fh == (void *)-1 || !fh) return 0;
    pFindClose(fh);
    if (*(u32 *)fd & 0x10) return 0;
    *t0 = *(u32 *)(fd + 20); *t1 = *(u32 *)(fd + 24); *size = *(u32 *)(fd + 32);
    return 1;
}

static u8 *load_txd(const char *path)
{
    u8 *st, *txd = 0;
    ((I1cFn)F_TXD_MODE)(1);
    st = ((StreamOpenFn)F_STREAM_OPEN)(2, 1, path);    /* rwSTREAMFILENAME, rwSTREAMREAD */
    if (!st) return 0;
    if (((StreamFindFn)F_STREAM_FIND)(st, 0x16, 0, 0)) txd = ((RwP1Fn)F_TXD_READ)(st);
    ((StreamCloseFn)F_STREAM_CLOSE)(st, 0);
    return txd;
}

static u8 *load_bmp(const char *path)
{
    u8 *img, *ras, *tex;
    int w, h, d, f;
    img = ((ImageReadFn)F_IMAGE_READ)(path);
    if (!img) return 0;
    ((RFmtFn)F_IMAGE_RFMT)(img, 4, &w, &h, &d, &f);
    ras = ((RasterCreateFn)F_RASTER_CREATE)(w, h, d, f);
    tex = 0;
    if (ras) {
        ((SetImgFn)F_RASTER_SETIMG)(ras, img);
        tex = ((RwP1Fn)F_TEX_CREATE)(ras);
        if (tex) { *(u32 *)(tex + 0x50) = (*(u32 *)(tex + 0x50) & 0xFFFFFF00u) | 2; scopy((char *)tex + 0x10, "subskin", 32); }
    }
    ((RwP1Fn)F_IMAGE_DESTROY)(img);
    return tex;
}

static void looks_add_desc(Sub *S, const char *n)
{
    int l = slen(S->looks);
    if (l + slen(n) + 3 >= (int)sizeof(S->looks)) return;
    if (l) sput(S->looks + l, ", "), l += 2;
    sput(S->looks + l, n);
}

static void looks_unload(void)
{
    int k;
    if (pal_game >= 0) memcpy((u8 *)A_CARCOLS + pal_game * 4, pal_backup, (256 - pal_game) * 4);
    pal_game = -1;
    for (k = 0; k < MAXSUB; k++) {
        Sub *S = &subs[k];
        S->pal_n = 0; S->ntex = 0; S->skin_on = 0; S->ndff = 0; S->part_on = 0; S->looks[0] = 0;
    }
    looks_any = 0;
    memset(wr_atom, 0, sizeof(wr_atom)); wr_count = 0;
}

/* Looks\SUBn of sub k; *pal = next free palette entry; cwd = game folder */
static void looks_load_one(int k, int *pal_next)
{
    static u8 fd[320];
    static char path[300], key[64];
    Sub *S = &subs[k];
    void *fh;
    u32 sz, t0, t1;
    int i;
    /* .txd files */
    looks_path(path, k, "*.txd");
    fh = pFindFirstFileA(path, fd);
    if (fh != (void *)-1 && fh) {
        do {
            const char *nm = (const char *)(fd + 44);
            LookTxd *L = 0;
            char *e;
            if (*(u32 *)fd & 0x10 || slen(nm) >= 56) continue;
            sz = *(u32 *)(fd + 32); t0 = *(u32 *)(fd + 20); t1 = *(u32 *)(fd + 24);
            e = sput(key, "SUB"); *e++ = (char)('1' + k); *e++ = '\\'; sput(e, nm);
            for (i = 0; i < nlk_txd; i++) {
                const char *a = lk_txd[i].name; int q = 0;
                while (a[q] && (a[q] | 0x20) == (key[q] | 0x20)) q++;
                if (!a[q] && !key[q]) { L = &lk_txd[i]; break; }
            }
            if (!L) { if (nlk_txd >= 64) continue; L = &lk_txd[nlk_txd++]; scopy(L->name, key, sizeof(L->name)); L->txd = 0; }
            if (!L->txd || L->size != sz || L->t0 != t0 || L->t1 != t1) {   /* new or changed: read it */
                looks_path(path, k, nm);
                L->txd = load_txd(path);
                L->size = sz; L->t0 = t0; L->t1 = t1;
                if (!L->txd) { char line[120]; sput(sput(line, "looks: could not read "), key); logs(line); continue; }
            }
            if (L->txd) {                                   /* its textures: list at txd + 8, link at tex + 8 */
                u8 *head = L->txd + 8, *l = *(u8 **)head;
                int guard = 0;
                while (l && l != head && S->ntex < 256 && guard++ < 4096) { S->tex[S->ntex++] = l - 8; l = *(u8 **)l; }
                looks_add_desc(S, nm);
                looks_any = 1;
            }
        } while (pFindNextFileA(fh, fd));
        pFindClose(fh);
    }
    /* .dff files: model copies */
    fh = pFindFirstFileA(looks_path(path, k, "*.dff"), fd);
    if (fh != (void *)-1 && fh) {
        do {
            const char *nm = (const char *)(fd + 44);
            int d;
            if (*(u32 *)fd & 0x10 || slen(nm) >= 60 || S->ndff >= 16) continue;
            d = load_dff(k, nm, *(u32 *)(fd + 32), *(u32 *)(fd + 20), *(u32 *)(fd + 24));
            if (d >= 0) {
                S->dff_orig[S->ndff] = lk_dff[d].orig; S->dff_clone[S->ndff++] = lk_dff[d].clone;
                looks_add_desc(S, nm);
            }
        } while (pFindNextFileA(fh, fd));
        pFindClose(fh);
    }
    cwd_game();
    /* particle.cfg */
    if (find_file(looks_path(path, k, "particle.cfg"), &sz, &t0, &t1)) {
        load_particles(k, sz, t0, t1);
        if (S->part_on) looks_add_desc(S, "particle.cfg");
    }
    /* player.bmp */
    if (find_file(looks_path(path, k, "player.bmp"), &sz, &t0, &t1)) {
        if (!S->skin || S->skin_size != sz || S->skin_t0 != t0 || S->skin_t1 != t1) {
            if (S->skin) ((RwP1Fn)F_TEX_DESTROY)(S->skin);
            S->skin = load_bmp(path);
            S->skin_size = sz; S->skin_t0 = t0; S->skin_t1 = t1;
        }
        if (S->skin) { S->skin_on = 1; looks_add_desc(S, "player.bmp"); }
    }
    /* carcols.dat: 'col' section -> the next free palette entries */
    if (pCreateFileA && pReadFile) {
        static char buf[16384];
        u32 r = 0;
        void *h = pCreateFileA(looks_path(path, k, "carcols.dat"), 0x80000000, 1, 0, 3, 0x80, 0);
        if (h != (void *)-1) {
            u8 *pal = (u8 *)A_CARCOLS;
            int base = *pal_next, n = 0, sect = 0;
            char *q, *eol;
            pReadFile(h, buf, sizeof(buf) - 1, &r, 0);
            pCloseHandle(h);
            buf[r] = 0;
            for (q = buf; *q && base > 0; q = eol) {
                int v[3], c;
                for (eol = q; *eol && *eol != '\n'; eol++) {}
                if (*eol) *eol++ = 0;
                while (*q == ' ' || *q == '\t' || *q == '\r') q++;
                if (*q == '#' || !*q) continue;
                if (q[0] == 'c' && q[1] == 'o' && q[2] == 'l') { sect = 1; continue; }
                if (q[0] == 'e' && q[1] == 'n' && q[2] == 'd') { sect = 0; continue; }
                if (q[0] == 'c' && q[1] == 'a' && q[2] == 'r') { sect = 2; continue; }
                if (sect != 1) continue;
                for (c = 0; c < 3; c++) {
                    v[c] = 0;
                    while (*q && (*q < '0' || *q > '9')) q++;
                    while (*q >= '0' && *q <= '9') v[c] = v[c] * 10 + (*q++ - '0');
                }
                if (base + n >= 256) break;
                pal[(base + n) * 4] = (u8)v[0]; pal[(base + n) * 4 + 1] = (u8)v[1];
                pal[(base + n) * 4 + 2] = (u8)v[2]; pal[(base + n) * 4 + 3] = 0xFF;
                n++;
            }
            S->pal_base = base; S->pal_n = n;
            *pal_next = base + n;
            if (n) looks_add_desc(S, "carcols.dat");
        }
    }
    {
        char line[300], *e = sput(line, "looks SUB");
        *e++ = (char)('1' + k); e = sput(e, ": ");
        e = sput(e, S->looks[0] ? S->looks : "none"); e = sput(e, "  textures "); iput(e, S->ntex, 0);
        logs(line);
    }
}

/* (re)read Looks\SUB1 .. SUBn for the subs in use; cwd = game folder */
static void looks_load(void)
{
    int k, pal_next = 0;
    u8 *pal = (u8 *)A_CARCOLS;
    looks_unload();
    if (!pFindFirstFileA || !g_nsub) return;
    while (pal_next < 256 && pal[pal_next * 4 + 3] == 0xFF) pal_next++;   /* the game's colours (alpha 255) */
    pal_game = pal_next;
    memcpy(pal_backup, pal + pal_game * 4, (256 - pal_game) * 4);
    for (k = 0; k < g_nsub; k++) looks_load_one(k, &pal_next);
}

/* ------------------------------------------------------ SUB 1 .. SUB 9 editing */
static u32 g_subdel_until;
static int g_pick_slot;                    /* the sub the picker fills (= g_nsub: a new one) */

/* rebuild the project table after a sub change; keys, marks and the shown
   moment of the main replay stay where they are */
static void rebuild_project(void)
{
    int f = shown_frame(), T;
    T = f >= 0 && f < nfr ? rel_ms(f) : 0;
    keys_to_times();
    if (!build_project()) { g_nsub = 0; looks_unload(); build_project(); message("SUB REPLAYS TOO LONG - REMOVED", 0); }
    times_to_keys();
    ed.limit = nfr - 1;
    ed.paused = 1;
    ed.dirty = 1;
    g_win = 1;                                     /* merged frames: always through the window */
    w0 = w1 = 0;                                   /* refill */
    f = frame_at_ms(T);
    window_goto(f);
    seek(f);
}

static int sub_step(void);
/* a sub's start offset as main frames (offset / average main frame time, rounded) */
static char *offs_put(char *e, int ms)
{
    int st = sub_step(), f;
    *e++ = ms < 0 ? '-' : '+';
    if (ms < 0) ms = -ms;
    f = (ms + st / 2) / st;
    e = iput(e, f, 0);
    return sput(e, f == 1 ? " FRAME" : " FRAMES");
}

static char *subname_put(char *e, int k) { e = sput(e, "SUB "); *e++ = (char)('1' + k); *e = 0; return e; }

/* remove sub k (later subs move up: SUB k+1 becomes SUB k) */
static void drop_sub(int k)
{
    Sub tmp;
    int i;
    if (k < 0 || k >= g_nsub) return;
    tmp = subs[k];
    if (tmp.data && pVirtualFree) pVirtualFree(tmp.data, 0, 0x8000);
    tmp.data = 0; tmp.len = 0; tmp.skin = 0; tmp.skin_size = 0; tmp.part_size = 0;
    if (subs[k].skin) ((RwP1Fn)F_TEX_DESTROY)(subs[k].skin);
    for (i = k; i < g_nsub - 1; i++) subs[i] = subs[i + 1];
    subs[g_nsub - 1] = tmp;                        /* its frame tables are kept for reuse */
    g_nsub--;
    for (i = 0; i < MAXSUB; i++) subs[i].part_size = 0;   /* particle tables follow the numbers: read again */
}

/* sub k = name (k == g_nsub: a new sub) */
static void attach_sub(int k, const char *name)
{
    static const u8 HDR[8] = { 'g', 't', 'a', 'V', 'C', '7', 'f', 0 };
    char line[160], *e;
    void *fh;
    Sub *S;
    u32 r = 0, size = 0;
    int i, ok = 0, added = k >= g_nsub;
    if (k > g_nsub || k >= MAXSUB || !pCreateFileA || !pReadFile || !pGetFileSize) return;
    S = &subs[k];
    cwd_docs();
    fh = pCreateFileA(name, 0x80000000, 1, 0, 3, 0x80, 0);
    if (fh != (void *)-1) {
        size = pGetFileSize(fh, 0);
        ok = size > 8 + 64 && size <= REP_MAX && sub_alloc(S, size) && pReadFile(fh, S->data, size, &r, 0) && r == size;
        pCloseHandle(fh);
    }
    cwd_game();
    for (i = 0; ok && i < 8; i++) if (S->data[i] != HDR[i]) ok = 0;
    if (ok) { S->len = size; ok = sub_build(S); }
    if (ok) {
        scopy(S->name, name, sizeof(S->name));
        if (added) { S->off = 0; g_nsub++; }
        ok = build_remap();
        if (!ok) { error_name("TOO MANY VEHICLES / PEDS WITH THIS SUB REPLAY: ", name); drop_sub(k); build_remap(); }
    } else {
        error_name("CANNOT USE AS SUB REPLAY: ", name);
        if (!added) { drop_sub(k); build_remap(); }
    }
    ed.track = ok ? TR_SUB + k : (g_nsub ? TR_SUB + g_nsub - 1 : TR_CAM);
    looks_load();                                  /* before the frames are encoded (colours, models) */
    rebuild_project();
    if (!ok) return;
    e = subname_put(line, k); e = sput(e, added ? " ADDED - " : " REPLACED - "); dput(e, name, 60);
    message(line, 1500);
    logs(line);
}

static void remove_sub(int k)
{
    char line[40], *e;
    drop_sub(k);
    build_remap();
    looks_load();
    ed.track = g_nsub ? TR_SUB + (k < g_nsub ? k : g_nsub - 1) : TR_CAM;
    rebuild_project();
    e = subname_put(line, k); sput(e, " REMOVED");
    message(line, 1000);
}

static void shift_sub(int k, int ms)
{
    char line[96], *e;
    Sub *S = &subs[k];
    S->off += ms;
    if (S->off > 600000) S->off = 600000;
    if (S->off < -600000) S->off = -600000;
    rebuild_project();
    e = subname_put(line, k); e = sput(e, " START "); offs_put(e, S->off);
    message(line, 700);
}

/* one main frame in ms */
static int sub_step(void) { int d = mnf > 1 ? (int)(mt[mnf - 1] - mt[0]) / (mnf - 1) : 33; return d > 0 ? d : 33; }

/* ------------------------------------------------------------------ menu */
#define MAXFILES 160
#define MENU_ROWS 12
typedef struct {
    char name[200];
    u32 ft[2];                         /* last write FILETIME */
    u32 size;
    u8 prj, prj_ok, embedded;
    short nkf;
    char src[64];                      /* .prj: its source replay (display copy) */
} FileEnt;
static FileEnt files[MAXFILES];
static int nfiles;

static struct {
    int open, page, sel, top, mode;    /* page 0: OPEN/PLAY/DELETE, 1: file list; mode 0 edit 1 play 2 delete 3 sub (editor) */
    u32 confirm_until;                 /* mission running: second Enter before this loads */
    int release_wait;                  /* closed: keep hiding keys until the menu keys are up */
    int del_confirm;                   /* DELETE: confirm box for files[sel] is up */
    char note[120];                    /* DELETE: result line under the list */
    u32 note_until;
} menu;
#define MENU_ITEMS 3
#define RE_VERSION "v1.02"                 /* shown at the right end of the F6 panel header */

static void scan(const char *pattern, const char *ext, int prj)
{
    static u8 fd[320];                 /* WIN32_FIND_DATAA */
    static PrjHdr h;
    void *fh;
    u32 rsz;
    if (!pFindFirstFileA) return;
    fh = pFindFirstFileA(pattern, fd);
    if (fh == (void *)-1 || !fh) return;
    do {
        const char *nm = (const char *)(fd + 44);
        FileEnt *f;
        if (nfiles >= MAXFILES) break;
        if (*(u32 *)fd & 0x10) continue;                         /* directory */
        if (slen(nm) >= (int)sizeof(f->name) || !ends_with(nm, ext)) continue;
        f = &files[nfiles++];
        scopy(f->name, nm, sizeof(f->name));
        f->ft[0] = *(u32 *)(fd + 20); f->ft[1] = *(u32 *)(fd + 24);
        f->size = *(u32 *)(fd + 32);
        f->prj = (u8)prj; f->prj_ok = 0; f->nkf = 0; f->src[0] = 0; f->embedded = 0;
        if (prj && read_prj(nm, &h, 0, &rsz, 0)) {
            f->prj_ok = 1; f->nkf = (short)h.nkf; f->embedded = rsz != 0;
            scopy(f->src, h.src, sizeof(f->src));
        }
    } while (pFindNextFileA(fh, fd));
    pFindClose(fh);
}

static int newer(const FileEnt *a, const FileEnt *b)
{
    return a->ft[1] != b->ft[1] ? a->ft[1] > b->ft[1] : a->ft[0] > b->ft[0];
}

static void list_files(int which)          /* 0 all, 1 .prj only, 2 .rep only */
{
    static FileEnt tmp;
    int i, j;
    nfiles = 0;
    cwd_docs();
    if (which != 1) scan("*.rep", ".rep", 0);
    if (which != 2) scan("*.prj", ".prj", 1);
    cwd_game();
    for (i = 1; i < nfiles; i++) {                               /* newest first */
        tmp = files[i];
        for (j = i; j > 0 && newer(&tmp, &files[j - 1]); j--) files[j] = files[j - 1];
        files[j] = tmp;
    }
}

/* CTheScripts: ScriptSpace[OnAMissionFlag] != 0 while a mission runs */
static int on_mission(void)
{
    u32 off = G32(A_ONMISSION_OFS);
    return off && off < 0x40000 && GI32(A_SCRIPTSPACE + off) != 0;
}

/* The game must not see the keys used in the menu (Enter = enter vehicle).
   CPad::UpdatePads (0x4AB6C0, start of CGame::Process) first updates the pads
   from NewKeyState, then copies TempKeyState -> NewKeyState; clearing both at
   the end of every frame the menu is open hides those keys from the game. */
#define A_NEWKEYSTATE   0x7D15A8
#define A_TEMPKEYSTATE  0x864A00
#define KEYSTATE_SIZE   0x270
static void swallow_keys(void)
{
    memset((void *)A_NEWKEYSTATE, 0, KEYSTATE_SIZE);
    memset((void *)A_TEMPKEYSTATE, 0, KEYSTATE_SIZE);
}

static void menu_open(void)
{
    menu.open = 1; menu.page = 0; menu.sel = 0; menu.top = 0; menu.confirm_until = 0;
    menu.del_confirm = 0; menu.note_until = 0;
}

/* DELETE PROJECT: remove files[i] from the user folder, then list again */
static void delete_project(int i)
{
    char *e;
    int ok;
    cwd_docs();
    ok = pDeleteFileA && pDeleteFileA(files[i].name);
    cwd_game();
    e = sput(menu.note, ok ? "DELETED " : "COULD NOT DELETE ");
    dput(e, files[i].name, 60);
    menu.note_until = now_ms() + 2500;
    logs(menu.note);
    list_files(1);
    if (menu.sel >= nfiles) menu.sel = nfiles - 1;
    if (menu.sel < 0) menu.sel = 0;
    if (menu.top > menu.sel) menu.top = menu.sel;
}

static void menu_close(void)
{
    menu.open = 0;
    menu.release_wait = 1;
}

static int menu_keys_down(void)
{
    static const u8 k[] = { 0x08, 0x0D, 0x21, 0x22, 0x23, 0x24, 0x26, 0x28 };
    int i;
    for (i = 0; i < (int)sizeof(k); i++) if (key_now[k[i]]) return 1;
    return key_now[K_MENU] != 0;
}

static void choose(const FileEnt *f, int mode)
{
    if (mode == 3) { attach_sub(g_pick_slot, f->name); return; }
    if (mode == 0) {
        if (f->prj) open_project(f->name, 0);
        else open_rep_in_editor(f->name);
    } else {
        if (f->prj) open_project(f->name, 1);
        else if (f->size > DIRECT_MAX) play_long(f->name);
        else if (play_file(f->name)) g_filecam = 1;
    }
}

static void menu_update(void)
{
    if (G8(A_MODE) != 0 && menu.mode != 3) { menu_close(); return; }   /* e.g. F3 pressed meanwhile */
    if (pressed(K_MENU)) { menu_close(); return; }               /* F6 (menu key) */
    if (menu.page == 0) {
        if (repeat(0x26)) menu.sel = (menu.sel + MENU_ITEMS - 1) % MENU_ITEMS;
        if (repeat(0x28)) menu.sel = (menu.sel + 1) % MENU_ITEMS;
        if (pressed(0x08)) { menu_close(); return; }
        if (pressed(0x0D)) {
            menu.mode = menu.sel;
            list_files(menu.mode == 2 ? 1 : 0);
            menu.page = 1; menu.sel = 0; menu.top = 0; menu.del_confirm = 0; menu.note_until = 0;
        }
        return;
    }
    if (menu.del_confirm) {                                        /* confirm box */
        if (pressed(0x0D)) { menu.del_confirm = 0; delete_project(menu.sel); }
        else if (pressed(0x08)) menu.del_confirm = 0;
        return;
    }
    if (repeat(0x26)) menu.sel--;
    if (repeat(0x28)) menu.sel++;
    if (repeat(0x21)) menu.sel -= MENU_ROWS;
    if (repeat(0x22)) menu.sel += MENU_ROWS;
    if (pressed(0x24)) menu.sel = 0;
    if (pressed(0x23)) menu.sel = nfiles - 1;
    if (menu.sel >= nfiles) menu.sel = nfiles - 1;
    if (menu.sel < 0) menu.sel = 0;
    if (menu.sel < menu.top) menu.top = menu.sel;
    if (menu.sel >= menu.top + MENU_ROWS) menu.top = menu.sel - MENU_ROWS + 1;
    if (pressed(0x08)) {
        if (menu.mode == 3) { menu_close(); return; }              /* sub picker: back to the editor */
        menu.page = 0; menu.sel = menu.mode; return;
    }
    if (pressed(0x0D) && nfiles > 0 && menu.mode == 2) { menu.del_confirm = 1; return; }
    if (pressed(0x0D) && nfiles > 0) {
        int sel = menu.sel, mode = menu.mode;
        if (mode != 3 && on_mission() && (int)(menu.confirm_until - now_ms()) <= 0) {
            menu.confirm_until = now_ms() + 4000;
            return;
        }
        menu.confirm_until = 0;
        menu_close();
        choose(&files[sel], mode);
    }
}

/* SUB 1 track, K: pick a .rep in the editor */
static void sub_picker(void)
{
    menu.open = 1; menu.page = 1; menu.mode = 3; menu.sel = 0; menu.top = 0;
    menu.del_confirm = 0; menu.note_until = 0; menu.confirm_until = 0;
    list_files(2);
    ed.paused = 1;
}

/* K / DEL on the SUB 1 track ([ / ] in move_key) */
static void sub_keys(void)
{
    int k = ed.track - TR_SUB;
    if (k < 0 || k >= g_nsub) return;
    if (pressed('K')) { g_pick_slot = k; sub_picker(); }
    if (pressed(0x2E)) {
        if ((int)(g_subdel_until - now_ms()) > 0) { g_subdel_until = 0; remove_sub(k); }
        else { char line[60], *e = sput(line, "DEL AGAIN: REMOVE "); subname_put(e, k); g_subdel_until = now_ms() + 3000; message(line, 3000); }
    }
}

/* [ / ]: move the selected keyframe of the current track one frame back / forward.
   Keys stay sorted and unique, so only the neighbour can be in the way. */
static void move_key(int dir)
{
    char line[96], *e;
    int tr = ed.track, i, n, *pf, *pn = 0, nf;
    if (tr >= TR_SUB) {
        if (tr - TR_SUB >= g_nsub) { message("NO SUB REPLAY - N: ADD ONE", 700); return; }
        shift_sub(tr - TR_SUB, dir * (key_now[0x10] ? 1000 : sub_step()));
        return;
    }
    if (tr == TR_CAM) { i = ed.sel; n = nkf; }
    else { i = esel[tr - 1]; n = nek[tr - 1]; }
    if (i < 0 || i >= n) { message("NO KEYFRAME SELECTED", 500); return; }
    pf = tr == TR_CAM ? &kf[i].frame : &ek[tr - 1][i].frame;
    if (i + dir >= 0 && i + dir < n) pn = tr == TR_CAM ? &kf[i + dir].frame : &ek[tr - 1][i + dir].frame;
    nf = *pf + dir;
    if (nf < 0 || nf >= nfr) { message(dir < 0 ? "CANNOT MOVE - ALREADY ON THE FIRST FRAME" : "CANNOT MOVE - ALREADY ON THE LAST FRAME", 500); return; }
    if (pn && *pn == nf) {
        e = sput(line, "CANNOT MOVE - FRAME "); e = iput(e, nf + 1, 0); sput(e, " ALREADY HAS A KEYFRAME");
        message(line, 500);
        return;
    }
    *pf = nf;
    ed.dirty = 1;
    ed.paused = 1;
    seek(nf);
    e = sput(line, tr == TR_CAM ? "KEYFRAME " : tr == TR_WEATHER ? "WEATHER KEY " : "TIME KEY ");
    e = iput(e, i + 1, 0); e = sput(e, " MOVED TO FRAME "); iput(e, nf + 1, 0);
    message(line, 500);
}

static void env_toast(int tr, int i, const char *what)
{
    char line[96], *e;
    e = sput(line, tr == 0 ? "WEATHER KEY " : "TIME KEY "); e = iput(e, i + 1, 0);
    e = sput(e, what); e = iput(e, ek[tr][i].frame + 1, 0); e = sput(e, " - ");
    if (tr == 0) e = sput(e, WEATHER_NAME[ek[tr][i].val]); else e = hhmm(e, ek[tr][i].val);
    e = sput(e, "  "); sput(e, ip_name(ek[tr][i].cut, 1));
    message(line, 700);
}

/* K / DEL / PGUP / PGDN / T / , . / B on the weather or time track */
static void env_keys(int f, int shift)
{
    int tr = ed.track - 1, i, j, n = nek[tr], *sel = &esel[tr];
    EKey *k = ek[tr];
    if (pressed('K') && f >= 0) {
        int val = tr == 0 ? cur_weather() : cur_minute();
        for (i = 0; i < n && k[i].frame < f; i++) {}
        if (i < n && k[i].frame == f) {
            k[i].val = (short)val; *sel = i; ed.dirty = 1; env_toast(tr, i, " UPDATED - FRAME ");
        } else if (n + nhek[tr] >= MAXEK) message("KEYFRAME LIMIT REACHED (64)", 0);
        else {
            for (j = n; j > i; j--) k[j] = k[j - 1];
            k[i].frame = f; k[i].val = (short)val; k[i].cut = i > 0 ? k[i - 1].cut : 0; k[i].rsv = 0;
            nek[tr] = ++n; *sel = i; ed.dirty = 1;
            env_toast(tr, i, " ADDED - FRAME ");
        }
    }
    if (*sel >= n) *sel = n - 1;
    if (pressed(0x2E) && *sel >= 0) {                     /* Delete */
        for (j = *sel; j < n - 1; j++) k[j] = k[j + 1];
        nek[tr] = --n;
        if (*sel >= n) *sel = n - 1;
        ed.dirty = 1;
    }
    if (pressed(0x21) && n) {                              /* PgUp / PgDn: select + go there */
        for (i = n - 1; i >= 0 && k[i].frame >= f; i--) {}
        *sel = i >= 0 ? i : 0; ed.paused = 1; seek(k[*sel].frame);
    }
    if (pressed(0x22) && n) {
        for (i = 0; i < n && k[i].frame <= f; i++) {}
        *sel = i < n ? i : n - 1; ed.paused = 1; seek(k[*sel].frame);
    }
    if (*sel < 0) return;
    i = *sel;
    if (tr == 0) {                                         /* weather: T / . next, , previous */
        int d = (pressed('T') || repeat(0xBE)) ? 1 : repeat(0xBC) ? -1 : 0;
        if (d) { k[i].val = (short)((k[i].val + d + NWEATHER) % NWEATHER); ed.dirty = 1; env_toast(tr, i, " - FRAME "); }
    } else {                                               /* time: , / . -/+ 10 min (Shift 1 h) */
        int d = repeat(0xBE) ? 1 : repeat(0xBC) ? -1 : 0;
        if (d) {
            k[i].val = (short)(((k[i].val + d * (shift ? 60 : 10)) % 1440 + 1440) % 1440);
            ed.dirty = 1; env_toast(tr, i, " - FRAME ");
        }
    }
    if (pressed('B')) { k[i].cut = (u8)ip_next(k[i].cut, 1, shift ? -1 : 1); ed.dirty = 1; env_toast(tr, i, " - FRAME "); }
}

static void __cdecl hk_update(void)
{
    int shift, f, i;
    float dt = 0;
    u32 now;
    if (!g_installed) { ((vfn)orig_update)(); return; }
    poll_keys();
    now = pTimeGetTime ? pTimeGetTime() : ed.last_ms + 33;
    dt = fclamp((float)(int)(now - ed.last_ms) * 0.001f, 0.0f, 0.1f);
    ed.last_ms = now;
    if (ed.active && G8(A_MODE) != 1) {
        ed.active = 0; ed.viewer = 0; g_win = 0; GF(A_FOV) = REPLAY_FOV; logs("playback ended outside the editor");
        g_nsub = 0; looks_unload(); dff_unregister();
        clear_pad_input(); swallow_keys();
    }
    if (G8(A_MODE) != 1) {
        if (g_filecam) { clear_pad_input(); swallow_keys(); }   /* F6 PLAY of a .rep has ended */
        g_filecam = 0;
    } else if (ed.active || g_filecam) clear_pad_input();       /* our playback: drop key presses */
    if (!ed.active) {
        if (menu.open) { menu_update(); swallow_keys(); }    /* also on the closing frame */
        else if (pressed(K_MENU) && can_enter()) {
            if (g_keydup) {                                  /* bad [Keys] setting: message only */
                char line[200], *e = sput(line, "KEY SETTING ERROR: THE SAME KEY IS SET TWICE IN [Keys] OF ReplayEditor.ini (");
                e = sput(e, "MENU="); e = sput(e, g_keyname[KEY_MENU]); e = sput(e, " SAVE="); e = sput(e, g_keyname[KEY_SAVE]);
                e = sput(e, " SAVE_AS="); e = sput(e, g_keyname[KEY_SAVEAS]); sput(e, ")");
                message(line, 0);
            } else menu_open();
            swallow_keys();
        }
        else if (menu.release_wait) {
            if (menu_keys_down() && G8(A_MODE) == 0) swallow_keys();
            else menu.release_wait = 0;
        }
        ((vfn)orig_update)();
        return;
    }
    if (ed.viewer) {                                       /* PLAY of a project */
        if (pressed(K_MENU) || ed.paused) { exit_editor(); return; }
        ((vfn)orig_update)();
        return;
    }
    if (menu.open) {                                       /* SUB 1 file picker */
        menu_update();
        ((vfn)orig_update)();
        return;
    }
    if (pressed(K_MENU)) {                                 /* F6 (menu key): leave the editor */
        char line[100];
        if (!ed.dirty || (int)(ed.exit_until - now) > 0) { exit_editor(); return; }
        ed.exit_until = now + 3000;
        kput(line, "UNSAVED CHANGES - \2 SAVE   /   \1 AGAIN: EXIT WITHOUT SAVING");
        message(line, 3000);
    }
    shift = key_now[0x10];
    f = shown_frame();
    if (pressed('H')) ed.help = !ed.help;
    if (pressed(0x20)) {                                   /* Space */
        if (ed.paused) {
            ed.freecam = 0;                                /* play shows the edited camera */
            if (f >= nfr - 1 || f == ed.out) { seek(ed.in); f = shown_frame(); }
            start_playing(f);
        } else ed.paused = 1;
    }
    if (f >= 0) {
        if (repeat(0x25)) { ed.paused = shift ? ed.paused : 1; seek(f - (shift ? 30 : 1)); }
        if (repeat(0x27)) { ed.paused = shift ? ed.paused : 1; seek(f + (shift ? 30 : 1)); }
        if (pressed(0x24)) seek(ed.in);                    /* Home / End: trim marks */
        if (pressed(0x23)) { ed.paused = 1; seek(ed.out); }
        f = shown_frame();
        if (pressed('I')) set_trim(0, shift ? 0 : f);
        if (pressed('O')) set_trim(1, shift ? nfr - 1 : f);
    }
    if (pressed(K_SAVE)) { ed.paused = 1; save_edit(0); }      /* F7: save */
    if (pressed(K_SAVEAS)) { ed.paused = 1; save_edit(1); }    /* F8: save as a new file */
    for (i = 0; i < 5; i++) if (pressed(0x31 + i)) set_speed(i);

    /* camera / keyframes */
    if (pressed(0x09)) {                                   /* Tab: free camera */
        ed.freecam = !ed.freecam;
        /* always start from the camera currently on screen (recorded, keyframe
           path or look-around), never from the previous free-camera pose */
        if (ed.freecam) pose_from_camera(&ed.fc);
    }
    if (ed.track >= ntracks()) ed.track = TR_CAM;
    if (pressed(0x26)) ed.track = (ed.track + ntracks() - 1) % ntracks();   /* Up / Down: track */
    if (pressed(0x28)) ed.track = (ed.track + 1) % ntracks();
    if (pressed('N')) {                                    /* N: new sub replay -> SUB 1 track */
        if (g_nsub >= MAXSUB) message("UP TO 9 SUB REPLAYS - K ON A SUB TRACK REPLACES ONE", 1500);
        else { g_pick_slot = g_nsub; sub_picker(); }
    }
    f = shown_frame();
    if (repeat(0xDB)) { move_key(-1); f = shown_frame(); }      /* [ */
    if (repeat(0xDD)) { move_key(1); f = shown_frame(); }       /* ] */
    if (ed.track >= TR_SUB) { sub_keys(); goto keys_done; }
    if (ed.track != TR_CAM) { env_keys(f, shift); goto keys_done; }
    if (pressed('K') && f >= 0) {
        Pose p;
        if (ed.freecam) p = ed.fc; else pose_from_camera(&p);
        int added;
        i = add_key(f, &p, &added);
        if (i < 0) message("KEYFRAME LIMIT REACHED (64)", 0);
        else {
            char line[96], *e;
            ed.sel = i;
            ed.dirty = 1;
            e = sput(line, "KEYFRAME "); e = iput(e, i + 1, 0);
            e = sput(e, added ? " ADDED - FRAME " : " UPDATED - FRAME "); e = iput(e, f + 1, 0);
            if (added) { e = sput(e, " - "); sput(e, KF_NAME[kf[i].type]); }
            message(line, 500);
        }
    }
    if (pressed(0x2E) && ed.sel >= 0) {                    /* Delete */
        delete_key(ed.sel);
        if (ed.sel >= nkf) ed.sel = nkf - 1;
        ed.dirty = 1;
    }
    if (pressed(0x21) && nkf) {                            /* PgUp: previous keyframe */
        for (i = nkf - 1; i >= 0 && kf[i].frame >= f; i--) {}
        select_key(i >= 0 ? i : 0);
    }
    if (pressed(0x22) && nkf) {                            /* PgDn: next keyframe */
        for (i = 0; i < nkf && kf[i].frame <= f; i++) {}
        select_key(i < nkf ? i : nkf - 1);
    }
    if (ed.sel >= nkf) ed.sel = nkf - 1;
    if (pressed('T') && ed.sel >= 0) { kf[ed.sel].type = KF_NEXT[kf[ed.sel].type]; ed.dirty = 1; }
    if (pressed('B') && ed.sel >= 0) {
        char line[80], *e;
        kf[ed.sel].cut = (u8)ip_next(kf[ed.sel].cut, 0, key_now[0x10] ? -1 : 1);
        ed.dirty = 1;
        e = sput(line, "KEYFRAME "); e = iput(e, ed.sel + 1, 0); e = sput(e, ": "); sput(e, ip_name(kf[ed.sel].cut, 0));
        message(line, 700);
    }
keys_done:
    if (pressed('P')) {                                    /* preview from the start mark */
        ed.freecam = 0;
        seek(ed.in);
        start_playing(ed.in);
    }
    update_freecam(dt);
    ((vfn)orig_update)();
}

static char __cdecl hk_stdcam(void)
{
    if (g_installed) looks_wrap();                 /* right after CReplay::Update, before the scene is drawn */
    if (g_installed && G8(A_MODE) == 1 && (ed.active || g_filecam)) return 0;
    return ((BoolFn)orig_stdcam)();
}

static void __cdecl hk_playback(void)
{
    int c, i;
    if (!ed.active || !g_installed) { call_playback(); return; }
    c = cur_index();
    if (g_win && c >= 0 && c < nfr && w1 < nfr && c >= w1 - 2) { window_goto(c); c = cur_index(); }
    if (c < 0) { call_playback(); return; }               /* unknown position: let the game run */
    if (ed.need_step) {
        ed.need_step = 0;
        if (c < nfr) { G32(A_DIVISOR) = 1; G32(A_INTERP_CNT) = 0; call_playback(); set_speed(ed.speed); }
        return;
    }
    /* never let the game reach the end of the data: it would finish playback.
       With slow motion the game also reads the frame after the current one. */
    if (c >= nfr || (G32(A_DIVISOR) > 1 && c >= nfr - 1) || c > ed.limit) ed.paused = 1;
    if (ed.paused) { editor_camera(); return; }
    for (i = 0; i < SPEED_REP[ed.speed]; i++) {
        c = cur_index();
        if (g_win && c >= 0 && c < nfr && w1 < nfr && c >= w1 - 2) { window_goto(c); c = cur_index(); }
        if (G8(A_MODE) != 1 || c < 0 || c >= nfr || c > ed.limit) break;
        call_playback();
    }
    c = cur_index();
    if (c >= nfr || c > ed.limit) ed.paused = 1;
}

static void __cdecl hk_lookaround(void)
{
    if (!ed.active || !g_installed) { ((vfn)orig_lookaround)(); return; }
    editor_camera();
}

/* ------------------------------------------------------------------- HUD */
static float g_w, g_h;
static float g_fs = 0.6f;          /* ReplayEditor.ini [Display] FONT_SCALE / 100 */
#define K g_fs                     /* text and text-box sizes scale with it */

static void rect(float x0, float y0, float x1, float y1, u8 r, u8 g, u8 b, u8 a)
{
    float rc[4];
    u8 col[4];
    rc[0] = x0; rc[1] = y1; rc[2] = x1; rc[3] = y0;
    col[0] = r; col[1] = g; col[2] = b; col[3] = a;
    Sprite2d_DrawRect(rc, col);
}

static void text(float x, float y, float scale, u8 r, u8 g, u8 b, const char *s)
{
    u16 w[160];
    u8 col[4];
    int i;
    for (i = 0; s[i] && i < 159; i++) w[i] = (u8)s[i];
    w[i] = 0;
    scale *= K;
    Font_SetScale(g_w / 640.0f * 0.5f * scale, g_h / 448.0f * 1.0f * scale);
    Font_ResetA();
    Font_ResetB();
    Font_SetWrapx(g_w);
    Font_ResetC();
    Font_SetPropOn();
    col[0] = r; col[1] = g; col[2] = b; col[3] = 255;
    Font_SetColor(col);
    Font_SetFontStyle(1);
    Font_PrintString(x, y, w);
}

static const char *HELP[] = {
    "PLAYBACK   SPACE: PLAY/PAUSE    LEFT/RIGHT: FRAME    SHIFT+LEFT/RIGHT: 1 SEC",
    "                HOME/END: GO TO START/END MARK    1-5: SPEED    \1: EXIT EDITOR",
    "CAMERA      TAB: FREE CAMERA ON/OFF    W/A/S/D: MOVE    Q/E: DOWN/UP",
    "                MOUSE: LOOK    Z/C: ROLL    R: RESET ROLL    -/=: ZOOM",
    "                SHIFT: FAST    CTRL: SLOW",
    "KEYFRAMES  K: ADD/UPDATE AT THIS FRAME    DEL: DELETE SELECTED    [ / ]: MOVE SELECTED -/+1 FRAME",
    "                PGUP/PGDN: PREVIOUS/NEXT    T: TYPE    B / SHIFT+B: INTERPOLATION (SMOOTH, LINEAR, EASE.., CUT)",
    "                P: PREVIEW FROM START MARK",
    "TRACKS     UP/DOWN: SELECT TRACK (CAMERA / WEATHER / TIME / SUB 1..9)    N: ADD A SUB REPLAY (UP TO 9)",
    "                WEATHER  T OR ,/.: CHANGE TYPE        TIME  ,/.: -/+ 10 MIN (SHIFT: 1 HOUR)",
    "                SUB n  K: REPLACE SUB REPLAY    DEL: REMOVE    [ / ]: MOVE -/+1 FRAME (SHIFT: 1 SEC)",
    "TRIM         I: SET START    O: SET END    SHIFT+I/O: RESET",
    "SAVE         \2: SAVE PROJECT",
    "                \3: SAVE AS NEW PROJECT",
    "H: HIDE/SHOW THIS HELP",
};

static void draw_help(void)
{
    int i, n = sizeof(HELP) / sizeof(HELP[0]);
    float x = g_w * 0.02f, y = g_h * 0.025f, lh = g_h * 0.030f * K;
    if (!ed.help) {
        rect(x, y, x + g_w * 0.17f * K, y + lh + g_h * 0.012f * K, 0, 0, 0, 150);
        text(x + g_w * 0.008f * K, y + g_h * 0.006f * K, 0.7f, 200, 200, 200, "H SHOW HELP");
        return;
    }
    rect(x, y, g_w - x, y + lh * n + g_h * 0.020f * K, 0, 0, 0, 165);
    rect(x, y, x + g_w * 0.004f, y + lh * n + g_h * 0.020f * K, 235, 125, 40, 230);
    for (i = 0; i < n; i++) {
        char line[200], *e = kput(line, HELP[i]);           /* configured key names */
        if (HELP[i][0] == 'S' && HELP[i][1] == 'A') {      /* SAVE line names the output */
            e = sput(e, ":  ");
            if (ed.prj[0]) dput(e, ed.prj, 48);
            else { e = dput(e, ed.base, 36); sput(e, ".edit.[DATE-TIME].prj"); }
        } else if (HELP[i][16] == 3) {                     /* SAVE AS line names the new file */
            e = sput(e, ":  ");
            e = dput(e, ed.base, 36); sput(e, ".edit.[DATE-TIME].prj");
        }
        text(x + g_w * 0.014f * K, y + g_h * 0.010f * K + lh * i, 0.7f, 225, 225, 225, line);
    }
}

/* editor panel: status + info line (scaled), timeline with its markers (fixed) */
/* editor panel: status + info line (scaled), 3 track lanes + timeline (fixed) */
#define LANE_H  0.018f
#define LANE_DY 0.024f
/* lanes get closer with more tracks so that the panel stays below ~ 1/3 of the screen */
static float lane_dy(void) { return g_nsub <= 1 ? LANE_DY : g_nsub <= 4 ? 0.019f : 0.015f; }
static float lane_h(void) { return g_nsub <= 1 ? LANE_H : g_nsub <= 4 ? 0.015f : 0.012f; }
static float panel_top(void) { return g_h * (0.873f - (lane_dy() * (float)ntracks() - LANE_DY * 3.0f) - 0.080f * K); }

static float frame_x(float f, float x0, float x1) { return x0 + (x1 - x0) * (nfr > 1 ? f / (float)(nfr - 1) : 0.0f); }

static void lerp_col(u8 *o, const u8 *a, const u8 *b, float u)
{
    int k;
    for (k = 0; k < 3; k++) o[k] = (u8)((float)a[k] + ((float)b[k] - (float)a[k]) * u);
}
static void time_col(u8 *o, float m)                    /* night blue .. day yellow */
{
    static const u8 NIGHT[3] = { 40, 50, 120 }, DAY[3] = { 255, 215, 90 };
    float h = m / 60.0f, d;
    d = h < 5.0f || h >= 21.0f ? 0.0f : h < 8.0f ? (h - 5.0f) / 3.0f : h < 18.0f ? 1.0f : (21.0f - h) / 3.0f;
    lerp_col(o, NIGHT, DAY, d);
}

/* one lane: value-coloured segments (gradient when SMOOTH) and key markers */
static void draw_lane(int tr, float x0, float x1, float y, int selected)
{
    int i, n, j, sel;
    float yb = y + g_h * lane_h();
    u8 c0[3], c1[3], c[3];
    rect(x0, y, x1, yb, 40, 40, 40, selected ? 230 : 170);
    if (tr >= TR_SUB) {                                    /* sub span (purple), main span (blue line) */
        int a = -1, b = -1, k = tr - TR_SUB;
        for (i = 0; i < nfr && k < g_nsub; i++) if (ps[k][i] >= 0) { if (a < 0) a = i; b = i; }
        if (k < g_nsub && a >= 0)
            rect(frame_x((float)a, x0, x1), y + g_h * 0.003f, frame_x((float)b, x0, x1), yb - g_h * 0.005f, 192, 38, 211, 220);
        rect(frame_x((float)g_pre, x0, x1), yb - g_h * 0.004f, frame_x((float)(g_pre + mnf - 1), x0, x1), yb, 90, 140, 230, 230);
        return;
    }
    n = tr == TR_CAM ? nkf : nek[tr - 1];
    sel = tr == TR_CAM ? ed.sel : esel[tr - 1];
    for (i = 0; i < n; i++) {
        int fa = tr == TR_CAM ? kf[i].frame : ek[tr - 1][i].frame;
        int fb = i + 1 < n ? (tr == TR_CAM ? kf[i + 1].frame : ek[tr - 1][i + 1].frame) : nfr - 1;
        int ip = tr == TR_CAM ? kf[i].cut : ek[tr - 1][i].cut, last = i + 1 >= n, cut = ip == IP_CUT;
        float kx = frame_x((float)fa, x0, x1), nx = frame_x((float)fb, x0, x1);
        if (tr == TR_CAM) {
            const u8 *kc = KF_COL[kf[i].type];
            rect(kx, y + g_h * 0.005f, nx, yb - g_h * 0.005f, kc[0], kc[1], kc[2], cut ? 90 : 200);
            if (!cut && !last && ip != IP_SMOOTH)          /* speed bar: bright = fast, dark = slow */
                for (j = 0; j < 8; j++) {
                    float sp = ease_speed(ip, ((float)j + 0.5f) / 8.0f) / 1.5f;
                    int v = 40 + (int)(215.0f * fclamp(sp, 0.0f, 1.0f));
                    rect(kx + (nx - kx) * (float)j / 8.0f, yb - g_h * 0.0045f, kx + (nx - kx) * (float)(j + 1) / 8.0f, yb - g_h * 0.0015f, (u8)v, (u8)v, (u8)v, 230);
                }
            c[0] = kc[0]; c[1] = kc[1]; c[2] = kc[2];
        } else {
            const EKey *a = &ek[tr - 1][i], *b = last ? a : &ek[tr - 1][i + 1];
            for (j = 0; j < 8; j++) {                      /* 8 steps of the segment */
                float u0 = (float)j / 8.0f, ua = cut || last ? 0.0f : ease(ip, u0 + 0.0625f);
                if (tr == TR_WEATHER) lerp_col(c, WEATHER_COL[a->val], WEATHER_COL[b->val], ua);
                else time_col(c, cut || last ? (float)a->val : time_lerp(a->val, b->val, ua));
                rect(kx + (nx - kx) * u0, y + g_h * 0.004f, kx + (nx - kx) * (u0 + 0.125f), yb - g_h * 0.004f, c[0], c[1], c[2], 210);
            }
            if (tr == TR_WEATHER) { c0[0] = WEATHER_COL[a->val][0]; c0[1] = WEATHER_COL[a->val][1]; c0[2] = WEATHER_COL[a->val][2]; }
            else time_col(c0, (float)a->val);
            c[0] = c0[0]; c[1] = c0[1]; c[2] = c0[2];
            (void)c1;
        }
        if (i == sel && selected) rect(kx - 6.0f, y - g_h * 0.003f, kx + 6.0f, yb + g_h * 0.003f, 255, 255, 255, 255);
        rect(kx - 4.0f, y, kx + 4.0f, yb, c[0], c[1], c[2], 255);
    }
}

static void draw_editor_hud(void)
{
    static const char *CAMTXT[3] = { "CAM RECORDED", "CAM KEYFRAMES", "CAM FREE" };
    static char line[400];
    char *e;
    int f = shown_frame(), i, at, tr;
    float x0, x1, y0, y1, cx, top, ly;
    u32 dur;
    if (f < 0) f = 0;
    dur = nfr > 0 ? fr_time[nfr - 1] - fr_time[0] : 0;

    draw_help();

    top = panel_top();
    rect(0, top, g_w, g_h, 0, 0, 0, 150);
    e = sput(line, ed.paused ? "PAUSED   " : "PLAYING   ");
    e = tput(e, fr_time[f] - fr_time[0]); e = sput(e, " / "); e = tput(e, dur);
    e = sput(e, "   "); e = sput(e, SPEED_TXT[ed.speed]);
    e = sput(e, "   FRAME "); e = iput(e, f + 1, 0); e = sput(e, " / "); e = iput(e, nfr, 0);
    e = sput(e, "   "); e = sput(e, CAMTXT[ed.cam_shown]);
    e = sput(e, "   "); e = sput(e, WEATHER_NAME[cur_weather()]);
    e = sput(e, "   "); e = hhmm(e, cur_minute());
    if (g_nsub && f < nfr && pm[f] < 0) {                 /* no main here: the camera stays on it */
        Pose pp;
        if (!eval_path((float)f, &pp)) e = sput(e, "   NO MAIN HERE: ADD A CAMERA KEY");
    }
    if (ed.dirty) sput(e, "   * UNSAVED");
    text(g_w * 0.05f, top + g_h * 0.012f * K, 0.9f, 255, 255, 255, line);

    e = sput(line, "START "); e = tput(e, fr_time[ed.in] - fr_time[0]);
    e = sput(e, "  END "); e = tput(e, fr_time[ed.out] - fr_time[0]);
    e = sput(e, "  LENGTH "); e = tput(e, fr_time[ed.out] - fr_time[ed.in]);
    if (ed.track == TR_CAM) {
        at = key_at_frame(f);
        e = sput(e, "      KEYFRAMES "); e = iput(e, nkf, 0);
        if (ed.sel >= 0 && ed.sel < nkf) {
            e = sput(e, "    SELECTED "); e = iput(e, ed.sel + 1, 0);
            e = sput(e, ": FRAME "); e = iput(e, kf[ed.sel].frame + 1, 0);
            e = sput(e, "  "); e = sput(e, KF_NAME[kf[ed.sel].type]);
            e = sput(e, "  "); e = sput(e, ip_name(kf[ed.sel].cut, 0));
        }
        if (at >= 0) e = sput(e, "    [ON KEYFRAME]");
        if (ed.freecam) { e = sput(e, "    FOV "); iput(e, (int)(ed.fc.fov + 0.5f), 0); }
    } else if (ed.track >= TR_SUB) {
        int k = ed.track - TR_SUB;
        e = sput(e, "      "); e = sput(e, track_name(ed.track)); e = sput(e, ": ");
        if (k < g_nsub) {
            Sub *S = &subs[k];
            e = dput(e, S->name, 30); e = sput(e, "    START "); e = offs_put(e, S->off);
            e = sput(e, "    LOOKS: "); dput(e, S->looks[0] ? S->looks : "NONE", 60);
        }
        else sput(e, "NONE    N: ADD A SUB REPLAY");
    } else {
        int t2 = ed.track - 1, s2 = esel[t2];
        e = sput(e, "      "); e = sput(e, TR_NAME[ed.track]); e = sput(e, " KEYS "); e = iput(e, nek[t2], 0);
        if (s2 >= 0 && s2 < nek[t2]) {
            e = sput(e, "    SELECTED "); e = iput(e, s2 + 1, 0);
            e = sput(e, ": FRAME "); e = iput(e, ek[t2][s2].frame + 1, 0); e = sput(e, "  ");
            e = t2 == 0 ? sput(e, WEATHER_NAME[ek[t2][s2].val]) : hhmm(e, ek[t2][s2].val);
            e = sput(e, "  "); sput(e, ip_name(ek[t2][s2].cut, 1));
        }
    }
    text(g_w * 0.05f, top + g_h * 0.045f * K, 0.75f, 210, 210, 210, line);

    /* track lanes */
    x0 = g_w * 0.12f; x1 = g_w * 0.97f;
    ly = top + g_h * (0.080f * K + 0.012f);
    for (tr = 0; tr < ntracks(); tr++) {
        float y = ly + g_h * lane_dy() * (float)tr;
        int on = tr == ed.track;
        if (on) rect(g_w * 0.015f, y - g_h * 0.002f, g_w * 0.018f, y + g_h * (lane_h() + 0.002f), 235, 125, 40, 255);
        text(g_w * 0.025f, y + g_h * 0.001f, g_nsub > 1 ? 0.55f : 0.7f, on ? 255 : 150, on ? 170 : 150, on ? 70 : 150, track_name(tr));
        draw_lane(tr, x0, x1, y, on);
    }
    /* timeline bar */
    y0 = ly + g_h * lane_dy() * (float)ntracks() + g_h * 0.004f; y1 = y0 + g_h * 0.015f;
    rect(x0, y0, x1, y1, 70, 70, 70, 220);
    cx = frame_x((float)f, x0, x1);
    rect(x0, y0, cx, y1, 235, 125, 40, 230);
    for (i = 1; i < nfr && !g_nsub; i++)                   /* buffer boundaries */
        if (fr_slot[i] != fr_slot[i - 1]) {
            float bx = frame_x((float)i, x0, x1);
            rect(bx - 1.0f, y1, bx + 1.0f, y1 + g_h * 0.006f, 160, 160, 160, 200);
        }
    {   /* trim: darken outside, start (green) / end (red) marks over all lanes */
        float ix = frame_x((float)ed.in, x0, x1), ox = frame_x((float)ed.out, x0, x1), ty = ly - g_h * 0.004f;
        if (ed.in > 0) rect(x0, ty, ix, y1 + g_h * 0.004f, 0, 0, 0, 170);
        if (ed.out < nfr - 1) rect(ox, ty, x1, y1 + g_h * 0.004f, 0, 0, 0, 170);
        rect(ix - 2.0f, ty, ix + 2.0f, y1 + g_h * 0.008f, 80, 230, 80, 255);
        rect(ix, ty, ix + 9.0f, ty + g_h * 0.006f, 80, 230, 80, 255);
        rect(ox - 2.0f, ty, ox + 2.0f, y1 + g_h * 0.008f, 240, 70, 70, 255);
        rect(ox - 9.0f, ty, ox, ty + g_h * 0.006f, 240, 70, 70, 255);
    }
    rect(cx - 1.5f, ly - g_h * 0.006f, cx + 1.5f, y1 + g_h * 0.006f, 255, 255, 255, 255);   /* playhead */
}

static char *date_put(char *d, const u32 *ft)     /* yyyy-mm-dd hh:mm local time */
{
    u32 loc[2];
    u16 st[8];
    if (!pFileTimeToLocalFileTime || !pFileTimeToSystemTime
        || !pFileTimeToLocalFileTime(ft, loc) || !pFileTimeToSystemTime(loc, st)) { *d = 0; return d; }
    d = iput(d, st[0], 4); *d++ = '-'; d = iput(d, st[1], 2); *d++ = '-'; d = iput(d, st[3], 2);
    *d++ = ' '; d = iput(d, st[4], 2); *d++ = ':'; return iput(d, st[5], 2);
}

static void draw_menu(void)
{
    static const char *ITEMS[MENU_ITEMS] = { "OPEN IN REPLAY EDITOR", "PLAY REPLAY", "DELETE PROJECT" };
    static char title3[60];
    static const char *TITLES[MENU_ITEMS + 1] = { "OPEN IN REPLAY EDITOR - SELECT A FILE", "PLAY REPLAY - SELECT A FILE",
                                              "DELETE PROJECT - SELECT A .prj FILE", title3 };
    char line[200], *e;
    float x0 = g_w * (0.5f - 0.40f * K), x1 = g_w * (0.5f + 0.40f * K), y0 = g_h * 0.30f, lh = g_h * 0.036f * K, y, y1;
    int i;
    if (menu.page == 0) {
        y1 = y0 + lh * (3.0f + MENU_ITEMS * 1.1f);
        rect(x0, y0, x1, y1, 0, 0, 0, 200);
        rect(x0, y0, x0 + g_w * 0.004f, y1, 235, 125, 40, 240);
        text(x0 + g_w * 0.025f * K, y0 + lh * 0.4f, 1.0f, 235, 125, 40, "REPLAY EDITOR");
        text(x1 - g_w * 0.045f * K, y0 + lh * 0.55f, 0.6f, 170, 170, 170, RE_VERSION);
        for (i = 0; i < MENU_ITEMS; i++) {
            y = y0 + lh * (1.7f + i * 1.1f);
            if (menu.sel == i) rect(x0 + g_w * 0.015f * K, y - lh * 0.15f, x1 - g_w * 0.015f * K, y + lh * 0.95f, 235, 125, 40, 200);
            text(x0 + g_w * 0.030f * K, y, 0.9f, 255, 255, 255, ITEMS[i]);
        }
        kput(line, "UP/DOWN SELECT     ENTER OK     BACKSPACE / \1 CLOSE");
        text(x0 + g_w * 0.025f * K, y0 + lh * (1.9f + MENU_ITEMS * 1.1f), 0.65f, 170, 170, 170, line);
        return;
    }
    y1 = y0 + lh * (MENU_ROWS + 5.7f);
    rect(x0, y0, x1, y1, 0, 0, 0, 205);
    rect(x0, y0, x0 + g_w * 0.004f, y1, 235, 125, 40, 240);
    {
        char *t = sput(title3, g_pick_slot < g_nsub ? "REPLACE " : "ADD ");
        t = subname_put(t, g_pick_slot); sput(t, " - SELECT A .rep FILE");
    }
    text(x0 + g_w * 0.025f * K, y0 + lh * 0.4f, 1.0f, 235, 125, 40, TITLES[menu.mode]);
    text(x1 - g_w * 0.045f * K, y0 + lh * 0.55f, 0.6f, 170, 170, 170, RE_VERSION);
    if (nfiles == 0)
        text(x0 + g_w * 0.030f * K, y0 + lh * 1.8f, 0.8f, 255, 210, 120, menu.mode == 3 ? "NO .rep FILES IN THE USER FOLDER" : menu.mode == 2 ? "NO .prj FILES IN THE USER FOLDER"
             : "NO .rep OR .prj FILES IN THE USER FOLDER (F2 SAVES replay.rep THERE)");
    for (i = 0; i < MENU_ROWS && menu.top + i < nfiles; i++) {
        const FileEnt *f = &files[menu.top + i];
        int idx = menu.top + i;
        u8 c = f->prj && !f->prj_ok ? 140 : 255;
        y = y0 + lh * (1.6f + i);
        if (idx == menu.sel) rect(x0 + g_w * 0.015f * K, y - lh * 0.1f, x1 - g_w * 0.015f * K, y + lh * 0.9f, 235, 125, 40, 200);
        dput(line, f->name, 46);                                  /* wider name column */
        text(x0 + g_w * 0.030f * K, y, 0.75f, c, c, c, line);
        e = iput(line, (int)((f->size + 1023) / 1024), 0); sput(e, " KB");   /* rounded up */
        text(x0 + g_w * 0.480f * K, y, 0.75f, c, c, c, line);
        date_put(line, f->ft);
        text(x0 + g_w * 0.600f * K, y, 0.75f, 190, 190, 190, line);
    }
    if (nfiles > MENU_ROWS) {                                       /* file count line */
        e = iput(line, menu.top + 1, 0); *e++ = '-'; e = iput(e, menu.top + MENU_ROWS < nfiles ? menu.top + MENU_ROWS : nfiles, 0);
        e = sput(e, " / "); iput(e, nfiles, 0);
        text(x0 + g_w * 0.025f * K, y0 + lh * (MENU_ROWS + 2.2f), 0.65f, 170, 170, 170, line);
    }
    kput(line, menu.mode == 3 ? "UP/DOWN/PGUP/PGDN SELECT     ENTER: USE THIS REPLAY     BACKSPACE / \1 CANCEL"
             : menu.mode == 2 ? "UP/DOWN/PGUP/PGDN SELECT     ENTER DELETE     BACKSPACE BACK     \1 CLOSE"
                              : "UP/DOWN/PGUP/PGDN SELECT     ENTER OPEN     BACKSPACE BACK     \1 CLOSE");
    text(x0 + g_w * 0.025f * K, y0 + lh * (MENU_ROWS + 3.1f), 0.65f, 170, 170, 170, line);   /* key line below it */
    if (menu.mode == 3) return;
    if (menu.mode == 2) {
        if ((int)(menu.note_until - now_ms()) > 0)
            text(x0 + g_w * 0.025f * K, y0 + lh * (MENU_ROWS + 4.1f), 0.7f, 120, 230, 120, menu.note);
        if (menu.del_confirm && menu.sel < nfiles) {               /* confirm box over the list */
            float bx0 = g_w * (0.5f - 0.30f * K), bx1 = g_w * (0.5f + 0.30f * K);
            float by0 = y0 + lh * 4.0f, by1 = by0 + lh * 4.4f;
            rect(bx0, by0, bx1, by1, 20, 20, 20, 245);
            rect(bx0, by0, bx0 + g_w * 0.004f, by1, 255, 90, 90, 255);
            text(bx0 + g_w * 0.020f * K, by0 + lh * 0.5f, 0.9f, 255, 90, 90, "DELETE THIS PROJECT?");
            dput(line, files[menu.sel].name, 60);
            text(bx0 + g_w * 0.020f * K, by0 + lh * 1.7f, 0.8f, 255, 255, 255, line);
            text(bx0 + g_w * 0.020f * K, by0 + lh * 3.0f, 0.7f, 200, 200, 200, "ENTER: DELETE     BACKSPACE: CANCEL");
        }
        return;
    }
    if ((int)(menu.confirm_until - now_ms()) > 0)
        text(x0 + g_w * 0.025f * K, y0 + lh * (MENU_ROWS + 4.1f), 0.7f, 255, 90, 90,
             "MISSION IN PROGRESS - A REPLAY MAY BREAK IT.   PRESS ENTER AGAIN TO LOAD ANYWAY");
    else if (on_mission())
        text(x0 + g_w * 0.025f * K, y0 + lh * (MENU_ROWS + 4.1f), 0.7f, 255, 170, 90,
             "WARNING: A MISSION IS IN PROGRESS - LOADING A REPLAY MAY BREAK IT");
}

static void __cdecl hk_display(void)
{
    if (!g_installed) { ((vfn)orig_display)(); return; }
    g_w = (float)GI32(A_SCREEN_W);
    g_h = (float)GI32(A_SCREEN_H);
    if (ed.active && !ed.viewer && G8(A_MODE) == 1) draw_editor_hud();
    else ((vfn)orig_display)();
    if (menu.open) draw_menu();
    if (message_visible() && ed.msg_toast && ed.active) {   /* short editor feedback above the panel */
        float y1 = (ed.viewer ? g_h : panel_top()) - g_h * 0.012f, y0 = y1 - g_h * 0.045f * K;
        float x0 = g_w * (0.5f - 0.32f * K), x1 = g_w * (0.5f + 0.32f * K);
        rect(x0, y0, x1, y1, 0, 0, 0, 190);
        rect(x0, y0, x0 + g_w * 0.004f, y1, 120, 230, 120, 255);
        text(x0 + g_w * 0.015f * K * K, y0 + g_h * 0.010f * K, 0.8f, 255, 255, 255, ed.msg);
    } else if (message_visible() && !ed.msg_toast) {
        rect(0, g_h * 0.45f, g_w, g_h * (0.45f + 0.08f * K), 0, 0, 0, 170);
        text(g_w * 0.05f, g_h * (0.45f + 0.015f * K), 1.0f, 255, 210, 120, ed.msg);
    }
}

/* ------------------------------------------------------------------ init */
/* ---- crash diagnostics: on an unhandled exception, log the registers and the
   game code addresses on the stack (the call path), then let the game's own
   handler run. */
typedef long (__stdcall *UEFilterFn)(void *);
typedef UEFilterFn (__stdcall *SetUEFFn)(UEFilterFn);
typedef int (__stdcall *IsBadReadPtrFn)(const void *, u32);
static UEFilterFn prev_filter;
static IsBadReadPtrFn pIsBadReadPtr;
static long __stdcall crash_filter(void *ep)
{
    static char line[1600];
    u32 *rec = *(u32 **)ep, *ctx = ((u32 **)ep)[1], esp, ebp, *sp;
    char *e = line;
    int i, n = 0;
    e = sput(e, "CRASH code "); e = hput(e, rec[0]); e = sput(e, " at "); e = hput(e, rec[3]);
    e = sput(e, "  eax "); e = hput(e, ctx[0xB0 / 4]); e = sput(e, " ebx "); e = hput(e, ctx[0xA4 / 4]);
    e = sput(e, " ecx "); e = hput(e, ctx[0xAC / 4]); e = sput(e, " edx "); e = hput(e, ctx[0xA8 / 4]);
    e = sput(e, " esi "); e = hput(e, ctx[0xA0 / 4]); e = sput(e, " edi "); e = hput(e, ctx[0x9C / 4]);
    e = sput(e, " ebp "); e = hput(e, ctx[0xB4 / 4]); e = sput(e, " esp "); e = hput(e, ctx[0xC4 / 4]);
    logs(line);
    esp = ctx[0xC4 / 4]; ebp = ctx[0xB4 / 4];
    e = sput(line, "CRASH stack code addresses:");
    for (i = 0; i < 1024 && n < 48; i++) {            /* every dword on the stack that points into game code */
        sp = (u32 *)(esp + i * 4);
        if (pIsBadReadPtr && pIsBadReadPtr(sp, 4)) break;
        if (*sp >= 0x401000 && *sp < 0x6F2000) { *e++ = ' '; e = hput(e, *sp); n++; }
    }
    logs(line);
    e = sput(line, "CRASH ebp chain:");
    for (i = 0; i < 24 && ebp; i++) {
        if (pIsBadReadPtr && pIsBadReadPtr((void *)ebp, 8)) break;
        *e++ = ' '; e = hput(e, ((u32 *)ebp)[1]);
        ebp = ((u32 *)ebp)[0];
    }
    logs(line);
    {
        char *e2 = sput(line, "CRASH state: copies ");
        int k;
        for (k = 0; k < nlk_dff; k++) { e2 = sput(e2, lk_dff[k].name); *e2++ = '='; e2 = iput(e2, lk_dff[k].clone, 0); *e2++ = ' '; }
        sput(e2, g_installed ? "(installed)" : "");
        logs(line);
    }
    return prev_filter ? prev_filter(ep) : 0;
}

/* game exit with the editor (or a project's playback) still open: 0x4A5B8E
   calls CGame::ShutDown 0x4A5110, then RsRwTerminate 0x602ED0. Before the
   shutdown the editor is closed the normal way: FinishPlayback deletes the
   replay entities while the model copies still exist, then the copies' RW
   objects go (leftovers would be destroyed after MatFX is closed -> crash) and
   the game's car colours come back. */
#define SITE_RWTERM 0x4A5B8E
static u32 orig_rwterm;
static void __cdecl hk_rwterm(void)
{
    if (ed.active || ed.viewer || G8(A_MODE) == 1 || nlk_dff) {
        logs("game exit with the editor open: closing it first");
        g_win = 0; g_nsub = 0;
        looks_unload();
        ed.active = 0; ed.viewer = 0;
        G32(A_DIVISOR) = 1;
        G32(A_INTERP_CNT) = 0;
        if (G8(A_MODE) == 1) ((vfn)F_FINISH)();
        dff_unregister();
    }
    if (orig_rwterm) ((vfn)orig_rwterm)();
}

__declspec(dllexport) int __cdecl RE_Init(void)
{
    void *k32, *u32m;
    GetModuleHandleAFn gmh;
    GetProcAddressFn gpa;
    if (g_installed) return 2;
    gmh = *(GetModuleHandleAFn *)IAT_GetModuleHandleA;
    gpa = *(GetProcAddressFn *)IAT_GetProcAddress;
    k32 = gmh("kernel32.dll");
    u32m = gmh("user32.dll");
    if (!k32 || !u32m) return -1;
    pVirtualProtect = (VirtualProtectFn)gpa(k32, "VirtualProtect");
    pCreateFileA = (CreateFileAFn)gpa(k32, "CreateFileA");
    pWriteFile = (WriteFileFn)gpa(k32, "WriteFile");
    pFlushFileBuffers = (FlushFileBuffersFn)gpa(k32, "FlushFileBuffers");
    pGetAsyncKeyState = (GetAsyncKeyStateFn)gpa(u32m, "GetAsyncKeyState");
    pTimeGetTime = *(TimeFn *)IAT_timeGetTime;
    pReadFile = (ReadFileFn)gpa(k32, "ReadFile");
    pCloseHandle = (CloseHandleFn)gpa(k32, "CloseHandle");
    pVirtualAlloc = (VirtualAllocFn)gpa(k32, "VirtualAlloc");
    pFindFirstFileA = (FindFirstFn)gpa(k32, "FindFirstFileA");
    pFindNextFileA = (FindNextFn)gpa(k32, "FindNextFileA");
    pFindClose = (FindCloseFn)gpa(k32, "FindClose");
    pGetFileSize = (GetFileSizeFn)gpa(k32, "GetFileSize");
    pDeleteFileA = (DeleteFileAFn)gpa(k32, "DeleteFileA");
    pGetLocalTime = (GetLocalTimeFn)gpa(k32, "GetLocalTime");
    pIsBadReadPtr = (IsBadReadPtrFn)gpa(k32, "IsBadReadPtr");
    pVirtualFree = (VirtualFreeFn)gpa(k32, "VirtualFree");
    {
        SetUEFFn set = (SetUEFFn)gpa(k32, "SetUnhandledExceptionFilter");
        if (set) prev_filter = set(crash_filter);
    }
    {   /* [Display] FONT_SCALE (percent) from <game>\CLEO\ReplayEditor.ini */
        typedef u32 (__stdcall *GetModuleFileNameAFn)(void *, char *, u32);
        typedef u32 (__stdcall *GetPrivateProfileIntAFn)(const char *, const char *, int, const char *);
        GetModuleFileNameAFn gmf = (GetModuleFileNameAFn)gpa(k32, "GetModuleFileNameA");
        GetPrivateProfileIntAFn gpi = (GetPrivateProfileIntAFn)gpa(k32, "GetPrivateProfileIntA");
        static char ini[300];
        int n, v;
        if (gmf && gpi && (n = (int)gmf(0, ini, 260)) > 0 && n < 260) {
            while (n > 0 && ini[n - 1] != '\\' && ini[n - 1] != '/') n--;
            sput(ini + n, "CLEO\\ReplayEditor.ini");
            v = (int)gpi("Display", "FONT_SCALE", 60, ini);
            if (v < 40) v = 40;
            if (v > 150) v = 150;
            g_fs = (float)v / 100.0f;
            {   /* [Keys] MENU / SAVE / SAVE_AS = F4 .. F12 */
                typedef u32 (__stdcall *GetPrivateProfileStringAFn)(const char *, const char *, const char *, char *, u32, const char *);
                GetPrivateProfileStringAFn gps = (GetPrivateProfileStringAFn)gpa(k32, "GetPrivateProfileStringA");
                static const char *NAMES[NKEYS] = { "MENU", "SAVE", "SAVE_AS" };
                char val[32];
                int k, j, vk;
                for (k = 0; gps && k < NKEYS; k++) {
                    val[0] = 0;
                    gps("Keys", NAMES[k], g_keyname[k], val, sizeof(val), ini);
                    vk = parse_fkey(val);
                    if (!vk) { g_keybad |= 1 << k; continue; }        /* keep the default */
                    g_key[k] = vk;
                    g_keyname[k][0] = 'F';
                    if (vk >= 0x79) { g_keyname[k][1] = '1'; g_keyname[k][2] = (char)('0' + vk - 0x79); g_keyname[k][3] = 0; }
                    else { g_keyname[k][1] = (char)('0' + vk - 0x6F); g_keyname[k][2] = 0; }
                }
                for (k = 0; k < NKEYS; k++)
                    for (j = k + 1; j < NKEYS; j++) if (g_key[k] == g_key[j]) g_keydup = 1;
            }
        }
    }
    pFileTimeToLocalFileTime = (FtFn)gpa(k32, "FileTimeToLocalFileTime");
    pFileTimeToSystemTime = (FtFn)gpa(k32, "FileTimeToSystemTime");
    if (!pVirtualProtect || !pGetAsyncKeyState) return -2;
    if (pCreateFileA)   /* GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS, NORMAL */
        g_log = pCreateFileA("CLEO\\ReplayEditor\\ReplayEditor.log", 0x40000000, 1, 0, 2, 0x80, 0);
    logs("Replay Editor native module (F6 menu) init");
    {
        char line[160], *e = sput(line, "keys: MENU="); e = sput(e, g_keyname[KEY_MENU]);
        e = sput(e, " SAVE="); e = sput(e, g_keyname[KEY_SAVE]); e = sput(e, " SAVE_AS="); e = sput(e, g_keyname[KEY_SAVEAS]);
        if (g_keybad) e = sput(e, "  (invalid [Keys] value: default used, allowed F4 - F12)");
        if (g_keydup) sput(e, "  ERROR: duplicate key, the menu will not open");
        logs(line);
    }
    ed.help = 0;                                           /* help box closed by default (H) */
    orig_update = hook_call(SITE_UPDATE, (void *)hk_update, "CReplay::Update");
    orig_playback = hook_call(SITE_PLAYBACK, (void *)hk_playback, "PlaybackThisFrame");
    orig_lookaround = hook_call(SITE_LOOKAROUND, (void *)hk_lookaround, "ProcessLookAroundCam");
    orig_display = hook_call(SITE_DISPLAY, (void *)hk_display, "CReplay::Display");
    orig_stdcam = hook_call(SITE_STDCAM, (void *)hk_stdcam, "ShouldStandardCameraBeProcessed");
    install_skin_patch();
    hook_call(SITE_DEFER1, (void *)deferred_render, "deferred atomics 1");
    hook_call(SITE_DEFER2, (void *)deferred_render, "deferred atomics 2");
    orig_addpart = hook_call(SITE_ADDPART, (void *)hk_addpart, "replay particle creation");
    orig_rwterm = hook_call(SITE_RWTERM, (void *)hk_rwterm, "game exit (before RenderWare closes)");
    if (orig_addpart) install_partfade();
    if (!orig_update || !orig_playback || !orig_lookaround || !orig_display || !orig_stdcam) { logs("init incomplete"); return -3; }
    g_installed = 1;
    return 1;
}

int __stdcall _DllMainCRTStartup(void *h, u32 reason, void *reserved)
{
    (void)h; (void)reason; (void)reserved;
    return 1;
}
