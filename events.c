/*
 * file: events.c
 * --------------
 * This file contains all the handler functions for X11 events.
 *
 * Author: Mason Armand
 * Date Created: Jun 1, 2023
 * Last Modified: Jun 8, 2023
 */
#include "notewm.h"

void handle_close_button(Display* display, Window root, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        close_frame(display, root, frame, list);
}


void handle_expand_button(Display* display, Window root, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        (void) list;
        (void) root;
        set_frame_fullscreen(display, frame, !frame->is_fullscreen);
}


void handle_split_left_button(Display* display, Window root, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        int screen_width;
        int screen_height;
        int x = 0;
        int y = global_total_bar_height;
        int width;
        int height;
        (void) list;
        (void) root;

        get_display_dimensions(display, &screen_width, &screen_height);
        frame->is_fullscreen = false;

        width = (screen_width / 2) - BORDER_WIDTH;
        height = (screen_height - global_total_bar_height) - (BORDER_WIDTH * 2);

        move_resize_frame(display, frame, x, y, width, height);
}


void handle_split_right_button(Display* display, Window root, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        int screen_width;
        int screen_height;
        int x;
        int y = global_total_bar_height;
        int width;
        int height;
        (void) list;
        (void) root;

        get_display_dimensions(display, &screen_width, &screen_height);
        frame->is_fullscreen = false;

        x = (screen_width / 2);
        width = (screen_width / 2) - BORDER_WIDTH;
        height = (screen_height - global_total_bar_height) - (BORDER_WIDTH * 2);

        move_resize_frame(display, frame, x, y, width, height);
}


void handle_key_press(Display* display, Window root, XKeyEvent* e, NoteWM_Frame* list)
{
        unsigned short workspace_id;

        if ((e->state & Mod4Mask)
        && (e->keycode >= XKeysymToKeycode(display, XK_1))
        && (e->keycode <= XKeysymToKeycode(display, XK_9))) {
                workspace_id = e->keycode - XKeysymToKeycode(display, XK_1);
                switch_to_workspace(display, root, workspace_id, list);
                printf("Switched to workspace: %d\n", workspace_id);
        }
}


void handle_button_press(Display* display, Window root, XButtonEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame;
        unsigned int i;

        /* start blitting a window */
        /*
        if ((e.state & Mod4Mask) && e.xbutton.button == Button2) {
                blit_start_x = e.x;
                blit_start_y = e.y;
                blitting = true;
        }*/

        frame = find_frame_by_component(e->subwindow, *list);
        if (frame) {
                if (e->button == Button1)
                        grab_change_cursor(display, frame->frame, global_cursor_grab);
                else if (e->button == Button3)
                        grab_change_cursor(display, frame->frame, global_cursor_resize);

                XRaiseWindow(display, frame->frame);
                XSetInputFocus(display, frame->child_window, RevertToParent, CurrentTime);
        }

        if (!(frame = find_frame_by_component(e->window, *list)))
                return;

        /* set focus to clicked window */
        XSetInputFocus(display, frame->child_window, RevertToParent, CurrentTime);
        XRaiseWindow(display, frame->frame);

        for (i = 0; i < frame->button_list->count; i++) {
                if (e->window == frame->button_list->buttons[i].window) {
                        frame->button_list->buttons[i].on_click(display, root, frame, list);
                        break;
                }
        }
}


void handle_motion_notify(Display* display, XButtonEvent* e, NoteWM_WindowResizeInfo r_info, NoteWM_Frame* list)
{
        XWindowAttributes attrs = r_info.attrs;
        XButtonEvent start = r_info.event;
        NoteWM_Frame* frame;

        /* move and resize frame window */
        if ((frame = find_frame_by_component(start.subwindow, list))) {
                int dx = e->x_root - start.x_root;
                int dy = e->y_root - start.y_root;

                frame->is_fullscreen = false;

                move_resize_frame(
                        display, frame,
                        attrs.x + (start.button == 1 ? dx : 0),
                        attrs.y + (start.button == 1 ? dy : 0),
                        attrs.width + (start.button == 3 ? dx : 0),
                        attrs.height + (start.button == 3 ? dy : 0)
                );
        }
}


void send_configure_notify(Display* display, NoteWM_Frame* frame)
{
        XConfigureEvent ce;
        XWindowAttributes attrs;

        XGetWindowAttributes(display, frame->frame, &attrs);

        ce.type = ConfigureNotify;
        ce.event = frame->child_window;
        ce.window = frame->child_window;

        ce.x = 0;
        ce.y = 0;
        ce.width = attrs.width;
        ce.height = attrs.height - TITLE_HEIGHT - (BORDER_WIDTH * 2);
        ce.border_width = 0;
        ce.above = None;
        ce.override_redirect = False;

        XSendEvent(display, frame->child_window, False, StructureNotifyMask, (XEvent*)&ce);
}


void handle_unmap_notify(Display* display, Window root, XUnmapEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, *list);
        printf("unmap notify on %ld\n", e->window);
        if (e->event == root || !is_valid_window(display, e->window))
                return;

        if (frame) {
                XUnmapWindow(display, frame->frame);
        }
        else {
                XUnmapWindow(display, e->window);
        }
}


void handle_map_request(Display* display, Window root, XMapRequestEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, *list);
        printf("Map request on %ld\n", e->window);
        if (frame) {
                XMapWindow(display, frame->frame);
                XMapWindow(display, frame->child_window);
                return;
        }
        map_window(display, root, e->window, list);
}


void handle_configure_request(Display* display, XConfigureRequestEvent* e, NoteWM_Frame* list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, list);
        XWindowChanges changes;

        if (frame && !frame->ignore_configure_events) {
                XMoveWindow(display, frame->frame, e->x, e->y);
                resize_frame(display, frame, e->width, e->height + TITLE_HEIGHT, true);
                return;
        }
        else if (frame && frame->ignore_configure_events) {
                frame->ignore_configure_events = false;
                return;
        }
        else if (!frame) {
                changes.x = e->x;
                changes.y = e->y;
                changes.width = e->width;
                changes.height = e->height;
                changes.border_width = e->border_width;
                changes.sibling = e->above;
                changes.stack_mode = e->detail;

                XConfigureWindow(display, e->window, e->value_mask, &changes);
        }
}


void handle_destroy_notify(Display* display, Window root, XDestroyWindowEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, *list);

        printf("destroy notify event on %ld\n", e->window);

        if (frame) {
                if (is_valid_window(display, frame->frame)) {
                        XDestroyWindow(display, frame->frame);
                }
                remove_client_window(display, root, frame->child_window);
                remove_frame(display, frame, list);
        }
        else if (is_valid_window(display, e->window)) {
                XDestroyWindow(display, e->window);
        }
}


void handle_resize_request(Display* display, XResizeRequestEvent* e, NoteWM_Frame* list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, list);

        if (frame) {
                int new_width = e->width + (2 * BORDER_WIDTH);
                int new_height = e->height + TITLE_HEIGHT + (2 * BORDER_WIDTH);
                resize_frame(display, frame, new_width, new_height, true);
                return;
        }
        XResizeWindow(display, e->window, e->width, e->height);
}


void handle_property_notify(Display* display, XPropertyEvent* e, NoteWM_Frame* list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, list);
        char window_name[256];

        if (!frame)
                return;

        switch (e->atom) {
        case XA_WM_TRANSIENT_FOR:
                break;
        case XA_WM_NAME:
                get_window_name(display, frame->child_window, window_name, sizeof(window_name));
                printf("new name: %s\n", window_name);
                update_frame_text(display, frame);
        default: break;
        }
        if (e->atom == _NET_WM_WINDOW_TYPE)
                update_window_type(display, frame->child_window);
}


void handle_client_message(Display* display, Window root, XClientMessageEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, *list);
        Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", false);

        if (e->message_type == net_current_desktop) {
                int new_workspace = e->data.l[0];
                switch_to_workspace(display, root, new_workspace, *list);
        }

        if (!frame)
                return;

        if (e->message_type == WM_PROTOCOLS) {
                if (e->data.l[0] == (long int) WM_DELETE_WINDOW) {
                        XDestroyWindow(display, frame->frame);
                        remove_client_window(display, root, frame->child_window);
                        remove_frame(display, frame, list);
                }
        }
        else if (e->message_type == _NET_WM_STATE) {
                if (e->data.l[1] == (long int) _NET_WM_STATE_FULLSCREEN
                ||  e->data.l[2] == (long int) _NET_WM_STATE_FULLSCREEN) {
                        bool fullscreen = (e->data.l[0] == 1 || (e->data.l[0] == 2 && !frame->is_fullscreen));
                        set_frame_fullscreen(display, frame, fullscreen);
                }
        }
        else if (e->message_type == _NET_CLOSE_WINDOW) {
                XDestroyWindow(display, frame->frame);
                remove_client_window(display, root, frame->child_window);
                remove_frame(display, frame, list);
        }
}


void handle_reparent_notify(Display* display, Window root, XReparentEvent* e, NoteWM_Frame** list)
{
        NoteWM_Frame* frame = find_frame_by_component(e->window, *list);
        (void) display;
        printf("reparent notify\n");
        if(e->parent == root) {
                printf("reparented to root\n");
                if (!frame) {
                        map_window(display, root, e->window, list);
                }
        }
        else {
                NoteWM_Frame* new_parent_frame = find_frame_by_component(e->parent, *list);
                printf("reparented to other window\n");
                if (new_parent_frame)
                        printf("reparented to frame");
                else
                        printf("reparented to somethin??");
                if (frame && !new_parent_frame) {
                        printf("no longer managing frame\n");
                        remove_client_window(display, root, frame->child_window);
                        remove_frame(display, frame, list);
                }
        }
}


void handle_enter_notify(Display* display, Window root, XCrossingEvent* e, NoteWM_Frame* list)
{
        if (e->window == root) {
                XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
                return;
        }
        focus_window(display, e->window, list);
}
