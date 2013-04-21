typedef struct
{
        struct
        {
        	unsigned long type,rd,len;
        	int pos;
        } track[160],sdtrack[160],hdtrack[160];
        gzFile *f;
} MFM;
