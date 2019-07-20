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
        /*Initialise podule instance. Any instance data should be stored
          in podule->p
          Returns zero on success, non-zero on error.*/
        int (*init)(struct podule_t *podule);
        /*Close podule instance, and free any instance data*/
        void (*close)(struct podule_t *podule);

        void (*write_b)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val);
        void (*write_w)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val);
        void (*write_l)(struct podule_t *podule, podule_io_type type, uint32_t addr, uint32_t val);
        uint8_t  (*read_b)(struct podule_t *podule, podule_io_type type, uint32_t addr);
        uint16_t (*read_w)(struct podule_t *podule, podule_io_type type, uint32_t addr);
        uint32_t (*read_l)(struct podule_t *podule, podule_io_type type, uint32_t addr);

        void (*reset)(struct podule_t *podule);

        int (*run)(struct podule_t *podule, int timeslice_us);
} podule_functions_t;

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
} podule_header_t;

typedef struct podule_t
{
        const podule_header_t *header;
        void *p;
} podule_t;

typedef struct podule_callbacks_t
{
        /*Update IRQ state*/
        void (*set_irq)(podule_t *podule, int state);
        /*Update FIQ state*/
        void (*set_fiq)(podule_t *podule, int state);

        void (*set_timer_delay_us)(podule_t *podule, int delay_us);
        int (*get_timer_remaining_us)(podule_t *podule);
        void (*stop_timer)(podule_t *podule);
        
        int (*config_get_int)(podule_t *podule, const char *name, int def);
        const char *(*config_get_string)(podule_t *podule, const char *name, const char *def);
        void (*config_set_int)(podule_t *podule, const char *name, int val);
        void (*config_set_string)(podule_t *podule, const char *name, char *val);
} podule_callbacks_t;

/*Main entry point to be implemented by the podule. Podule should store
  callbacks and path locally, and return a pointer to the podule header.

  If the podule can't be initialised (eg due to a missing file), this
  function should return NULL. The podule will then be unavailable to
  the user.*/
const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);

#endif /*PODULE_API_H*/
