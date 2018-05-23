enum
{
        FDC_WD1770,
        FDC_82C711
};

extern int fdctype;

float config_get_float(char *head, char *name, float def);
int config_get_int(char *head, char *name, int def);
char *config_get_string(char *head, char *name, char *def);
void config_set_float(char *head, char *name, float val);
void config_set_int(char *head, char *name, int val);
void config_set_string(char *head, char *name, char *val);

int config_free_section(char *head);

void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

char *get_filename(char *s);
void append_filename(char *dest, char *s1, char *s2, int size);
void append_slash(char *s, int size);
void put_backslash(char *s);
char *get_extension(char *s);

void config_load(char *fn);
void config_save(char *fn);
void config_dump();

void loadconfig();
void saveconfig();
