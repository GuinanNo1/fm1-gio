/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* GIO-1 app: the project (everything that is saved), the app's entry points and its half of the contract with
 * fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "gio.h"

#define APP_NAME "GIO-1"
#define APP_TAGLINE "ELECTRONIC DISCO"
#define APP_SPLASH_RGB RGB(255, 72, 170)  /* (fm1-base's own start-up screen: GIO-1 has its own, below) */
#define APP_SPLASH                       /* app_splash (intro.c): the start-up screen's background, then the intro */
void app_splash(void);
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#ifdef APP_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION APP_VERSION
#else
#define EN_VERSION "DEV"
#endif

/* the store: the project and the panel calibration (where AMB-1 keeps them: another firmware's project there is
 * not GIO-1's, by its magic, and gives the defaults) */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}
#define PROJ_MAGIC 0x314F4947u           /* "GIO1" */
#define PROJ_MAGIC_OLD 0x31524F4Du       /* the same instrument before its name was GIO-1: taken up as GIO-1's */
#define PROJ_FORMAT 3u                   /* 2: the lead's echo, reverb and tone; ten lead voices; 3: the chords */
#define NPIECES EN_NSONGS

typedef struct {
    int16_t par[P_NPARAMS];
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the song playing, as it is now (gio.h P_*) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which song */
    uint8_t rsv0[2];
    slot_t slot[NPIECES];                /* every song as it was left (Tune and MIDI out are global) */
    uint8_t rsv[16];                     /* room to grow */
    uint16_t chords[NPIECES][EN_NPROG * EN_PROG_MAX];   /* every song's chord changes (gio.h EN_CH), as edited */
} project_t;
#define PROJ_V2_SIZE (sizeof(project_t) - sizeof(((project_t *)0)->chords))   /* format 2 had no chords */

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine, then the song from the top */
const char *piece_name(int i);
void project_piece(int i);               /* the song playing kept, song i taken up as it was left */
void project_piece_reset(void);          /* the song playing back to its recipe */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
#define APP_REDRAW                       /* the screen mirror (fm1.toml mirror = true): everything drawn again */
void app_redraw(void);

/* the host simulator's part (app/host.c): GIO-1's checks, its commands, the screen reader's dump */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
#define APP_HOST_REBOOT
void app_host_reboot(void);
