extern int cp15_cacheon;

struct
{
        uint32_t ctrl;
        uint32_t cache,update,disrupt;
} arm3cp;
