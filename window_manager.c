/*
 * file: window_manager.c
 * ----------------------
 * This file contains functions that do standard window manager things:
 * closing windows, focusing windows, mapping windows, getting the names
 * of windows, etc.
 *
 * Author: Mason Armand
 * Date Created: Jun 1, 2023
 * Last Modified: Jun 8, 2023
 */
#include "notewm.h"

int ignore_xerrors(Display *display, XErrorEvent *error);

unsigned int global_total_bar_height;


void update_net_client_list(Display* display, Window root)
{
        Atom net_client_list = XInternAtom(display, "_NET_CLIENT_LIST", false);
        XChangeProperty(display, root, net_client_list, XA_WINDOW, 32, PropModeReplace, (unsigned char*)global_state.client_windows, global_state.num_client_windows);
}


void add_client_window(Display* display, Window root, Window window)
{
        int num = global_state.num_client_windows;
        global_state.client_windows = realloc(global_state.client_windows, (num + 1) * sizeof(Window));
        global_state.client_windows[num] = window;
        global_state.num_client_windows++;
        update_net_client_list(display, root);
}


void remove_client_window(Display* display, Window root, Window window)
{
        unsigned int i;
        for (i = 0; i < global_state.num_client_windows; i++) {
                if (global_state.client_windows[i] == window) {
                        memmove(&global_state.client_windows[i], &global_state.client_windows[i + 1], (global_state.num_client_windows - i - 1) * sizeof(Window));

                        global_state.num_client_windows--;

                        global_state.client_windows = realloc(global_state.client_windows, global_state.num_client_windows * sizeof(Window));

                        update_net_client_list(display, root);

                        return;
                }
        }
}


void free_client_window_list(void)
{
        if(global_state.client_windows) {
                free(global_state.client_windows);
                global_state.client_windows = NULL;
        }
        global_state.num_client_windows = 0;
}


void set_ewhm_desktop_properties(Display* display, Window root)
{
        Atom net_number_of_desktops = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", false);
        Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", false);
        Atom net_desktop_names = XInternAtom(display, "_NET_DESKTOP_NAMES", false);
        Atom net_desktop_viewport = XInternAtom(display, "_NET_DESKTOP_VIEWPORT", false);
        XTextProperty text_property;
        long number_of_desktops = NUM_WORKSPACES;
        long current_desktop = global_state.current_workspace;
        const char* desktop_names[NUM_WORKSPACES] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
        long desktop_viewport[NUM_WORKSPACES * 2] = {0};

        XChangeProperty(display, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&number_of_desktops, 1);
        XChangeProperty(display, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&current_desktop, 1);

        XStringListToTextProperty((char**) desktop_names, NUM_WORKSPACES, &text_property);
        XSetTextProperty(display, root, &text_property, net_desktop_names);
        XFree(text_property.value);

        XChangeProperty(display, root, net_desktop_viewport, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)desktop_viewport, NUM_WORKSPACES * 2);
}


void set_net_wm_desktop(Display* display, Window window, int workspace_id)
{
        Atom net_wm_desktop = XInternAtom(display, "_NET_WM_DESKTOP", false);
        XChangeProperty(display, window, net_wm_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&workspace_id, 1);
}


void set_net_supported(Display* display, Window root)
{
        Atom net_supported = XInternAtom(display, "_NET_SUPPORTED", false);
        Atom supported_atoms[6];
        supported_atoms[0] = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", false);
        supported_atoms[1] = XInternAtom(display, "_NET_CURRENT_DESKTOP", false);
        supported_atoms[2] = XInternAtom(display, "_NET_DESKTOP_NAMES", false);
        supported_atoms[3] = XInternAtom(display, "_NET_DESKTOP_VIEWPORT", false);
        supported_atoms[4] = XInternAtom(display, "_NET_WM_DESKTOP", false);
        supported_atoms[5] = XInternAtom(display, "_NET_CLIENT_LIST", False);
        XChangeProperty(display, root, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char*)&supported_atoms, sizeof(supported_atoms) / sizeof(supported_atoms[0]));
}


void switch_to_workspace(Display* display, Window root, unsigned short new_workspace, NoteWM_Frame* list)
{
        NoteWM_Frame* frame = list;
        Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", false);
        XChangeProperty(display, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&new_workspace, 1);
        while (frame) {
                if (frame->workspace_id == new_workspace
                && is_valid_window(display, frame->child_window))
                        XMapWindow(display, frame->frame);
                else
                        XUnmapWindow(display, frame->frame);
                frame = frame->next;
        }
        global_state.current_workspace = new_workspace;
}


void focus_window(Display* display, Window window, NoteWM_Frame* list)
{
        NoteWM_Frame* frame = find_frame_by_component(window, list);

        if (frame) {
                /*XRaiseWindow(display, frame->frame);*/
                XSetInputFocus(display, frame->child_window, RevertToPointerRoot, CurrentTime);
        }
}


void map_window(Display* display, Window root, Window window, NoteWM_Frame** list)
{
        Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", false);
        Atom actual_type;
        XWindowAttributes attrs;
        NoteWM_Frame* frame;
        Window* children;
        Window parent;
        int actual_format;
        unsigned long i;
        unsigned long n_items;
        unsigned int n_children;
        unsigned long bytes_after;
        unsigned char* data = NULL;

        XGetWindowAttributes(display, window, &attrs);

        if (attrs.override_redirect || attrs.map_state == IsViewable)
                return;

        if (XQueryTree(display, window, &root, &parent, &children, &n_children)) {
                if (children)
                        XFree(children);
                /* do not frame windows that are children of other windows
                 * (besides the root window) */
                if (parent != root) {
                        map_noframe_window(display, window);
                        return;
                }
        }

        /* get _NET_WM_TYPE atoms */
        XGetWindowProperty(
                display, window, type, 0, (~0L), False, AnyPropertyType,
                &actual_type, &actual_format, &n_items, &bytes_after, &data
        );

        printf("Mapping window %ld\n", window);

        if (data != NULL) {
                for (i = 0; i < n_items; i++) {
                        Atom atom = ((Atom *)data)[i];

                        if (atom == _NET_WM_WINDOW_TYPE_NORMAL
                        || atom == _NET_WM_WINDOW_TYPE_DIALOG) {

                                frame = create_frame(display, window, root);
                                add_frame(frame, list);
                                XFree(data);
                                return;
                        }
                        else {
                                map_noframe_window(display, window);
                                XFree(data);
                                return;
                        }
                }
                XFree(data);
        }
        frame = create_frame(display, window, root);
        add_frame(frame, list);
}


void get_display_dimensions(Display* display, int* width, int* height)
{
        int screen_num = DefaultScreen(display);
        *width = XDisplayWidth(display, screen_num);
        *height = XDisplayHeight(display, screen_num);
}


void update_window_type(Display* display, Window window)
{
        Atom state = get_atom(display, window, "_NET_WM_STATE");
        Atom window_type = get_atom(display, window, "_NET_WM_WINDOW_TYPE");

        if (state == _NET_WM_STATE_FULLSCREEN)
                printf("window wants to be fullscreen\n");
        if (window_type == _NET_WM_WINDOW_TYPE_DIALOG)
                printf("window wants to be dialog\n");
}


Atom get_atom(Display* display, Window window, const char* atom_name)
{
        Atom atom = XInternAtom(display, atom_name, true);
        Atom prop = None;
        Atom dummy_atom = None;
        int dummy_i;
        unsigned long dummy_l;
        unsigned char* data;
        if (XGetWindowProperty(display, window, atom, 0L, sizeof prop, false, XA_ATOM,
                &dummy_atom, &dummy_i, &dummy_l, &dummy_l, &data) == Success && data) {

                prop = *(Atom *)data;
                XFree(data);
        }

        return prop;

}

void grab_change_cursor(Display* display, Window window, Cursor cursor)
{
        XGrabPointer(
                display,
                window,
                true,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync,
                GrabModeAsync,
                None,
                cursor,
                CurrentTime
        );
}


void adopt_existing_windows(Display* display, Window root, NoteWM_Frame** list)
{
        Window returned_root;
        Window returned_parent;
        Window* children;
        unsigned int i;
        unsigned int num_children;

        XQueryTree(display, root, &returned_root, &returned_parent, &children, &num_children);
        for (i = 0; i < num_children; i++) {
                XWindowAttributes attrs;
                XGetWindowAttributes(display, children[i], &attrs);
                if (attrs.map_state == IsViewable) {
                        map_window(display, root, children[i], list);
                }
        }
        XFree(children);
}


void launch_program(const char* program)
{
        pid_t pid = fork();

        if (pid == 0) {
                char* argv[] = {NULL, NULL};
                argv[0] = (char*) program;
                execvp(argv[0], argv);
                perror("execvp");
                exit(1);
        }
        else if (pid < 0) {
                perror("fork");
        }
}


void grab_keys(Display* display, Window root, NoteWM_KeyBinding* keybindings)
{
        unsigned int i = 0;

        XUngrabKey(display, AnyKey, AnyModifier, root);

        while (keybindings[i].key_sym != NoSymbol) {
                KeySym key_sym = keybindings[i].key_sym;
                KeyCode keycode = XKeysymToKeycode(display, key_sym);
                unsigned int modifiers = keybindings[i].modifiers;

                XGrabKey(display, keycode, modifiers, root, True, GrabModeAsync, GrabModeAsync);
                i++;
        }
}


void grab_buttons(Display* display, Window root, NoteWM_MouseBinding* mousebindings)
{
        unsigned int i = 0;
        while (mousebindings[i].button != 0) {
                unsigned int button = mousebindings[i].button;
                unsigned int modifiers = mousebindings[i].modifiers;
                XGrabButton(
                        display, button, modifiers, root, True,
                        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                        GrabModeAsync, GrabModeAsync,
                        None, None
                );
                i++;
        }
}


/* this function is a modified version of gettextprop from dwm */
int get_window_name(Display* display, Window window, char* text, unsigned int size)
{
        XTextProperty name;
        Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", false);
        char** list = NULL;
        int n;

        if (!text || size == 0) {
                return 0;
        }

        text[0] = '\0';

        if ((!XGetTextProperty(display, window, &name, net_wm_name) || !name.nitems)
        && (!XGetTextProperty(display, window, &name, XA_WM_NAME) || !name.nitems)) {
                return 0;
        }

        if (name.encoding == XA_STRING) {
                strncpy(text, (char*)name.value, size - 1);
        }
        else if (XmbTextPropertyToTextList(display, &name, &list, &n) >= Success && n > 0 && *list) {
                strncpy(text, *list, size - 1);
                XFreeStringList(list);
        }

        text[size - 1] = '\0';
        XFree(name.value);

        return 1;
}


void close_frame(Display* display, Window root, NoteWM_Frame* frame, NoteWM_Frame** list)
{
        Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", false);
        Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
        Atom* protocols = NULL;
        int num_protocols;
        int i;
        bool supports_del_window = false;

        if (!XGetWMProtocols(display, frame->child_window, &protocols, &num_protocols)) {
                num_protocols = 0;
        }

        for (i = 0; i < num_protocols; i++) {
                if (protocols[i] == wm_delete_window) {
                        supports_del_window = true;
                        break;
                }
        }

        XFree(protocols);

        if (supports_del_window) {
                /* client supports WM_DELETE_WINDOW so we send a delete window event to it */
                XEvent event;
                event.xclient.type = ClientMessage;
                event.xclient.window = frame->child_window;
                event.xclient.message_type = wm_protocols;
                event.xclient.format = 32;
                event.xclient.data.l[0] = wm_delete_window;
                event.xclient.data.l[1] = CurrentTime;

                XSendEvent(display, frame->child_window, false, NoEventMask, &event);
        }
        else {
                /* otherwise forcefully kill the window */
                XDestroyWindow(display, frame->frame);
                remove_client_window(display, root, frame->child_window);
                remove_frame(display, frame, list);
        }
}


void map_noframe_window(Display* display, Window window)
{
        XWindowAttributes attrs;
        XSizeHints hints;
        long msize;
        int width = 1;
        int height = 1;

        XGetWindowAttributes(display, window, &attrs);

        if (XGetWMNormalHints(display, window, &hints, &msize) && (hints.flags & PSize) && hints.width != 0) {
                width = hints.width;
                height = hints.height;
        } else if (attrs.width != 0 && attrs.height != 0) {
                width = attrs.width;
                height = attrs.height;
        }
        else {
                XResizeWindow(display, window, width, height);
        }

        XSelectInput(display, window, StructureNotifyMask | SubstructureRedirectMask);
        XMapWindow(display, window);
}


int xerror_handler(Display* display, XErrorEvent* error)
{
        char error_msg[120];
        XGetErrorText(display, error->error_code, error_msg, sizeof(error_msg));
        fprintf(stderr, "X Error: %s\n", error_msg);
        /*exit(1);*/
        return 0;
}


int ignore_xerrors(Display* display, XErrorEvent* error)
{
        (void) display; (void) error;
        return 0;
}


bool is_valid_window(Display* display, Window window)
{
        XWindowAttributes attrs;
        int result;
        XSetErrorHandler(ignore_xerrors);
        result = XGetWindowAttributes(display, window, &attrs);
        XSync(display, false);
        XSetErrorHandler(xerror_handler);
        return (result != 0);
}
