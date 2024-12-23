/*
   Header file for Windows specific Glk features.
   Glk API version 0.7.5, WinGlk release 1.48.
*/

#ifndef WINGLK_H_
#define WINGLK_H_

#include <wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function to be implemented in the Glk program. */
int winglk_startup_code(const char* cmdline);

/* Windows Glk specific functions. */
strid_t winglk_stream_open_resource(const char* name, const char* type, glui32 rock);
void winglk_app_set_name(const char* name);
void winglk_window_set_title(const char* title);
void winglk_set_resource_directory(const char* dir);
const char* winglk_get_initial_filename(const char* cmdline, const char* title, const char* filter);
void winglk_set_gui(unsigned int id);
void winglk_load_config_file(const char* gamename);
void* winglk_get_resource_handle(void);
void winglk_set_about_text(const char* text);
void winglk_set_menu_name(const char* name);
void winglk_set_help_file(const char* filename);
frefid_t winglk_fileref_create_by_name(glui32 usage, char *name, glui32 rock, int validate);
void winglk_show_game_dialog(void);

/* Windows Glk specific events. */
#define winglk_evtype_GuiInput (0x80000000)

/* Unofficial Glk extensions. */
void sglk_set_basename(char *s);

/* Gargoyle Glk extensions. */

#define GLK_MODULE_GARGLKTEXT
#define gestalt_GarglkText (0x1100)

void garglk_set_zcolors(glui32 fg, glui32 bg);
void garglk_set_zcolors_stream(strid_t str, glui32 fg, glui32 bg);
void garglk_set_reversevideo(glui32 reverse);
void garglk_set_reversevideo_stream(strid_t str, glui32 reverse);

#define zcolor_Default (-1)
#define zcolor_Current (-2)

#ifdef __cplusplus
}
#endif

#endif /* WINGLK_H_ */
