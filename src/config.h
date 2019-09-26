enum
{
        FDC_WD1770,
        FDC_82C711
};

extern int fdctype;

enum
{
        ROM_ARTHUR_030,
        ROM_ARTHUR_120,
        ROM_RISCOS_200,
        ROM_RISCOS_201,
        ROM_RISCOS_300,
        ROM_RISCOS_310,
        ROM_RISCOS_311,
        ROM_RISCOS_319,
        
        ROM_MAX
};

extern int romset_available_mask;

char *config_get_romset_name(int romset);
char *config_get_cmos_name(int romset, int fdctype);
        
enum
{
        MONITOR_STANDARD,
        MONITOR_MULTISYNC,
        MONITOR_VGA,
        MONITOR_MONO
};

extern int monitor_type;

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
extern uint32_t unique_id;
extern char joystick_if[16];
