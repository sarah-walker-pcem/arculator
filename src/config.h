enum
{
        FDC_WD1770,
        FDC_82C711
};

extern int fdctype;

extern float config_get_float(const char *head, const char *name, float def);
extern int config_get_int(const char *head, const char *name, int def);
extern const char *config_get_string(const char *head, const char *name, const char *def);
extern void config_set_float(const char *head, const char *name, float val);
extern void config_set_int(const char *head, const char *name, int val);
extern void config_set_string(const char *head, const char *name, char *val);

extern int config_free_section(const char *name);

extern void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

extern char *get_filename(char *s);
extern void append_filename(char *dest, const char *s1, const char *s2, int size);
extern void append_slash(char *s, int size);
extern void put_backslash(char *s);
extern char *get_extension(char *s);

extern void config_load(char *fn);
extern void config_save(char *fn);
extern void config_dump();

extern void loadconfig();
extern void saveconfig();
