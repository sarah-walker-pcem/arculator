extern int cp15_cacheon;

typedef struct arm3cp_t
{
	uint32_t ctrl;
	uint32_t cache,update,disrupt;
} arm3cp_t;

extern arm3cp_t arm3cp;
