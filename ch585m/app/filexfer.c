/********************************** (C) COPYRIGHT *******************************
 * File Name          : filexfer.c
 * Description        : Synthetic BMP file generator, streamed over EP2
 *                       bulk IN for USB throughput testing. Identical
 *                       logic to ../../ch32l103/User/filexfer.c, adapted
 *                       to the CH592F's USBD_EP2_* driver API.
 *
 *  Nothing is stored in flash for the pixel data itself: only two small
 *  lookup tables (blue-by-column, green-by-row) are precomputed once at
 *  init, and per-byte pixel-walk state (row/col/channel) is advanced
 *  incrementally - no division/modulo anywhere in steady-state operation.
 *
 *  Producer/consumer ring buffer: packet generation is done by
 *  FileXfer_Pump(), called repeatedly from the idle main loop, which fills
 *  a ring of pre-built 64-byte packets ahead of time. The USB interrupt
 *  handler's fill callback then just memcpy()s the next ready packet -
 *  minimal work in interrupt context. If the ring ever runs dry (e.g.
 *  right after FileXfer_Start(), before the main loop has had a chance to
 *  pump), the callback falls back to generating that one packet
 *  synchronously - correctness is preserved either way, only speed differs.
 *
 *  BMP layout (24-bit uncompressed, BITMAPINFOHEADER, 54-byte header):
 *  rows are stored bottom-to-top, each row BGR8:8:8, row size already a
 *  multiple of 4 bytes (no padding needed for width=1024).
 *******************************************************************************/
#include "filexfer.h"
#include "ch585_usbhs_device.h"
#include <string.h>

/* Ring buffer of pre-built packets. 32 slots * 64 bytes = 2KB, a small
 * fraction of the CH592F's RAM. Must be a power of two for cheap masking
 * instead of modulo. */
#define RING_SLOTS   16u  /* 16 x 512B = 8KB ring */
#define RING_MASK    (RING_SLOTS - 1u)

static uint8_t  s_ringData[RING_SLOTS][DEF_USBD_UEP2_SIZE];
static uint8_t  s_ringLen[RING_SLOTS];
static volatile uint32_t s_ringHead = 0; /* producer: slots produced so far   */
static volatile uint32_t s_ringTail = 0; /* consumer: slots consumed so far   */

/* Set by the USB ISR when it had to NAK because the ring ran dry; the
 * main-loop pump re-arms EP2 after refilling. Generation must NEVER happen
 * in interrupt context: at transfer rates an in-ISR generator burns more
 * CPU than the main loop has left, starving the pump so the ring stays
 * empty forever - the device deadlocks itself at ~1/4 of the achievable
 * throughput (measured 244 KB/s before this was enforced). */
static volatile uint8_t s_needArm = 0;

/* Set by the USB ISR (START_FILE_XFER, EP0 SETUP context) to request a
 * restart; the main-loop pump performs the state reset + prefill + arm,
 * keeping all generator/ring state single-threaded in main context. */
static volatile uint8_t s_startReq = 0;


/* True while a download should be streaming (set on START_FILE_XFER,
 * cleared once the last byte has left the ring). Lets the pump's
 * self-heal path distinguish "idle" from "stalled". */
static volatile uint8_t s_active = 0;

/* Pixel-walk generation position. Owned exclusively by FileXfer_Pump()
 * (main-loop context): the ISR never generates, so no locking needed. */
static uint32_t s_genPos;
static uint32_t s_col;         /* 0 .. FILEXFER_IMG_WIDTH-1 */
static uint32_t s_channel;     /* 0=Blue, 1=Green, 2=Red    */
static uint32_t s_actualRow;   /* counts down from HEIGHT-1 */

/* Checkerboard: 8x8 black/white squares. Two precomputed rows (one
 * starting black, one white), each 3*WIDTH bytes; every image row is a
 * memcpy from one of them - generation cost drops from a per-byte LUT
 * walk to one memcpy per 3 KB row. */
#define SQ 8
static uint8_t s_rowA[FILEXFER_IMG_WIDTH * 3];
static uint8_t s_rowB[FILEXFER_IMG_WIDTH * 3];
static uint8_t s_header[FILEXFER_BMP_HEADER_LEN];

/* Builds the 54-byte BMP header into 'hdr'. */
static void BuildBmpHeader(uint8_t *hdr)
{
    uint32_t fileSize  = FILEXFER_TOTAL_SIZE;
    uint32_t dataOffset = FILEXFER_BMP_HEADER_LEN;
    uint32_t dibSize    = 40;
    uint32_t width      = FILEXFER_IMG_WIDTH;
    uint32_t height     = FILEXFER_IMG_HEIGHT;
    uint32_t imgSize    = FILEXFER_IMG_WIDTH * FILEXFER_IMG_HEIGHT * 3u;

    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2]  = (uint8_t)(fileSize);       hdr[3]  = (uint8_t)(fileSize >> 8);
    hdr[4]  = (uint8_t)(fileSize >> 16); hdr[5]  = (uint8_t)(fileSize >> 24);
    hdr[6] = 0; hdr[7] = 0; hdr[8] = 0; hdr[9] = 0;                          /* reserved */
    hdr[10] = (uint8_t)(dataOffset);       hdr[11] = (uint8_t)(dataOffset >> 8);
    hdr[12] = (uint8_t)(dataOffset >> 16); hdr[13] = (uint8_t)(dataOffset >> 24);
    hdr[14] = (uint8_t)(dibSize);       hdr[15] = (uint8_t)(dibSize >> 8);
    hdr[16] = (uint8_t)(dibSize >> 16); hdr[17] = (uint8_t)(dibSize >> 24);
    hdr[18] = (uint8_t)(width);       hdr[19] = (uint8_t)(width >> 8);
    hdr[20] = (uint8_t)(width >> 16); hdr[21] = (uint8_t)(width >> 24);
    hdr[22] = (uint8_t)(height);       hdr[23] = (uint8_t)(height >> 8);
    hdr[24] = (uint8_t)(height >> 16); hdr[25] = (uint8_t)(height >> 24);
    hdr[26] = 1; hdr[27] = 0;   /* planes = 1 */
    hdr[28] = 24; hdr[29] = 0;  /* bits per pixel = 24 */
    hdr[30] = 0; hdr[31] = 0; hdr[32] = 0; hdr[33] = 0; /* compression = 0 (BI_RGB) */
    hdr[34] = (uint8_t)(imgSize);       hdr[35] = (uint8_t)(imgSize >> 8);
    hdr[36] = (uint8_t)(imgSize >> 16); hdr[37] = (uint8_t)(imgSize >> 24);
    hdr[38] = 0; hdr[39] = 0; hdr[40] = 0; hdr[41] = 0; /* x pixels/meter */
    hdr[42] = 0; hdr[43] = 0; hdr[44] = 0; hdr[45] = 0; /* y pixels/meter */
    hdr[46] = 0; hdr[47] = 0; hdr[48] = 0; hdr[49] = 0; /* colors used */
    hdr[50] = 0; hdr[51] = 0; hdr[52] = 0; hdr[53] = 0; /* important colors */
}

/* Advances the incremental pixel-walk state by exactly one byte. */
static inline void AdvancePixelState(void)
{
    s_channel++;
    if (s_channel >= 3u)
    {
        s_channel = 0;
        s_col++;
        if (s_col >= FILEXFER_IMG_WIDTH)
        {
            s_col = 0;
            s_actualRow--;
        }
    }
}

/* Generates up to 'maxlen' bytes starting at the current s_genPos into
 * 'buf', advancing s_genPos and the pixel-walk state accordingly.
 * Returns the number of bytes written (0 only if s_genPos was already
 * at end-of-file when called). */
static uint16_t GenerateChunk(uint8_t *buf, uint16_t maxlen)
{
    uint32_t remain = FILEXFER_TOTAL_SIZE - s_genPos;
    uint16_t n = (remain < maxlen) ? (uint16_t)remain : maxlen;
    uint16_t i = 0;

    while (i < n)
    {
        uint32_t pos = s_genPos + i;
        if (pos < FILEXFER_BMP_HEADER_LEN)
        {
            buf[i++] = s_header[pos];
            continue;
        }
        uint32_t pix = pos - FILEXFER_BMP_HEADER_LEN;
        uint32_t row = pix / (FILEXFER_IMG_WIDTH * 3u);
        uint32_t off = pix % (FILEXFER_IMG_WIDTH * 3u);
        const uint8_t *src = ((row / SQ) & 1u) ? s_rowB : s_rowA;
        uint16_t avail = (uint16_t)(FILEXFER_IMG_WIDTH * 3u - off);
        if (avail > (uint16_t)(n - i))
        {
            avail = (uint16_t)(n - i);
        }
        memcpy(&buf[i], &src[off], avail);
        i += avail;
    }
    s_genPos += n;
    return n;
}

/* Called from the idle main loop: fills the ring buffer with as many
 * freshly-generated packets as there is room for, then re-arms EP2 if the
 * ISR previously had to NAK on an empty ring. Cheap to call often - it's
 * a no-op once the ring is full or the file is exhausted. */
void FileXfer_Pump(void)
{
    if (s_startReq)
    {
        s_startReq = 0;
        s_genPos    = 0;
        s_col       = 0;
        s_channel   = 0;
        s_actualRow = FILEXFER_IMG_HEIGHT - 1u;
        s_ringHead  = 0;
        s_ringTail  = 0;
        s_needArm   = 1; /* arm the first packet once the prefill below ran */
        s_active    = 1;
    }

    while ((s_ringHead - s_ringTail) < (RING_SLOTS - 1)) /* keep one slot of
    * headroom: a slot handed to DMA stays unread until IN-complete */
    {
        uint32_t idx;
        uint16_t len;

        if (s_genPos >= FILEXFER_TOTAL_SIZE)
        {
            break;
        }

        idx = s_ringHead & RING_MASK;
        len = GenerateChunk(s_ringData[idx], DEF_USBD_UEP2_SIZE);
        s_ringLen[idx] = (uint8_t)len;
        s_ringHead++;
    }

    /* Transfer finished once the generator is exhausted and drained. */
    if (s_active && (s_genPos >= FILEXFER_TOTAL_SIZE) && (s_ringHead == s_ringTail))
    {
        s_active = 0;
    }

    if (s_needArm && (s_ringHead != s_ringTail))
    {
        s_needArm = 0;
        USBD_EP2_StartTransfer(); /* pulls one slot via the callback + arms */
    }
    else if (s_active)
    {
        /* Self-heal, part 1: if the fast path above ever misses (lost
         * s_needArm), re-attempt the arm periodically.
         * USBD_EP2_StartTransfer() is idempotent - it no-ops unless EP2 is
         * currently NAKed. */
        static uint32_t s_heal = 0;
        if (++s_heal >= 1000u)
        {
            s_heal = 0;
            USBD_EP2_StartTransfer();
        }

        /* Self-heal, part 2: data-toggle resync. If the host re-opens the
         * device (browser reload, libusb close/open), the kernel resets its
         * host-side DATA0/1 toggle WITHOUT any wire traffic, while ours
         * keeps its value (RB_UC auto-toggle only advances on ACKed
         * transfers). Every packet is then silently discarded and the
         * transfer deadlocks with zero ring progress. Detect exactly that
         * (transfer active + no ring progress for ~1s) and flip our toggle:
         * the pending/next packet is then (re)sent with the now-matching
         * PID. A false flip (host merely idle) self-corrects on the next
         * interval, since the same detector flips back. */
        static uint32_t s_stuck = 0;
        static uint32_t s_lastTail = 0;
        if (s_ringTail != s_lastTail)
        {
            s_lastTail = s_ringTail;
            s_stuck = 0; /* packets are being delivered: healthy */
        }
        else if (++s_stuck >= 100000u)
        {
            s_stuck = 0;
            R8_UEP2_CTRL ^= RB_UEP_T_TOG;
            USBD_EP2_StartTransfer(); /* re-arm too, in case EP2 went NAK */
        }
    }
}

/* USB-interrupt-context callback: pops the next ready packet from the
 * ring (fast memcpy only). If the ring is dry it returns 0, which NAKs
 * the endpoint - the host retries while the main-loop pump refills and
 * re-arms from FileXfer_Pump(). No generation happens here (see
 * s_needArm). */
static uint16_t FileXfer_FillCallback(const uint8_t **pptr, uint16_t maxlen)
{
    (void)maxlen;

    if (s_ringHead != s_ringTail)
    {
        uint32_t idx = s_ringTail & RING_MASK;
        *pptr = s_ringData[idx];
        uint16_t len = s_ringLen[idx];
        s_ringTail++;
        return len;
    }
    *pptr = NULL;

    if (s_genPos >= FILEXFER_TOTAL_SIZE)
    {
        return 0; /* end of file: stay NAKed */
    }

    /* Ring underrun: NAK (return 0) and ask the pump to re-arm us. */
    s_needArm = 1;
    return 0;
}

void FileXfer_Init(void)
{
    uint32_t i;

    BuildBmpHeader(s_header);

    for (i = 0; i < FILEXFER_IMG_WIDTH * 3u; i++)
    {
        uint8_t c = (((((i / 3u) / SQ) & 1u) != 0u)) ? 0xFFu : 0x00u;
        s_rowA[i] = c;
        s_rowB[i] = (uint8_t)~c;
    }

    USBD_EP2_SetFillCallback(FileXfer_FillCallback);
}

void FileXfer_Start(void)
{
    /* Called from EP0 SETUP (USB ISR) context: only record the request;
     * FileXfer_Pump() in the main loop does the reset + prefill + arm. */
    s_startReq = 1;
}
