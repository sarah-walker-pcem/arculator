typedef struct d71071l_t
{
        struct
        {
                uint32_t addr_base,  addr_cur;
                int32_t  count_base, count_cur;

                uint8_t mode;
        } channel[4];

        int base;
        int selch;

        uint8_t mask;
        
        podule_t *podule;
} d71071l_t;

void d71071l_init(d71071l_t *dma, podule_t *podule);
void d71071l_write(d71071l_t *dma, uint32_t addr, uint8_t val);
uint8_t d71071l_read(d71071l_t *dma, uint32_t addr);

int dma_read(d71071l_t *dma, int channel);
int dma_write(d71071l_t *dma, int channel, uint8_t val);
