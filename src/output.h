void o_line(void);
void o_par_n(int n);
void o_par(void);
void o_begin_box(char *boxclass);
void o_end_box(void);
void o_space(void);
void o_space_n(int n);
void o_nospace(void);
void o_sync(void);
void o_set_style(int style);
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
void o_reset(int force_width, int quirks);
void o_leave_all(void);
void o_cleanup(void);
int o_get_width(void);
