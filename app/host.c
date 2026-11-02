/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* GIO-1 in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): GIO-1's checks for
 * "expect", its command, a reboot and the screen reader's dump.
 *
 * GIO-1's checks: playing held view piece dirty sixteenths ext section bar layers key chordroot pad_level fx leadstep parN
 * nchords chordN (the section's progression: its length, chord N packed as gio.h's EN_CH) cslot progslot
 * battery (fm1-base's own: store_writes clocks_out midi_out peak_db_max peak_db_min). Its commands: battery N (the
 * battery read as N, plat.h's plat_battery: -1..4; "battery off" the simulator's own, full); saved_as old|v2 (the project
 * saved now as an older firmware left it: under the instrument's old name, or format 2, before the chords). */

int app_expect(const char *what, int *got)
{
    int g;
    if (!strcmp(what, "playing"))
        g = en_playing;
    else if (!strcmp(what, "held"))
        g = en_held;
    else if (!strcmp(what, "view"))
        g = ui.view;
    else if (!strcmp(what, "piece"))
        g = proj.piece;
    else if (!strcmp(what, "battery"))
        g = batt();
    else if (!strcmp(what, "dirty"))
        g = ui.dirty;
    else if (!strcmp(what, "sixteenths"))
        g = (int)en_bars;
    else if (!strcmp(what, "ext"))
        g = en_ext;
    else if (!strcmp(what, "section"))
        g = en_sec;
    else if (!strcmp(what, "bar"))
        g = en_sec_bar;
    else if (!strcmp(what, "layers"))
        g = en_layers;
    else if (!strcmp(what, "key"))
        g = en_key;
    else if (!strcmp(what, "chordroot"))
        g = en_chord_root;
    else if (!strcmp(what, "fx"))                /* the effects on now, as a mask */
        g = en_fx_on;
    else if (!strcmp(what, "leadstep"))          /* the white key the lead plays, -1 none */
        g = en_lead_step;
    else if (!strcmp(what, "pad_level"))
        g = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "nchords"))
        g = ccount();
    else if (!strcmp(what, "cslot"))             /* the Chords page's chord edited */
        g = cslot();
    else if (!strcmp(what, "progslot"))          /* the chord playing in its progression */
        g = en_prog_slot;
    else if (!strncmp(what, "chord", 5) && what[5] >= '0' && what[5] <= '7')
        g = *cslot_p(what[5] - '0');
    else if (!strncmp(what, "par", 3))           /* parN: parameter N (gio.h P_*) */
        g = proj.par[atoi(what + 3)];
    else
        return 0;
    *got = g;
    return 1;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "battery")) {               /* battery N: plat_battery as N (-1..4), "battery off" its own */
        batt_host = (int8_t)(strcmp(a, "off") ? atoi(a) : -2);
        return 1;
    }
    if (!strcmp(cmd, "saved_as")) {             /* saved_as old|v2: the project as an older firmware saved it */
        static project_t o;
        project_save();
        o = proj;
        if (!strcmp(a, "old")) {
            o.magic = PROJ_MAGIC_OLD;
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else {
            o.format = 2u;
            plat_store_save(OBJ_PROJ, &o, (uint32_t)PROJ_V2_SIZE);
        }
        return 1;
    }
    return 0;
}

void app_host_reboot(void) { memset(&proj, 0, sizeof proj); }

void app_speak_dump(void) { speak_dump(); }
