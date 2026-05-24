void o_line(void);
void o_par_n(int n);
void o_par(void);
void o_begin_box(char *boxclass);
void o_end_box(void);
void o_space(void);
void o_space_n(int n);
void o_nospace(void);
void o_nbsp(void);
void o_sync(void);
void o_set_style(int style);
void o_set_style_colors(int style, int fg, int bg);
void o_set_upper(void);
void o_print_word(const char *utf8);
void o_print_opaque_word(const char *utf8);
void o_print_str(const char *utf8);
void o_begin_link(const char *utf8);
void o_end_link();
void o_begin_self_link();
void o_end_self_link();
void o_progress_bar(int position, int total);
void o_clear(int all);
void o_post_input(int external_lf);
void o_reset();
void o_leave_all(void);
void o_cleanup(void);
int o_get_width(void);
int o_get_height(void);
int o_is_pretty(void);

#define FORMAT_DEFAULT 0
#define FORMAT_ALWAYS 1
#define FORMAT_NEVER 2

struct output_config {
	int return_value; // Return value set by (quit $)
	int dfrotz_quirks; // Emulate dfrotz as perfectly as possible, including quirks
	int numbered_levels; // Show trace depth with numbers instead of bars
	int formatting; // Should debugger use ANSI formatting? (FORMAT_*)
	int tag_lines; // Put two chars in front of every line to mark input requests
	int force_width; // Force output width
	int force_height; // Force output height
	int transcripting; // Make (transcript active) succeed
};
