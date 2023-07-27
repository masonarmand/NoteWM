/*
 * file: main.c
 * ------------
 * This is the entry point for the NoteWM window manager. For more information on NoteWM, see README.md
 *
 * Author: Mason Armand
 * Date Created: May 31, 2023
 * Last Modified: Jun 8, 2023
 */
#include "notewm.h"

NoteWM_GlobalState global_state = { 0 };
Cursor global_cursor_default;
Cursor global_cursor_resize;
Cursor global_cursor_grab;
Cursor global_cursor_plus;
XFontStruct* global_font;

int main(void)
{
        Display* display;
        Window root;
        XEvent e;
        bool running = true;

        /* GrabKeys */
        NoteWM_KeyBinding keybindings[] = {
                { XK_d, Mod4Mask },
                { XK_Return, Mod4Mask },
                { XK_q, Mod4Mask },
                { XK_E, Mod4Mask | ShiftMask},
                /* for switching workspaces */
                { XK_1, Mod4Mask },
                { XK_2, Mod4Mask },
                { XK_3, Mod4Mask },
                { XK_4, Mod4Mask },
                { XK_5, Mod4Mask },
                { XK_6, Mod4Mask },
                { XK_7, Mod4Mask },
                { XK_8, Mod4Mask },
                { XK_9, Mod4Mask },
                { NoSymbol, 0 } /* end of list */
        };
        /* GrabButtons */
        NoteWM_MouseBinding mousebindings[] = {
                { Button1, Mod4Mask },
                { Button2, Mod4Mask },
                { Button3, Mod4Mask },
                { 0, 0 } /* end of list */
        };

        NoteWM_Frame* client_list = NULL;
        NoteWM_WindowResizeInfo r_info = { 0 };

        /* default colors */
        global_state.conf.title_bar_color = 0xeaffff;
        global_state.conf.button_border_color = 0x000000;
        global_state.conf.border_color = 0x55aaaa;
        global_state.conf.fg_color = 0x000000;
        global_state.conf.bg_color = 0xffffea;
        global_state.conf.close_color = 0xffaaaa;
        global_state.conf.expand_color = 0xeeee9e;
        global_state.conf.split_color = 0x88cc88;

        if (ini_parse("/home/mace/.config/notewm/config.ini", conf_handler, &global_state.conf) < 0) {
                fprintf(stderr, "Failed to load config.ini, using default settings.\n");
        }

        XSetErrorHandler(xerror_handler);

        if (!(display = XOpenDisplay(NULL)))
                fprintf(stderr, "Failed to open display.\n");

        root = DefaultRootWindow(display);

        XSelectInput(
                display, root,
                SubstructureRedirectMask |
                SubstructureNotifyMask |
                PropertyChangeMask |
                ButtonPressMask |
                ButtonReleaseMask |
                PointerMotionMask
        );

        /*XSynchronize(display, true);*/
        global_state.current_workspace = 0;

        global_font = XLoadQueryFont(display, "-misc-fixed-bold-r-normal--13-120-75-75-C-70-iso10646-1");
        if (!global_font) {
                fprintf(stderr, "Failed to load font\n");
                return 1;
        }

        /* define cursors */
        global_cursor_default = XCreateFontCursor(display, XC_left_ptr);
        global_cursor_grab = XCreateFontCursor(display, XC_fleur);
        global_cursor_resize = XCreateFontCursor(display, XC_sizing);
        global_cursor_plus = XCreateFontCursor(display, XC_plus);
        XDefineCursor(display, root, global_cursor_default);

        grab_keys(display, root, keybindings);
        grab_buttons(display, root, mousebindings);

        adopt_existing_windows(display, root, &client_list);
        set_net_supported(display, root);
        set_ewhm_desktop_properties(display, root);
        XFlush(display);
        XMapRaised(display, root);

        while (running) {
                NoteWM_Frame* frame;
                XKeyEvent k;

                XNextEvent(display, &e);

                switch (e.type) {
                case MapRequest:
                        handle_map_request(display, root, &e.xmaprequest, &client_list);
                        break;
                case UnmapNotify:
                        handle_unmap_notify(display, root, &e.xunmap, &client_list);
                        break;
                case DestroyNotify:
                        handle_destroy_notify(display, root, &e.xdestroywindow, &client_list);
                        XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
                        XSync(display, false);
                        break;
                case ConfigureRequest:
                        handle_configure_request(display, &e.xconfigurerequest, client_list);
                        break;
                case PropertyNotify:
                        handle_property_notify(display, &e.xproperty, client_list);
                        break;
                case ClientMessage:
                        handle_client_message(display, root, &e.xclient, &client_list);
                        break;
                case ReparentNotify:
                        handle_reparent_notify(display, root, &e.xreparent, &client_list);
                        break;
                case ResizeRequest:
                        handle_resize_request(display, &e.xresizerequest, client_list);
                        break;
                case ButtonPress:
                        handle_button_press(display, root, &e.xbutton, &client_list);
                        r_info.event = e.xbutton;
                        if (e.xbutton.subwindow != None)
                                XGetWindowAttributes(display, e.xbutton.subwindow, &r_info.attrs);
                        break;
                case ButtonRelease:
                        XUngrabPointer(display, CurrentTime);
                        r_info.event.subwindow = None;
                        break;
                case MotionNotify:
                        handle_motion_notify(display, &e.xbutton, r_info, client_list);
                        break;
                case EnterNotify:
                        handle_enter_notify(display, root, &e.xcrossing, client_list);
                        break;
                case KeyPress:
                        k = e.xkey;
                        handle_key_press(display, root, &e.xkey, client_list);

                        if ((k.state & Mod4Mask) && (k.state & ShiftMask) && k.keycode == XKeysymToKeycode(display, XK_E))
                                running = false;

                        if ((k.state & Mod4Mask) && k.keycode == XKeysymToKeycode(display, XK_d)) {
                                system("dmenu_run -fn 'Droid Sans Mono-12' -nb black -sb '#ffffff' -l 5");
                        }
                        if ((k.state & Mod4Mask) && k.keycode == XKeysymToKeycode(display, XK_Return)) {
                                launch_program("xfce4-terminal");
                        }
                        break;
                case Expose:
                        if (e.xexpose.count == 0) {
                                frame = find_frame_by_component(e.xexpose.window, client_list);
                                if (frame) {
                                        update_frame_text(display, frame);
                                }
                        }
                        break;
                }
        }
        free_client_window_list();
        XFreeFont(display, global_font);
        XFreeCursor(display, global_cursor_default);
        XFreeCursor(display, global_cursor_resize);
        XFreeCursor(display, global_cursor_grab);
        XFreeCursor(display, global_cursor_plus);
        XCloseDisplay(display);

        return 0;
}
