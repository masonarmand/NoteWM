/*
 * file: frame.c
 * -------------
 * This file contains all functions related to the NoteWM_Frame data structure,
 * NoteWM_Button data structures, reparenting, and drawing frames.
 *
 * Author: Mason Armand
 * Date Created: Jun 1, 2023
 * Last Modified: Jun 8, 2023
 */
#include "notewm.h"

/* ---------- Linked List Functions ---------- */
void add_frame(NoteWM_Frame* frame, NoteWM_Frame** list)
{
        frame->next = *list;
        *list = frame;
}


void remove_frame(Display* display, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        NoteWM_Frame* current = *list;
        NoteWM_Frame* prev = NULL;

        while (current != NULL) {
                if (current == frame) {
                        if (prev == NULL) {
                                *list = current->next;
                        }
                        else {
                                prev->next = current->next;
                        }
                        free_frame(display, frame);
                        return;
                }
                prev = current;
                current = current->next;
        }
}


void free_frame(Display* display, NoteWM_Frame* frame)
{
        if (is_valid_window(display, frame->frame))
                XDestroyWindow(display, frame->frame);

        if(frame->button_list) {
                if(frame->button_list->buttons) {
                        free(frame->button_list->buttons);
                        frame->button_list->buttons = NULL;
                }
                free(frame->button_list);
        }

        XFreeGC(display, frame->gc);

        free(frame);
}


NoteWM_Frame* find_frame_by_component(Window component, NoteWM_Frame* list)
{
        NoteWM_Frame* current = list;
        while (current != NULL) {
                if (current->frame == component
                || current->title_bar == component
                || current->child_window == component
                || is_button(component, current)) {

                        return current;
                }
                current = current->next;
        }
        return NULL;
}


bool is_button(Window window, NoteWM_Frame* frame)
{
        unsigned int i;
        for (i = 0; i < frame->button_list->count; i++) {
                if (window == frame->button_list->buttons[i].window) {
                        return true;
                }
        }
        return false;
}


/* ---------- Frame Window Creation Functions ---------- */
NoteWM_Frame* create_frame(Display* display, Window window, Window root)
{
        NoteWM_Frame* frame;
        XWindowAttributes attrs;
        XSizeHints hints;
        long msize;
        int f_width = 100;
        int f_height = 100;

        XGetWindowAttributes(display, window, &attrs);

        if (XGetWMNormalHints(display, window, &hints, &msize) && (hints.flags & PSize) && hints.width != 0) {
                f_width = hints.width;
                f_height = hints.height + TITLE_HEIGHT;
        } else if (attrs.width != 0 && attrs.height != 0) {
                f_width = attrs.width;
                f_height = attrs.height + TITLE_HEIGHT;
        }
        else {
                XResizeWindow(display, window, f_width, f_height - TITLE_HEIGHT);
        }

        frame = malloc(sizeof(NoteWM_Frame));
        frame->is_fullscreen = false;
        frame->child_window = window;
        frame->workspace_id = global_state.current_workspace;

        frame->frame = XCreateSimpleWindow(
                display, root,
                attrs.x, attrs.y,
                f_width, f_height,
                BORDER_WIDTH, global_state.conf.border_color, global_state.conf.bg_color
        );

        create_title_bar(display, frame);

        XSelectInput(display, window, StructureNotifyMask | PropertyChangeMask | EnterWindowMask);
        XSelectInput(display, frame->frame, StructureNotifyMask);

        XReparentWindow(display, window, frame->frame, 0, 0);

        XMapWindow(display, frame->frame);
        XMapWindow(display, window);

        XMoveWindow(display, window, 0, 0 + TITLE_HEIGHT + BORDER_WIDTH);

        set_net_wm_desktop(display, window, frame->workspace_id);
        add_client_window(display, root, window);

        frame->next = NULL;
        printf("created frame for window %ld %ld\n", window, frame->child_window);

        return frame;
}


void create_title_bar(Display* display, NoteWM_Frame* frame)
{
        XWindowAttributes frame_attrs;

        XGetWindowAttributes(display, frame->frame, &frame_attrs);

        frame->title_bar = XCreateSimpleWindow(
                display, frame->frame,
                -BORDER_WIDTH, -BORDER_WIDTH,
                frame_attrs.width, TITLE_HEIGHT,
                BORDER_WIDTH, global_state.conf.border_color, global_state.conf.title_bar_color
        );

        frame->title_string_window = XCreateSimpleWindow(
                display, frame->title_bar,
                BORDER_WIDTH, BORDER_WIDTH,
                frame_attrs.width - (PADDING * 2) - BUTTON_SIZE, TITLE_HEIGHT + (PADDING * 2),
                0, global_state.conf.fg_color, global_state.conf.title_bar_color
        );

        frame->button_list = NULL;
        frame->gc = XCreateGC(display, frame->title_string_window, 0, NULL);
        XSetForeground(display, frame->gc, global_state.conf.fg_color);
        XSetFont(display, frame->gc, global_font->fid);

        XSelectInput(display, frame->title_bar, SubstructureRedirectMask | ButtonPressMask | ExposureMask);
        XMapWindow(display, frame->title_bar);
        XMapWindow(display, frame->title_string_window);

        create_button(display, frame, global_state.conf.close_color, ButtonPressMask, handle_close_button);
        create_button(display, frame, global_state.conf.expand_color, ButtonPressMask, handle_expand_button);
        create_button(display, frame, global_state.conf.split_color, ButtonPressMask, handle_split_right_button);
        create_button(display, frame, global_state.conf.split_color, ButtonPressMask, handle_split_left_button);
        update_frame_text(display, frame);
}

/* ---------- Frame Management Functions ---------- */

void resize_frame(Display* display, NoteWM_Frame* frame, int width, int height, bool is_event)
{
        unsigned int i;

        if (!is_event)
                frame->ignore_configure_events = true;

        XResizeWindow(display, frame->frame, width, height);
        XResizeWindow(display, frame->child_window, width, height - TITLE_HEIGHT);
        XResizeWindow(display, frame->title_bar, width, TITLE_HEIGHT);

        for (i = 0; i < frame->button_list->count; i++) {
                XMoveWindow(
                        display, frame->button_list->buttons[i].window,
                        width - (BUTTON_SIZE * (i + 1)) - (PADDING * (i + 1)),
                        TITLE_HEIGHT - BUTTON_SIZE - PADDING
                );
        }
}


void move_resize_frame(Display* display, NoteWM_Frame* frame, int x, int y, int width, int height)
{
        resize_frame(display, frame, width, height, false);
        XMoveWindow(display, frame->frame, x, y);
        /* notify the child of its new size and position, so that things like mouse input
         * don't behave as if the window is at (0,0) */
        send_configure_notify(display, frame);
}


void update_frame_text(Display* display, NoteWM_Frame* frame)
{
        char window_name[256];
        get_window_name(display, frame->child_window, window_name, sizeof(window_name));
        XClearWindow(display, frame->title_string_window);

        XDrawString(
                display, frame->title_string_window, frame->gc,
                TITLE_STRING_X, TITLE_STRING_Y,
                window_name, strlen(window_name)
        );
}


void set_frame_fullscreen(Display* display, NoteWM_Frame* frame, bool fullscreen)
{
        if (fullscreen && !frame->is_fullscreen) {
                XWindowAttributes attrs;
                int screen_width;
                int screen_height;
                int x = 0;
                int y = global_total_bar_height;
                int width;
                int height;

                XGetWindowAttributes(display, frame->frame, &attrs);
                get_display_dimensions(display, &screen_width, &screen_height);
                frame->x = attrs.x;
                frame->y = attrs.y;
                frame->w = attrs.width;
                frame->h = attrs.height;
                frame->is_fullscreen = true;

                width = screen_width - (BORDER_WIDTH * 2);
                height = (screen_height - global_total_bar_height) - (BORDER_WIDTH * 2);

                move_resize_frame(display, frame, x, y, width, height);
                XRaiseWindow(display, frame->frame);
        }
        else if (!fullscreen && frame->is_fullscreen) {
                move_resize_frame(display, frame, frame->x, frame->y, frame->w, frame->h);
                frame->is_fullscreen = false;
        }
}

/* ---------- Button Related Functions ---------- */
void create_button(Display* display, NoteWM_Frame* frame, unsigned long color, unsigned long mask, ButtonClickFunc event_function)
{
        XWindowAttributes attrs;
        NoteWM_Button new_button;
        int index;
        int x;
        int y;

        if (!frame->button_list)
                frame->button_list = init_button_list(4);

        XGetWindowAttributes(display, frame->title_bar, &attrs);

        index = frame->button_list->count + 1;
        x = attrs.width - (BUTTON_SIZE * index) - (PADDING * index);
        y = TITLE_HEIGHT - BUTTON_SIZE - PADDING;

        new_button.window = XCreateSimpleWindow(
                display, frame->title_bar,
                x, y,
                BUTTON_SIZE, BUTTON_SIZE,
                1, global_state.conf.button_border_color, color
        );

        new_button.on_click = event_function;

        XSelectInput(display, new_button.window, mask);
        append_button(frame->button_list, new_button);
        XMapWindow(display, new_button.window);
}


NoteWM_ButtonList* init_button_list(unsigned int initial_capacity)
{
        NoteWM_ButtonList* button_list = malloc(sizeof(NoteWM_ButtonList));
        button_list->buttons = malloc(sizeof(NoteWM_Button) * initial_capacity);
        button_list->count = 0;
        button_list->capacity = initial_capacity;
        return button_list;
}


void append_button(NoteWM_ButtonList* button_list, NoteWM_Button button)
{
        if (button_list->count >= button_list->capacity) {
                button_list->capacity *= 2;
                button_list->buttons = realloc(button_list->buttons, sizeof(NoteWM_Button) * button_list->capacity);
        }

        button_list->buttons[button_list->count++] = button;
}


void free_button_list(NoteWM_ButtonList* button_list)
{
        free(button_list->buttons);
        free(button_list);
}

