#ifndef PODULE_API_H
#define PODULE_API_H

#define PODULE_API_VERSION 1

struct podule_t;

typedef enum podule_io_type
{
        /*Access is in IOC space*/
        PODULE_IO_TYPE_IOC = 0,
        /*Access is in MEMC space*/
        PODULE_IO_TYPE_MEMC,
        /*Access is in EASI space (not Archimedes)*/
        PODULE_IO_TYPE_EASI
} podule_io_type;

typedef struct podule_functions_t
{
        /*init() - Initialise podule instance
          @podule: podule pointer
          
          Returns zero on success, non-zero on error.

          Any instance data should be stored in podule->p*/
        int (*init)(struct podule_t *podule);
        /*close() - Close podule instance
          @podule: podule pointer
          
          This function is responsible for freeing any instance data in podule->p*/
        void (*close)(struct podule_t *podule);

        /*write_b() - Write byte to podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to write to
          @val:    value to write*/
        void (*write_b)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val);
        /*write_w() - Write word to podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to write to
          @val:    value to write*/
        void (*write_w)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val);
        /*write_l() - Write long word to podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to write to
          @val:    value to write*/
        void (*write_l)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint32_t val);
        /*read_b() - Read byte from podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to read from

          Returns value read from podule space*/
        uint8_t  (*read_b)(struct podule_t *podule, podule_io_type type, uint32_t addr);
        /*read_w() - Read word from podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to read from

          Returns value read from podule space*/
        uint16_t (*read_w)(struct podule_t *podule, podule_io_type type, uint32_t addr);
        /*read_l() - Read long from podule space
          @podule: podule pointer
          @type:   type of access (see @podule_io_type)
          @addr:   address to read from

          Returns value read from podule space*/
        uint32_t (*read_l)(struct podule_t *podule, podule_io_type type, uint32_t addr);

        /*reset() - Reset podule
          @podule: podule pointer*/
        void (*reset)(struct podule_t *podule);

        /*run() - Callback from podule timer
          @podule:       podule pointer
          @timeslice_us: time expired since last run() call
          
          Returns delay (in us) to reload timer with. Non-zero delay can be used
          to create a free-running timer. Returning zero will stop the timer.
          
          When using a free-running timer, @timeslice_us must be used to check
          how much time has actually passed since the last callback. Emulators
          should support at least a 1ms timer granularity. Arculator will
          generally implement a one instruction granularity*/
        int (*run)(struct podule_t *podule, int timeslice_us);
} podule_functions_t;

#define CONFIG_STRING 0
#define CONFIG_INT 1
#define CONFIG_BINARY 2
#define CONFIG_SELECTION 3
#define CONFIG_SELECTION_STRING 4
#define CONFIG_BUTTON 5

/*Control can not be altered by user*/
#define CONFIG_FLAGS_DISABLED (1 << 0)
/*'name' entry should be prefixed with the prefix passed to config_open()*/
#define CONFIG_FLAGS_NAME_PREFIXED (1 << 1)

typedef struct podule_config_selection_t
{
        /*User-visible description of selection*/
        const char *description;
        /*Value this selection represents. Valid when parent control is
          CONFIG_SELECTION*/
        int value;
        /*Value this selection represents. Valid when parent control is
          CONFIG_SELECTION_STRING*/
        char *value_string;
} podule_config_selection_t;

typedef struct podule_config_item_t
{
        /*Name of config variable. May be NULL if this is a GUI field that isn't
          backed by a config variable. May be prefixed if CONFIG_FLAGS_NAME_PREFIXED
          is set.
          
          If this is non-NULL then any change to the item will cause a config
          update and an emulator reset*/
        const char *name;
        /*User-visible description of item*/
        const char *description;
        /*ID to be used with config_get_current()/config_set_current()*/
        const int id;
        /*Type of control to be presented to the user*/
        const int type;
        /*Combination of CONFIG_FLAGS_* */
        const int flags;
        
        const char *default_string;
        int default_int;
        podule_config_selection_t *selection;
        
        /*function() - Callback function for control
          @window_p: window pointer
          @item:     item pointer
          @new_val:  new value (if valid)
          
          If this item has a type other than CONFIG_BUTTON then this function
          indicates that the user has altered the value of this item, and
          @new_val is valid.

          If the type is CONFIG_BUTTON, then this function indicates that the
          user has clicked on the button, and @new_val is NULL.

          Returns non-zero if this callback has made any changes to other items
          in this dialogue, and that the dialogue needs to be updated.*/
        int (*function)(void *window_p, const struct podule_config_item_t *item, void *new_data);
} podule_config_item_t;

typedef struct podule_config_t
{
        /*Title of dialogue window*/
        const char *title;
        /*init() - function called on config dialogue open
          @window_p: window pointer*/
        void (*init)(void *window_p);
        /*close() - function called if user pressed OK
          @window_p: window pointer
          
          Returns non-zero if the emulated machine needs to be reset*/
        int (*close)(void *window_p);
        
        podule_config_item_t items[];
} podule_config_t;

/*Only one instance of this podule allowed per system*/
#define PODULE_FLAGS_UNIQUE (1 << 0)

typedef struct podule_header_t
{
        /*Podule API version. Should always be set to PODULE_API_VERSION*/
        const uint32_t version;
        /*Podule flags. See PODULE_FLAGS_* */
        const uint32_t flags;
        /*Short internal name, no spaces. Should be <= 15 characters*/
        const char *short_name;
        /*Long user visible name*/
        const char *name;

        const podule_functions_t functions;
        
        podule_config_t *config;
} podule_header_t;

typedef struct podule_t
{
        /*Pointer to header for this podule*/
        const podule_header_t *header;
        /*Pointer to private instance data for this podule*/
        void *p;
} podule_t;

#define CONFIG_FILESEL_LOAD (0)
#define CONFIG_FILESEL_SAVE (1 << 0)

typedef struct podule_callbacks_t
{
        /*set_irq() - Update IRQ state
          @podule: podule pointer
          @state:  new IRQ state*/
        void (*set_irq)(podule_t *podule, int state);
        /*set_fiq() - Update FIQ state
          @podule: podule pointer
          @state:  new FIQ state*/
        void (*set_fiq)(podule_t *podule, int state);

        /*set_timer_delay_us() - Set timer delay until podule run() function called
          @podule:   podule pointer
          @delay_us: desired delay in us*/
        void (*set_timer_delay_us)(podule_t *podule, int delay_us);
        /*get_timer_remaining_us() - Return time until timer expires and run()
                                     is called
          @podule: podule pointer

          Returns: Time until timer expires in us, or zero if timer already
          expired*/
        int (*get_timer_remaining_us)(podule_t *podule);
        /*stop_timer() - Stop timer from running. run() will not be called
          @podule: podule pointer*/
        void (*stop_timer)(podule_t *podule);

        /*config_get_int() - Read integer variable from podule configuration
          @podule: podule pointer
          @name:   name of variable
          @def:    default value to return if variable does not exist
          
          Returns: Value of variable, or @def if variable does not exist*/
        int (*config_get_int)(podule_t *podule, const char *name, int def);
        /*config_get_string() - Read string variable from podule configuration
          @podule: podule pointer
          @name:   name of variable
          @def:    default string to return if variable does not exist

          Returns: Pointer to variable string, or @def if variable does not exist*/
        const char *(*config_get_string)(podule_t *podule, const char *name, const char *def);
        /*config_set_int() - Write integer variable to podule configuration
          @podule: podule pointer
          @name:   name of variable
          @val:    value to write*/
        void (*config_set_int)(podule_t *podule, const char *name, int val);
        /*config_set_string() - Write string variable to podule configuration
          @podule: podule pointer
          @name:   name of variable
          @val:    Pointer to string to write*/
        void (*config_set_string)(podule_t *podule, const char *name, char *val);
        
        /*config_open() - Open new configuration dialogue.
          @window_p: parent window pointer
          @config:   config to open
          @prefix:   prefix for any prefixed config items. May be NULL
          
          Returns: Non-zero if dialogue closed with OK, zero otherwise

          Should only be used from a callback from an already open dialogue*/
        int (*config_open)(void *window_p, podule_config_t *config,
                        const char *prefix);
        /*config_file_selector() - Open a file selector
          @window_p:     parent window pointer
          @title:        window title
          @default_path: Default path for file. May be NULL
          @default_fn:   Default filename for file. May be NULL
          @default_ext:  Default extension for file. May be NULL
          @wildcard:
          @dest:         String to hold destination file path/name
          @dest_len:     Length of @dest, including NULL terminator
          @flags:        Combination of CONFIG_FILESEL_*
          
          Returns: Non-zero if file selected, zero if user dismissed selector
         */
        int (*config_file_selector)(void *window_p, const char *title,
                        const char *default_path, const char *default_fn,
                        const char *default_ext, const char *wildcard,
                        char *dest, int dest_len, int flags);
        /*config_get_current() - Get the value of a config item in the currently
                                 open dialogue
          @window_p: window pointer
          @id:       ID of config item

          Returns: Pointer to value, or NULL if item not found*/
        void *(*config_get_current)(void *window_p, int id);
        /*config_get_current() - Set the value of a config item in the currently
                                 open dialogue
          @window_p: window pointer
          @id:       ID of config item
          @val:      Pointer to new value*/
        void (*config_set_current)(void *window_p, int id, void *val);
} podule_callbacks_t;

/*Main entry point to be implemented by the podule. Podule should store
  callbacks and path locally, and return a pointer to the podule header.

  If the podule can't be initialised (eg due to a missing file), this
  function should return NULL. The podule will then be unavailable to
  the user.*/
const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);

#endif /*PODULE_API_H*/
