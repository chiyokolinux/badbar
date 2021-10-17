/*******************************************************************************
 *                                                                             *
 * badbar - A simple bar for badwm                                             *
 * Copyright (C) 2021 Emily <elishikawa@jagudev.net>                           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation, either version 3 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program.  If not, see <https://www.gnu.org/licenses/>.            *
 *                                                                             *
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <time.h>
#include <X11/Xlib.h>

#define ITOA(n)         my_itoa((char [12]) { 0 }, (n) )

typedef struct Desktop {
    int haswin;
    int hasurgn;
    int showbar;
    char title[52];
} Desktop;

typedef struct BadStatus {
    int desknum;
    int deskfocus;
    Desktop **desks;
} BadStatus;

int wh, ww, retval = 0;
unsigned int win_unfocus, win_focus, win_focus_urgn, win_unfocus_urgn, bgcol, fgcol;
Display *dis;
Window root, bar;
Pixmap pm;
GC gc;

#include "config.h"

void loadfont();
unsigned long getcolor(const char* color, const int screen);
char *my_itoa(char *dest, int i);
void initbar();
void runbar();
void parsebar(BadStatus *stat);
void printbar(BadStatus *stat);
void renderbar(BadStatus *stat);
void cleanup();

/**
 * load bar font
**/
void loadfont() {
    XFontStruct *font = XLoadQueryFont(dis, PANELFONT);
    if (!font) {
        fprintf(stderr, "ERROR: Unable to load font %s: using fixed\n", PANELFONT);
        font = XLoadQueryFont(dis, "fixed");
    }
    XSetFont(dis, gc, font->fid);
}

/**
 * convert hex color to X11 color
**/
unsigned long getcolor(const char* color, const int screen) {
    XColor c; Colormap map = DefaultColormap(dis, screen);
    if (!XAllocNamedColor(dis, map, color, &c, &c)) err(EXIT_FAILURE, "cannot allocate color");
    return c.pixel;
}

/**
 * int to char*
**/
char *my_itoa(char *dest, int i) {
    sprintf(dest, "%d", i);
    return dest;
}

/**
 * init bar stuff
**/
void initbar() {
    const int screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    ww = XDisplayWidth(dis, screen);
    wh = XDisplayHeight(dis, screen) - PANEL_HEIGHT;

    win_focus = getcolor(FOCUS, screen);
    win_unfocus = getcolor(UNFOCUS, screen);
    win_focus_urgn = getcolor(URGENTFOCUS, screen);
    win_unfocus_urgn = getcolor(URGENTUNFOCUS, screen);
    bgcol = getcolor(BACKGROUNDCOL, screen);
    fgcol = getcolor(FOREGROUNDCOL, screen);

    XSetWindowAttributes wa = { .background_pixel = bgcol, .override_redirect = 1, .event_mask = ExposureMask, };
    bar = XCreateWindow(dis, root, 0,
            (TOP_PANEL) ? 0 : wh, ww, PANEL_HEIGHT, 1,
            CopyFromParent, InputOutput, CopyFromParent,
            CWBackPixel | CWOverrideRedirect | CWEventMask, &wa);
    XMapWindow(dis, bar);
    XSetWindowBorderWidth(dis, bar, 0);

    XGCValues gcv = { .graphics_exposures = 0, };
    gc = XCreateGC(dis, root, GCGraphicsExposures, &gcv);
    pm = XCreatePixmap(dis, bar, ww, PANEL_HEIGHT, DefaultDepth(dis,screen));
}

/**
 * bar run function
**/
void runbar() {
    BadStatus *stat = malloc(sizeof(BadStatus));
    stat->desknum = 0;
    stat->deskfocus = 0;
    while (!feof(stdin)) {
        parsebar(stat);
        // printbar(stat);
        renderbar(stat);
        for (int i = 0; i < stat->desknum; i++) {
            free(stat->desks[i]);
        }
        if (stat->desknum)
            free(stat->desks);
    }
}

/**
 * parse bar info from stdin
**/
void parsebar(BadStatus *stat) {
    int scanval = scanf("%d:%d", &stat->desknum, &stat->deskfocus);
    if (scanval == 0 || scanval == EOF) {
        // EOF
        getchar();
        stat->desknum = 0;
        stat->deskfocus = 0;
        return;
    }
    stat->desks = malloc(sizeof(Desktop *) * (stat->desknum + 1));
    for (int i = 0; i < stat->desknum; i++) {
        stat->desks[i] = malloc(sizeof(Desktop) + 1);
        scanf(" %d:%d:%d:%48[^\v]\v", &stat->desks[i]->haswin, &stat->desks[i]->hasurgn, &stat->desks[i]->showbar, stat->desks[i]->title);
        stat->desks[i]->title[48] = '\0';
    }
    fflush(stdin);
}

/**
 * print bar info
**/
void printbar(BadStatus *stat) {
    printf("%d:%d", stat->desknum, stat->deskfocus);
    for (int i = 0; i < stat->desknum; i++) {
        printf(" %d:%d:%d:%.48s:", stat->desks[i]->haswin, stat->desks[i]->hasurgn, stat->desks[i]->showbar, stat->desks[i]->title);
    }
    putchar('\n');
    fflush(stdout);
}

/**
 * renders bar
**/
void renderbar(BadStatus *stat) {
    XSetForeground(dis, gc, bgcol);
    XFillRectangle(dis, pm, gc, 0, 0, ww, PANEL_HEIGHT);

    for (int i = 0; i < stat->desknum; i++) {
        if (i == stat->deskfocus || stat->desks[i]->hasurgn) {
            XSetForeground(dis, gc, stat->desks[i]->hasurgn ? ((i == stat->deskfocus) ? win_focus_urgn : win_unfocus_urgn) : win_focus);
            XFillRectangle(dis, pm, gc, i * PANEL_HEIGHT, 0, PANEL_HEIGHT, PANEL_HEIGHT);
        } else {
            XSetForeground(dis, gc, win_unfocus);
            XFillRectangle(dis, pm, gc, i * PANEL_HEIGHT, 0, PANEL_HEIGHT, PANEL_HEIGHT);
        }
        XSetForeground(dis, gc, fgcol);
        XDrawString(dis, pm, gc, i * PANEL_HEIGHT + PANELSTARTOFST, PANEL_HEIGHT - PANELSTARTOFST, ITOA(i+1), strlen(ITOA(i+1)));
    }

    XSetForeground(dis, gc, win_unfocus);
    XFillRectangle(dis, pm, gc, ww - (9 * PANEL_HEIGHT) - PANELSTARTOFST, 0, PANELSTARTOFST + (9 * PANEL_HEIGHT), PANEL_HEIGHT);

    time_t timer;
    char buffer[20];
    struct tm* tm_info;
    timer = time(NULL);
    tm_info = localtime(&timer);
    strftime(buffer, 22, TIMEFORMAT, tm_info);
    XSetForeground(dis, gc, fgcol);
    XDrawString(dis, pm, gc, ww - (9 * PANEL_HEIGHT), PANEL_HEIGHT - PANELSTARTOFST, buffer, 19);

    XCopyArea(dis, pm, bar, gc, 0, 0, ww, PANEL_HEIGHT, 0, 0);
    XSync(dis, False);
}

/**
 * cleans up everyhting
**/
void cleanup() {
    XFreeGC(dis, gc);
    XFreePixmap(dis, pm);
    XDestroyWindow(dis, bar);
    XSync(dis, False);
}

int main(int argc, char const *argv[]) {
    if (argc == 2 && !strncmp(argv[1], "-v", 3))
        errx(0, "badbar - A simple bar for badwm");
    if(!(dis = XOpenDisplay(NULL))) errx(1, "CRITICAL: Could not open X Display - terminating!");
    initbar();
    runbar();
    XCloseDisplay(dis);
    return retval;
}
