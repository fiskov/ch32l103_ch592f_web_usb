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
#include "ch592_usbd_device.h"
#include <string.h>

/* Ring buffer of pre-built packets. 32 slots * 64 bytes = 2KB, a small
 * fraction of the CH592F's RAM. Must be a power of two for cheap masking
 * instead of modulo. */
#define RING_SLOTS   32u
#define RING_MASK    (RING_SLOTS - 1u)

static uint8_t  s_ringData[RING_SLOTS][DEF_USBD_UEP2_SIZE];
static uint8_t  s_ringLen[RING_SLOTS];
static volatile uint32_t s_ringHead = 0; /* producer: slots produced so far   */
static volatile uint32_t s_ringTail = 0; /* consumer: slots consumed so far   */

/* Pixel-walk generation position, shared between the producer
 * (FileXfer_Pump, main-loop context) and the rare ISR-context fallback
 * in FileXfer_FillCallback - safe without locks because it's a strict
 * single-producer/single-consumer sequence: the fallback path only ever
 * runs when the ring is empty (head==tail), i.e. exactly when the
 * "next byte to generate" position is unambiguous. */
static uint32_t s_genPos;
static uint32_t s_col;         /* 0 .. FILEXFER_IMG_WIDTH-1 */
static uint32_t s_channel;     /* 0=Blue, 1=Green, 2=Red    */
static uint32_t s_actualRow;   /* counts down from HEIGHT-1 */

static uint8_t s_blueByCol[FILEXFER_IMG_WIDTH];
static uint8_t s_greenByRow[FILEXFER_IMG_HEIGHT];
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
    uint16_t i;
    uint32_t offset = s_genPos;

    for (i = 0; i < n; i++)
    {
        if (offset < FILEXFER_BMP_HEADER_LEN)
        {
            buf[i] = s_header[offset];
        }
        else
        {
            switch (s_channel)
            {
                case 0: buf[i] = s_blueByCol[s_col]; break;
                case 1: buf[i] = s_greenByRow[s_actualRow]; break;
                default: buf[i] = (uint8_t)((s_col + s_actualRow) & 0xFFu); break;
            }
            AdvancePixelState();
        }
        offset++;
    }
    s_genPos = offset;
    return n;
}

/* Called from the idle main loop: fills the ring buffer with as many
 * freshly-generated packets as there is room for. Cheap to call often -
 * it's a no-op once the ring is full or the file is exhausted. */
void FileXfer_Pump(void)
{
    while ((s_ringHead - s_ringTail) < RING_SLOTS)
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
}

/* USB-interrupt-context callback: pops the next ready packet from the
 * ring (fast memcpy, no generation work) or, on a rare ring underrun,
 * falls back to generating it synchronously right here. */
static uint16_t FileXfer_FillCallback(uint8_t *buf, uint16_t maxlen)
{
    if (s_ringHead != s_ringTail)
    {
        uint32_t idx = s_ringTail & RING_MASK;
        uint16_t len = s_ringLen[idx];
        memcpy(buf, s_ringData[idx], len);
        s_ringTail++;
        return len;
    }

    if (s_genPos >= FILEXFER_TOTAL_SIZE)
    {
        return 0; /* end of file */
    }

    /* Ring underrun (e.g. right after FileXfer_Start()): generate this
     * one packet directly so correctness never depends on pump timing. */
    return GenerateChunk(buf, maxlen);
}

void FileXfer_Init(void)
{
    uint32_t i;

    BuildBmpHeader(s_header);

    for (i = 0; i < FILEXFER_IMG_WIDTH; i++)
    {
        s_blueByCol[i] = (uint8_t)((i * 255u) / (FILEXFER_IMG_WIDTH - 1u));
    }
    for (i = 0; i < FILEXFER_IMG_HEIGHT; i++)
    {
        s_greenByRow[i] = (uint8_t)((i * 255u) / (FILEXFER_IMG_HEIGHT - 1u));
    }

    USBD_EP2_SetFillCallback(FileXfer_FillCallback);
}

void FileXfer_Start(void)
{
    s_genPos    = 0;
    s_col       = 0;
    s_channel   = 0;
    s_actualRow = FILEXFER_IMG_HEIGHT - 1u;
    s_ringHead  = 0;
    s_ringTail  = 0;

    /* Pre-fill the ring before arming the first transfer, so even the
     * very first packets benefit from the fast ISR path. */
    FileXfer_Pump();

    if (USBD_DevEnumStatus)
    {
        USBD_EP2_StartTransfer();
    }
}
