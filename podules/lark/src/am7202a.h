typedef struct am7202a_t
{
        int rp, wp;
        uint16_t data[1024];
        
        void *p;
        
        void (*set_ef)(int state, void *p);
        void (*set_ff)(int state, void *p);
        void (*set_hf)(int state, void *p);
} am7202a_t;

void am7202a_init(am7202a_t *fifo, void (*set_ef)(int state, void *p), void (*set_ff)(int state, void *p), void (*set_hf)(int state, void *p), void *p);
void am7202a_reset(am7202a_t *fifo);
void am7202a_write(am7202a_t *fifo, uint16_t val);
uint16_t am7202a_read(am7202a_t *fifo);
