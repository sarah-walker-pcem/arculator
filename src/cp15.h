struct _arm3cp
{
        uint32_t ctrl;
        uint32_t cache,update,disrupt;
};

extern int cp15_cacheon;
extern struct _arm3cp arm3cp;
