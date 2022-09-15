#define ROPKIT_BINLOAD_TEXTOFFSET 0
#define ROPKIT_BINLOAD_SIZE 0x1000
#define ROPKIT_BEFOREJUMP_CACHEBUFADDR 0x15800000
#define ROPKIT_BEFOREJUMP_CACHEBUFSIZE 0x800000
#define ROPKIT_LINEARMEM_REGIONBASE 0x14000000
#define ROPKIT_TMPDATA 0x0FFFC000
#define ROPKIT_LINEARMEM_BUF 0x15000000
#define ROPKIT_ENABLETERMINATE_THREADS

#define GAMESTARTBUFFER (0x1424F524)

#define ROPBUF (GAMESTARTBUFFER + 8)
#define ROPKIT_BINLOAD_ADDR 0x14254000
#define MINIAPP_ROPKIT_LOADADDR (GAMESTARTBUFFER + 0x37C * 24)

#define MINIAPP_ROPKIT_SIZE_PTR GAMESTARTBUFFER
#define MINIAPP_ROPKIT_REPEATTIMES_PTR (GAMESTARTBUFFER + 4)

#define MINIAPP_ROPKIT_DEST_ADDR ROPKIT_LINEARMEM_BUF
#define MINIAPP_OTHERAPP_TEXT_ADDR (0x00290000)
#define MINIAPP_MAXROPKIT_SIZE (0xC000)

#define MINIAPP_WORKBUFFER_OFFSET ((0x16000000) - ROPKIT_LINEARMEM_REGIONBASE)