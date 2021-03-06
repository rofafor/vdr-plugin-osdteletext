/*************************************************************** -*- c++ -*-
 *                                                                         *
 *   display.c - Actual implementation of OSD display variants and         *
 *               Display:: namespace that encapsulates a single cDisplay.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Changelog:                                                            *
 *     2005-03    initial version (c) Udo Richter                          *
 *                                                                         *
 ***************************************************************************/

#include <strings.h>
#include <vdr/config.h>
#include "setup.h"
#include "display.h"
#include "txtfont.h"

// Static variables of Display:: namespace
Display::Mode Display::mode=Display::Full;
cDisplay *Display::display=NULL;


void Display::SetMode(Display::Mode NewMode) {
    // (re-)set display mode.

    if (display!=NULL && NewMode==mode) return;
    // No change, nothing to do

    // OSD origin, centered on VDR OSD
    int x0=Setup.OSDLeft+(Setup.OSDWidth-ttSetup.OSDwidth)*ttSetup.OSDHAlign/100;
    int y0=Setup.OSDTop +(Setup.OSDHeight-ttSetup.OSDheight)*ttSetup.OSDVAlign/100;

    switch (NewMode) {
    case Display::Full:
        // Need to re-initialize *display:
        Delete();
        // Try 32BPP display first:
        display=new cDisplay32BPP(x0,y0,ttSetup.OSDwidth,ttSetup.OSDheight);
        break;
    case Display::HalfUpper:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfLower) {
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetUpper(true);
            break;
        }
        // Need to re-initialize *display:
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,ttSetup.OSDwidth,ttSetup.OSDheight,true);
        break;
    case Display::HalfLower:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfUpper) {
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetUpper(false);
            break;
        }
        // Need to re-initialize *display:
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,ttSetup.OSDwidth,ttSetup.OSDheight,false);
        break;
    }
    mode=NewMode;
    // If display is invalid, clean up immediately:
    if (!display->Valid()) Delete();
    // Pass through OSD black transparency
    SetBackgroundColor((tColor)ttSetup.configuredClrBackground);
}

void Display::ShowUpperHalf() {
    // Enforce upper half of screen to be visible
    if (GetZoom()==cDisplay::Zoom_Lower)
        SetZoom(cDisplay::Zoom_Upper);
    if (mode==HalfLower)
        SetMode(HalfUpper);
}


cDisplay32BPP::cDisplay32BPP(int x0, int y0, int width, int height)
    : cDisplay(width,height) {
    // 32BPP display for True Color OSD providers

    osd = cOsdProvider::NewOsd(x0, y0);
    if (!osd) return;

    width=(width+1)&~1;
    // Width has to end on byte boundary, so round up

    tArea Areas[] = { { 0, 0, width - 1, height - 1, 32 } };
    if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        DELETENULL(osd);
        return;
    }
    osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));

    setOutputWidth(width);
    setOutputHeight(Height);

#if defined(APIVERSNUM) && APIVERSNUM >= 20107
    Width = 480;
    Height = 250;
#endif

    esyslog("OSD-Teletext: 32BPP");

    InitScaler();

    CleanDisplay();
}


cDisplay32BPPHalf::cDisplay32BPPHalf(int x0, int y0, int width, int height, bool upper)
    : cDisplay(width,height), Upper(upper), OsdX0(x0), OsdY0(y0)
{
    osd=NULL;

    // Redirect all real init work to method
    InitOSD();
}

void cDisplay32BPPHalf::InitOSD() {
    delete osd;
    osd = cOsdProvider::NewOsd(OsdX0, OsdY0);
    if (!osd) return;

    int width=(Width+1)&~1;
    // Width has to end on byte boundary, so round up

    tArea Areas[] = { { 0, 0, width - 1, Height - 1, 32 } };
    // Try full-size area first

    while (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        // Out of memory, so shrink
        if (Upper) {
            // Move up lower border
            Areas[0].y2=Areas[0].y2-1;
        } else {
            // Move down upper border
            Areas[0].y1=Areas[0].y1+1;
        }
        if (Areas[0].y1>Areas[0].y2) {
            // Area is empty, fail miserably
            DELETENULL(osd);
            return;
        }
    }
    // Add some backup
    // CanHandleAreas is not accurate enough
    if (Upper) {
        Areas[0].y2=Areas[0].y2-10;
    } else {
        Areas[0].y1=Areas[0].y1+10;
    }

    osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));

    setOutputWidth(width);
    setOutputHeight(Height);

#if defined(APIVERSNUM) && APIVERSNUM >= 20107
    Width = 480;
    Height = 250;
#endif

    InitScaler();

    CleanDisplay();

    // In case we switched on the fly, do a full redraw
    Dirty=true;
    DirtyAll=true;
    Flush();
}
