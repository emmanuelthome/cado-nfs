/* Max length of the line of the current shell */
#define PREEMPT_S_CMD (1<<16)

/* Length of preempt buffer. Must be a power of 2. */
#define PREEMPT_BUF (1<<22)

/* Length of one write in preempt buffer. Between 64 and 1024 Ko
   seems the best. */
#define PREEMPT_ONE_READ (PREEMPT_BUF>>2)

typedef struct {
  char **files, *buf;
  volatile char *pcons, *pprod;
  volatile unsigned int end;
} __preempt_t;
typedef __preempt_t preempt_t[1];

/* Return a unix commands list with antebuffer. Example:
   antebuffer X file_relation1 | cat -
   antebuffer X file_relation2.gz file_relation3.gz | gzip -dc -
   antebuffer X file_relation4.bz2 file_relation5.bz2 | bzip2 -dc -
   [empty string]
*/
extern char ** preempt_open_compressed_rs (char * rep_cado, char ** ficname);
