#include "notewm.h"

unsigned long color_to_ulong(const char* color_str);

unsigned long color_to_ulong(const char* color_str)
{
        if (color_str[0] == '#') {
                color_str++;
        }

        return strtoul(color_str, NULL, 16);
}

int conf_handler(void* user, const char* section, const char* name, const char* value)
{
        NoteWM_Config* pconfig = (NoteWM_Config*)user;

        #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

        if (MATCH("Colors", "title-bar"))
                pconfig->title_bar_color = color_to_ulong(value);
        else if (MATCH("Colors", "border"))
                pconfig->border_color = color_to_ulong(value);
        else if (MATCH("Colors", "background"))
                pconfig->bg_color = color_to_ulong(value);
        else if (MATCH("Colors", "foreground"))
                pconfig->fg_color = color_to_ulong(value);
        else if (MATCH("Colors", "button-close"))
                pconfig->close_color = color_to_ulong(value);
        else if (MATCH("Colors", "button-expand"))
                pconfig->expand_color = color_to_ulong(value);
        else if (MATCH("Colors", "button-split"))
                pconfig->split_color = color_to_ulong(value);
        else
                return 0;
        return 1;
}
