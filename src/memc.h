void initmemc();

extern int memc_videodma_enable;
extern int memc_refreshon;
extern int memc_refresh_always;
extern int memc_is_memc1;
extern int memc_type;

extern uint32_t sstart, ssend, sptr;
extern uint32_t spos,sendN,sstart2;
extern int nextvalid;
extern int sdmaena;


extern int memc_dma_sound_req;
extern uint64_t memc_dma_sound_req_ts;
extern int memc_dma_video_req;
extern uint64_t memc_dma_video_req_ts;
extern uint64_t memc_dma_video_req_start_ts;
extern uint64_t memc_dma_video_req_period;
extern int memc_dma_cursor_req;
extern uint64_t memc_dma_cursor_req_ts;
