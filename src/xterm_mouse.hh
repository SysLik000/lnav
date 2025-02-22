/**
 * Copyright (c) 2007-2012, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file xterm_mouse.hh
 */

#ifndef xterm_mouse_hh
#define xterm_mouse_hh

#include <notcurses/notcurses.h>

#include "config.h"

/**
 * Base class for delegates of the xterm_mouse class.
 */
class mouse_behavior {
public:
    virtual ~mouse_behavior() = default;

    /**
     * Callback used to process mouse events.
     *
     * @param button The button that was pressed or released.  This will
     *   be one of the XT_BUTTON or XT_SCROLL constants in the xterm_mouse
     *   class.
     * @param x      The X coordinate where the event occurred.
     * @param y      The Y coordinate where the event occurred.
     */
    virtual void mouse_event(notcurses* nc, int button, bool release, int x, int y) = 0;
};

/**
 * Class that handles xterm mouse events coming through the ncurses interface.
 */
class xterm_mouse {
public:
    static const int XT_BUTTON1 = 0;
    static const int XT_BUTTON2 = 1;
    static const int XT_BUTTON3 = 2;

    static const int XT_DRAG_FLAG = 32;
    static const int XT_SCROLL_WHEEL_FLAG = 64;
    static const int XT_SCROLL_UP = XT_SCROLL_WHEEL_FLAG | XT_BUTTON1;
    static const int XT_SCROLL_DOWN = XT_SCROLL_WHEEL_FLAG | XT_BUTTON2;

    static const int XT_BUTTON__MASK
        = XT_SCROLL_WHEEL_FLAG | XT_BUTTON1 | XT_BUTTON2 | XT_BUTTON3;

    static const int XT_MODIFIER_SHIFT = 4;
    static const int XT_MODIFIER_META = 8;
    static const int XT_MODIFIER_CTRL = 16;
    static const int XT_MODIFIER_MASK
        = XT_MODIFIER_SHIFT | XT_MODIFIER_META | XT_MODIFIER_CTRL;

    static const char* XT_TERMCAP;
    static const char* XT_TERMCAP_TRACKING;
    static const char* XT_TERMCAP_SGR;

    /**
     * @param enabled True if xterm mouse support should be enabled in the
     *   terminal.
     */
    void set_enabled(notcurses* nc, bool enable);

    /**
     * @return True if xterm mouse support is enabled, false otherwise.
     */
    bool is_enabled() const { return this->xm_enabled; }

    /**
     * @param mb The delegate to send mouse events to.
     */
    void set_behavior(mouse_behavior* mb) { this->xm_behavior = mb; }

    mouse_behavior* get_behavior() const { return this->xm_behavior; }

    /**
     * Handle a KEY_MOUSE character from ncurses.
     * @param ch unused
     */
    void handle_mouse(notcurses* nc, const ncinput& nci);

private:
    bool xm_enabled{false};
    mouse_behavior* xm_behavior{nullptr};
};
#endif
