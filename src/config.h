enum
{
        FDC_WD1770,
        FDC_82C711
};

extern int fdctype;

extern float config_get_float(int is_global, const char *head, const char *name, float def);
extern int config_get_int(int is_global, const char *head, const char *name, int def);
extern const char *config_get_string(int is_global, const char *head, const char *name, const char *def);
extern void config_set_float(int is_global, const char *head, const char *name, float val);
extern void config_set_int(int is_global, const char *head, const char *name, int val);
extern void config_set_string(int is_global, const char *head, const char *name, char *val);

extern int config_free_section(int is_global, const char *name);

extern void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

extern char *get_filename(char *s);
extern void append_filename(char *dest, const char *s1, const char *s2, int size);
extern void append_slash(char *s, int size);
extern void put_backslash(char *s);
extern char *get_extension(char *s);

extern void config_load(int is_global, char *fn);
extern void config_save(int is_global, char *fn);
extern void config_dump(int is_global);

extern void loadconfig();
extern void saveconfig();

#define CFG_MACHINE 0
#define CFG_GLOBAL  1

extern char machine_config_name[256];
extern char machine_config_file[256];
extern char hd_fn[2][512];
extern int hd_spt[2], hd_hpc[2], hd_cyl[2];
extern char machine[7];
