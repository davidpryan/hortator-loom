/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen capture for the colour-theme check: drives the firmware's own UI to every screen and dumps the framebuffer.
 *   cap OUTDIR PALETTE      (PALETTE: the stored COLOR id, e.g. 16 = THEME_TAG|0) */
#include "sim_core.c"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static const char *OUT;
static FILE *idx;
static uint32_t nshot;

static void run(uint32_t frames)
{
    while (frames) {
        uint32_t n = frames < SIM_MAX_FRAMES ? frames : SIM_MAX_FRAMES;
        sim_render(n);
        frames -= n;
    }
}

static void dump(const char *name)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%03u-%s.raw", OUT, nshot, name);
    f = fopen(path, "wb");
    fwrite(sim_fb(), 2, 240 * 240, f);
    fclose(f);
    fprintf(idx, "%03u-%s\n", nshot, name);
    nshot++;
}

static void redraw(void)                              /* everything drawn again from the state as it is */
{
    ui.force = 1;
    ui.graph_sig = ui.head_sig = ui.foot_sig = ui.col_sig = ui.lev_sig = ui.menu_sig = 0;
    ui.pr.sig = 0;
    nm.sig = 0;
}

static void shot(const char *fmt, ...)
{
    char name[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof name, fmt, ap);
    va_end(ap);
    redraw();
    run(FS / 10u);
    dump(name);
}

static void tap(uint32_t b)
{
    sim_btn(b, 1);
    run(FS * 12u / 100u);
    sim_btn(b, 0);
    run(FS / 10u);
}

static void page(uint32_t i)
{
    ui.home = 0;
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = (uint8_t)i;
    page_entered();
}

static uint32_t find_page(const char *title, uint32_t nth)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (!strcmp(PAGES[i].title, title) && !nth--)
            return i;
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t i, m, t, pal;
    char path[512];
    if (argc < 3)
        return 2;
    OUT = argv[1];
    pal = (uint32_t)atoi(argv[2]);
    snprintf(path, sizeof path, "%s/index.txt", OUT);
    idx = fopen(path, "w");

    sim_init(1);
    settings.palette = (uint8_t)pal;
    theme_set(settings.palette);
    run(FS / 2u);

    draw_boot_title();
    dump("boot-title");

    /* every page of the table, on track 1 as the demo has it */
    song.sel = 0;
    for (i = 0; i < NPAGES; i++) {
        if (page_id(&PAGES[i], 0) == 0xFFu && PAGES[i].fam == FAM_SND)
            continue;                                 /* a SOUND page past the model's list is never shown */
        page(i);
        shot("page%02u-%s", i, PAGES[i].title);
    }

    /* HOME's second mode and the COMP pages reached from it, by the panel */
    home_tracks();
    shot("home-tracks");
    home_screen();
    shot("home-screen");
    sim_enc(EN_SELECT, 1);
    shot("home-comp2");
    sim_enc(EN_SELECT, 1);
    shot("home-comp3");

    /* every track selected: TRACKS, SOUND 1, HOME */
    for (t = 0; t < NTRK; t++) {
        song.sel = (uint8_t)t;
        home_tracks();
        shot("trk%u-tracks", t + 1u);
        page(0);
        shot("trk%u-sound", t + 1u);
        page(find_page("STEP", 0));
        shot("trk%u-step", t + 1u);
        home_screen();
        shot("trk%u-home", t + 1u);
    }

    /* every drum model's SOUND pages (track 1) */
    song.sel = 0;
    {
        uint32_t keep = (uint32_t)TSEL->p[P_MODEL];
        for (m = 0; m < NMODELS; m++) {
            drum_set_model(TSEL, m);
            for (i = 0; i < 5u; i++) {
                if (page_id(&PAGES[i], 0) == 0xFFu)
                    continue;
                page(i);
                shot("model%02u-%s-sound%u", m, N_MODEL[m], i + 1u);
            }
        }
        drum_set_model(TSEL, keep);
    }

    /* a knob just turned (the hot column), on SOUND and FILTER */
    page(0);
    sim_enc(EN_K2, 2);
    shot("hot-sound-knob2");
    page(find_page("FILTER", 0));
    sim_enc(EN_K1, 1);
    shot("hot-filter-type-on");
    sim_enc(EN_K2, -6);
    shot("hot-filter-cut");
    page(find_page("FILTER", 1));
    shot("filter2-on");
    page(find_page("INSERT", 0));
    for (i = 0; i < 6u; i++) {
        sim_enc(EN_K1, 1);
        shot("insert-type%u", i + 1u);
    }
    page(find_page("RESON", 0));
    for (i = 0; i < 4u; i++) {
        sim_enc(EN_K1, 1);
        shot("reson-model%u", i + 1u);
    }
    page(find_page("SLICER", 0));
    sim_enc(EN_K1, 1);
    shot("slicer-on");
    page(find_page("LFO", 0));
    for (i = 0; i < 8u; i++) {
        sim_enc(EN_K1, 1);
        shot("lfo-wave%u", i + 1u);
    }
    page(find_page("GRIDS", 0));
    sim_enc(EN_K1, 1);
    shot("grids-mode2");
    page(find_page("GRIDS", 1));
    shot("grids-mode2-page2");
    page(find_page("GRIDS", 0));
    sim_enc(EN_K1, -1);
    page(find_page("REVERB", 0));
    sim_enc(EN_K1, 1);
    shot("reverb-type2");
    sim_enc(EN_K1, 1);
    shot("reverb-type3");

    /* popups over a page */
    page(0);
    ui.bpm_t = 40;
    shot("popup-tempo");
    ui.bpm_t = 0;
    run(FS / 10u);
    ui_message("SAVED A");
    shot("popup-message");
    ui.msg_t = 0;
    run(FS / 10u);
    sim_enc(EN_PRESET, 1);
    shot("popup-preset");
    ui.pr.pop = 0;
    run(FS / 10u);
    for (i = 1; i <= 8u; i++) {
        ui.confirm = (uint8_t)i;
        ui.confirm_trk = 0;
        shot("confirm%u", i);
    }
    ui.confirm = 0;
    run(FS / 10u);

    /* the PRESETS list */
    preset_list_open();
    shot("preset-list");
    sim_enc(EN_SELECT, 3);
    shot("preset-list-moved");
    pl_close();
    run(FS / 10u);

    /* NAME */
    name_open(NK_RENAME, 0);
    shot("name-editor");
    name_close();
    run(FS / 10u);

    /* the TOOLS page with an action armed */
    page(find_page("TOOLS", 0));
    sim_enc(EN_K1, 1);
    shot("tools-page-armed");
    ui.arm = 0, ui.arm_t = 0;

    /* key roles */
    page(0);
    keys_toggle();
    shot("keys-on");
    keys_toggle();
    tap(B_ENV);
    shot("env-steps-on");
    tap(B_ENV);
    ml_toggle();
    ui.msg_t = 0;
    shot("midi-learn-on");
    ml_toggle();
    ui.msg_t = 0;

    /* STEP: a step held (its locks), step record */
    page(find_page("STEP", 0));
    sim_key(4, 1);
    run(FS * 6u / 10u);
    shot("step-held");
    sim_enc(EN_K1, -3);
    shot("step-held-edited");
    sim_key(4, 0);
    run(FS / 10u);
    tap(B_REC);
    shot("step-record");
    tap(B_REC);

    /* held-button screens */
    page(0);
    sim_btn(B_FX, 1);
    run(FS * 8u / 10u);
    shot("perform");
    sim_key(1, 1);
    run(FS / 5u);
    shot("perform-repeat-held");
    sim_key(1, 0);
    sim_key(0, 1);
    run(FS / 5u);
    shot("perform-track-held");
    sim_key(0, 0);
    sim_enc(EN_K1, 4);
    sim_enc(EN_K4, -3);
    shot("perform-macros-turned");
    sim_btn(B_FX, 0);
    run(FS / 5u);
    perf_k[0] = perf_k[1] = perf_k[2] = 0, perf_k[3] = 100;

    sim_btn(B_GLO, 1);
    run(FS * 8u / 10u);
    shot("mute");
    sim_key(2, 1);
    run(FS / 10u);
    sim_key(2, 0);
    shot("mute-track2-muted");
    sim_key(2, 1);
    run(FS / 10u);
    sim_key(2, 0);
    sim_btn(B_GLO, 0);
    run(FS / 5u);

    sim_btn(B_SEQ, 1);
    run(FS * 8u / 10u);
    shot("tools-held");
    sim_btn(B_SEQ, 0);
    run(FS / 5u);

    sim_btn(B_SAVE, 1);
    run(FS * 8u / 10u);
    shot("cue-map-held");
    sim_btn(B_SAVE, 0);
    run(FS / 5u);

    /* playing, then recording */
    page(find_page("STEP", 0));
    tap(B_PLAY);
    run(FS);
    shot("play-step");
    page(find_page("PATTERN", 0));
    shot("play-pattern");
    home_screen();
    shot("play-home");
    home_tracks();
    shot("play-tracks");
    page(find_page("LFO", 0));
    shot("play-lfo");
    page(find_page("COMP", 0));
    shot("play-comp");
    sim_btn(B_FX, 1);
    run(FS * 8u / 10u);
    sim_key(1, 1);
    run(FS / 4u);
    shot("play-perform-repeat");
    sim_key(1, 0);
    sim_btn(B_FX, 0);
    run(FS / 5u);
    page(find_page("STEP", 0));
    tap(B_REC);
    run(FS / 2u);
    shot("play-rec");
    tap(B_REC);
    tap(B_PLAY);
    run(FS / 4u);

    /* MENU: the cursor on every row, ABOUT, Bluetooth */
    for (i = 0; i < MI_COUNT; i++) {
        ui.menu = 1;
        ui.menu_sel = (uint8_t)i;
        shot("menu-row%02u", i);
    }
    ui.menu = 2;
    ui.about_top = 0;
    shot("menu-about");
    ui.about_top = 4;
    shot("menu-about-scrolled");
#if FELUCCA_BLE
    ui.menu = 1;
    bt_page_open();
    shot("menu-bluetooth");
    bt_page_close();
#endif
    ui.menu = 0;
    run(FS / 10u);

    /* large values (MENU > ZOOM) */
    settings.zoom = 1;
    page(0);
    sim_enc(EN_K2, 1);
    shot("zoom-on-knob-turned");
    settings.zoom = 0;

    safe_start = 1;
    shot("safe-start");

    fclose(idx);
    printf("cap: %u screens -> %s\n", nshot, OUT);
    return 0;
}
