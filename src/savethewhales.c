/*

;;;;;;;;;;;;;;;;;;;;;
;; SAVETHEWHALES.C ;;
;;   mpower 2026   ;;
;;;;;;;;;;;;;;;;;;;;;

build with MSVC

cl /nologo /O2 /W4 savethewhales.c

usage:

savethewhales
savethewhales path\to\dsk_folder

*/

/* ===================
** hide deprc warnings
** ===================
*/
#define _CRT_SECURE_NO_WARNINGS

/* ========
** includes
** ========
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#define  PATH_SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#define  PATH_SEP '/'
#endif

/* =============
** disk geometry
** =============
*/
#define DSK_SIZE             143360u
#define TRACKS               35u
#define SECTORS_PER_TRACK    16u
#define SECTOR_SIZE          256u

/* =================
** newsroom clip art
** =================
*/
#define CLIP_TRACK           34u
#define CLIP_TRACK_OFFSET    (CLIP_TRACK * SECTORS_PER_TRACK * SECTOR_SIZE)
#define CLIP_TRACK_SIZE      (SECTORS_PER_TRACK * SECTOR_SIZE)
#define NAME_COUNT_OFFSET    0x1bu
#define NAME_START_OFFSET    0x1cu
#define POINTER_START_OFFSET (6u * SECTOR_SIZE)

#define PAGE_X_ORIGIN        7
#define PAGE_WIDTH           245
#define PAGE_HEIGHT          192

#define MAX_PAGES            128
#define MAX_PAGE_NAME        64
#define MAX_PATH_LEN         1024

/* ===============
** DOS 3.3 catalog
** ===============
*/
#define DOS_VTOC_TRACK       17u
#define DOS_VTOC_SECTOR      0u
#define DOS_CATALOG_ENTRY    0x0bu
#define DOS_CATALOG_STEP     35u
#define DOS_CATALOG_SLOTS    7u
#define DOS_TS_START         0x0cu

/* ===================
** Print Shop graphics
** ===================
*/
#define PS_WIDTH             88u
#define PS_HEIGHT            52u
#define PS_ROW_BYTES         11u
#define PS_DATA_SIZE         (PS_ROW_BYTES * PS_HEIGHT)

/* ===================
** check dsk extension
** ===================
*/
static int hasDskExtension (
    const char *name)
{
    size_t n = strlen (name);
    const char *p;

    if (n < 4)
        return 0;

    /* locate final four-character extension
    */
    p = name + n - 4;
    return p[0] == '.' &&
           tolower ((unsigned char)p[1]) == 'd' &&
           tolower ((unsigned char)p[2]) == 's' &&
           tolower ((unsigned char)p[3]) == 'k';
}

/* =======================
** create output directory
** =======================
*/
static int makeDirectory (
    const char *path)
{
#if defined(_WIN32)
    /* create native Windows directory
    */
    if (
        CreateDirectoryA (
            path,
            NULL))
        return 1;
    return GetLastError () == ERROR_ALREADY_EXISTS;
#else
    /* create POSIX directory
    */
    if (mkdir (path, 0777) == 0)
        return 1;
    return errno == EEXIST;
#endif
}

/* ====================
** join filesystem path
** ====================
*/
static int joinPath (
    char       *out,
    size_t     outSize,
    const char *left,
    const char *right)
{
    size_t n;
    int rc;

    /* add separator when needed
    */
    n = strlen (left);
    if (
        n != 0 &&
        (left[n - 1] == '/' ||
        left[n  - 1] == '\\'))

        rc = snprintf (
            out,
            outSize,
            "%s%s",
            left,
            right
        );
    else
        rc = snprintf (
            out,
            outSize,
            "%s%c%s",
            left,
            PATH_SEP,
            right
        );

    return rc >= 0 && (size_t)rc < outSize;
}

/* ====================
** sanitize output name
** ====================
*/
static void sanitizeName (
    char       *dst,
    size_t     dstSize,
    const char *src)
{
    size_t i = 0;

    /* replace characters illegal in Windows filenames
    */
    while (*src && i + 1 < dstSize) {
        unsigned char c = (unsigned char)*src++;

        if (c <  32  ||
           c == '<'  ||
           c == '>'  ||
           c == ':'  ||
           c == '"'  ||
           c == '/'  ||
           c == '\\' ||
           c == '|'  ||
           c == '?'  ||
           c == '*')

           c = '_';

        dst[i++] = (char)c;
    }

    /* trim trailing spaces and periods
    */
    while (
        i != 0 &&
        (dst[i - 1] == ' ' ||
        dst[i - 1] == '.'))

        --i;

    if (i == 0 && dstSize > 1)
        dst[i++] = '_';

    dst[i] = '\0';
}

/* =====================
** make disk folder name
** =====================
*/
static void diskStem (
    char       *dst,
    size_t     dstSize,
    const char *filename)
{
    char temp[MAX_PATH_LEN];
    size_t n;

    /* copy name and rmv .dsk extension
    */
    snprintf (
        temp,
        sizeof (temp),
        "%s",
        filename
    );

    n = strlen (
        temp
    );

    if (n >= 4 &&
        hasDskExtension (temp))

        temp[n - 4] = '\0';

    sanitizeName (
        dst,
        dstSize,
        temp
    );
}

/* ====================
** read disk image file
** ====================
*/
static int readFile (
    const char *path,
    uint8_t    **dataOut,
    size_t     *sizeOut)
{
    FILE    *fp;
    long    length;
    uint8_t *data;

    *dataOut = NULL;
    *sizeOut = 0;

    /* open source file
    */
    fp = fopen (
        path,
        "rb"
    );

    if (!fp)
        return 0;

    if (
        fseek (
            fp,
            0,
            SEEK_END)
            != 0) {

            fclose (
               fp
            );

        return 0;
    }

    /* determine complete file size
    */
    length = ftell (
        fp
    );

    if (
        length < 0 ||
        fseek (
            fp,
            0,
            SEEK_SET)
            != 0) {
                fclose (
                    fp
                );
        return 0;
    }

    /* allocate and read entire image
    */
    data = (uint8_t *)malloc ((size_t)length);
    if (!data) {
        fclose (fp);
        return 0;
    }

    if (
        fread (
            data, 1, 
            (size_t)length, fp) != 
            (size_t)length) {

            free (data);
            fclose (fp);
            return 0;
    }

    fclose (fp);
    *dataOut = data;
    *sizeOut = (size_t)length;
    return 1;
}

/* =======================
** recognize newsroom disk
** =======================
*/
static int isClipDisk (
    const uint8_t *disk,
    size_t diskSize)
{
    static const char sig[] = "SSI CLIP";
    const uint8_t *meta;
    size_t i;

    if (diskSize != DSK_SIZE)
        return 0;

    /* compare high-bit Apple II signature text
    */
    meta = disk + CLIP_TRACK_OFFSET;

    for (
        i = 0;
        i < 8;
        ++i) {

        if (
            (meta[i] & 0x7f) != (uint8_t)sig[i])
            return 0;
    }

    return 1;
}

/* ========================
** read newsroom page names
** ========================
*/
static int readPageNames (
    const uint8_t *disk,
    char names[MAX_PAGES][MAX_PAGE_NAME],
    int *countOut)
{
    const uint8_t *meta = disk + CLIP_TRACK_OFFSET;
    unsigned int  count = meta[NAME_COUNT_OFFSET];
    size_t pos =  NAME_START_OFFSET;
    unsigned int  page;

    if (
        count == 0 ||
        count >  MAX_PAGES)
        return 0;

    /* read null-terminated high-bit page names
    */
    for (
        page = 0;
        page < count;
        ++page) {

        size_t out   = 0;
        int foundEnd = 0;

        while (pos < POINTER_START_OFFSET) {
            uint8_t c = meta[pos++];

            if (c == 0) {
                foundEnd = 1;
                break;
            }

            c &= 0x7f;
            if (out + 1 < MAX_PAGE_NAME)
                names[page][out++] = (char)c;
        }

        if (!foundEnd)
            return 0;

        names[page][out] = '\0';
    }

    *countOut = (int)count;
    return 1;
}

/* ======================
** calculate shape offset
** ======================
*/
static size_t shapeOffset (
    unsigned int track,
    unsigned int sector,
    unsigned int byteOffset)
{
    /* convert track / sector / byte into disk-image offset
    */
    return ((size_t)track *
        SECTORS_PER_TRACK + sector) *
        SECTOR_SIZE + byteOffset;
}

/* ====================
** forward declarations
** ====================
*/
static void putU16 (
    FILE     *fp,
    uint16_t v
);
static void putU32 (
    FILE     *fp,
    uint32_t v
);

/* =====================
** check filename prefix
** =====================
*/
static int startsWith (
    const char *text,
    const char *prefix)
{
    /* compare only prefix length
    */
    return strncmp (
        text,
        prefix,
        strlen (prefix)
    ) == 0;
}

/* ==================
** locate disk sector
** ==================
*/
static const uint8_t *diskSector (
    const uint8_t *disk,
    unsigned int  track,
    unsigned int  sector)
{
    if (
        track >= TRACKS ||
        sector >= SECTORS_PER_TRACK)
        return NULL;

    /* return first byte of requested sector
    */
    return disk + (
        (size_t)track *
        SECTORS_PER_TRACK + sector
    ) * SECTOR_SIZE;
}

/* ===================
** write packed bitmap
** ===================
*/
static int writePackedBmp (
    const char    *path,
    const uint8_t *pixels,
    unsigned int  width,
    unsigned int  height,
    unsigned int  sourceRowBytes)
{
    FILE         *fp;
    unsigned int bmpRowBytes;
    unsigned int rowStride;
    uint32_t     pixelBytes;
    uint32_t     pixelOffset;
    uint32_t     fileSize;
    unsigned int y;

    if (!pixels     ||
        width  == 0 ||
        height == 0)
        return 0;

    /* calculate BMP row layout
    */
    bmpRowBytes = (width + 7u) / 8u;

    if (sourceRowBytes < bmpRowBytes)
        return 0;

    rowStride   = (bmpRowBytes + 3u)  & ~3u;
    pixelBytes  = (uint32_t)rowStride * height;
    pixelOffset = 14u + 40u + 8u;
    fileSize    = pixelOffset + pixelBytes;

    /* create destination bitmap
    */
    fp = fopen (path, "wb");
    if (!fp)
        return 0;

    /* BITMAPFILEHEADER
    */
    fputc ('B', fp);
    fputc ('M', fp);
    putU32 (fp, fileSize);
    putU16 (fp, 0);
    putU16 (fp, 0);
    putU32 (fp, pixelOffset);

    /* BITMAPINFOHEADER
    */
    putU32 (fp, 40);
    putU32 (fp, width);
    putU32 (fp, height);
    putU16 (fp, 1);
    putU16 (fp, 1);
    putU32 (fp, 0);
    putU32 (fp, pixelBytes);
    putU32 (fp, 0);
    putU32 (fp, 0);
    putU32 (fp, 2);
    putU32 (fp, 2);

    /* Palette: 0 = white, 1 = black.
    */
    fputc (255, fp); fputc (255, fp); fputc (255, fp); fputc (0, fp);
    fputc (0, fp);   fputc (0,   fp); fputc (0,   fp); fputc (0, fp);

    /* write rows bottom-up for BMP storage
    */
    for (y = height; y != 0; --y) {

        const uint8_t *row = pixels +
            (size_t)(y - 1) * sourceRowBytes;

        unsigned int xbyte;

        for (
            xbyte = 0;
            xbyte < bmpRowBytes;
            ++xbyte) {

            uint8_t v = row[xbyte];

            if (
                xbyte + 1 == bmpRowBytes &&
                (width & 7u) != 0) {

                unsigned int used = width & 7u;
                v &= (uint8_t)(0xffu << (8u - used));
            }

            fputc (v, fp);
        }

        for (
            xbyte = bmpRowBytes;
            xbyte < rowStride;
            ++xbyte)

            fputc (
                0,
                fp
            );
    }

    if (ferror (fp)) {
        fclose (fp);
        return 0;
    }

    fclose (fp);
    return 1;
}

/* ====================
** read DOS binary file
** ====================
*/
static int readDosBinary (
    const uint8_t *disk,
    unsigned int  tsTrack,
    unsigned int  tsSector,
    uint8_t       **payloadOut,
    size_t        *payloadSizeOut)
{
    uint8_t      *raw;
    size_t       rawSize = 0;
    unsigned int track = tsTrack;
    unsigned int sector = tsSector;
    unsigned int guard = 0;
    uint16_t     dataLength;

    *payloadOut     = NULL;
    *payloadSizeOut = 0;

    /* reserve enough space for one full disk image
    */
    raw = (uint8_t *)malloc (DSK_SIZE);
    if (!raw)
        return 0;

    /* follow DOS 3.3 track / sector list chain
    */
    while (
        track != 0 &&
        guard++ < TRACKS *
        SECTORS_PER_TRACK) {

        const uint8_t *ts = diskSector (
            disk,
            track,
            sector
        );

        unsigned int nextTrack, nextSector;
        unsigned int pos;

        if (!ts) {
            free (raw);
            return 0;
        }

        nextTrack = ts[1];
        nextSector = ts[2];

        /* append every referenced data sector
        */
        for (
            pos = DOS_TS_START;
            pos + 1 < SECTOR_SIZE;
            pos += 2) {

            unsigned int  dataTrack  = ts[pos];
            unsigned int  dataSector = ts[pos + 1];
            const uint8_t *data;

            if (dataTrack == 0)
                continue;

            data = diskSector (
                disk,
                dataTrack,
                dataSector
            );

            if (
                !data ||
                rawSize + 
                SECTOR_SIZE >
                DSK_SIZE) {

                free (raw);
                return 0;
            }

            memcpy (
                raw + rawSize,
                data,
                SECTOR_SIZE
            );

            rawSize += SECTOR_SIZE;
        }

        track = nextTrack;
        sector = nextSector;
    }

    if (rawSize < 4) {
        free (raw);
        return 0;
    }

    /* strip DOS binary load address and length header
    */
    dataLength = 
        (uint16_t)(raw[2] |
        ((uint16_t)raw[3] << 8)
    );

    if ((size_t)dataLength + 4 > rawSize) {
        free (raw);
        return 0;
    }

    memmove (
        raw,
        raw + 4,
        dataLength
    );

    *payloadOut     = raw;
    *payloadSizeOut = dataLength;
    return 1;
}

/* =======================
** write art gallery image
** =======================
*/
static int writeArtGalleryGraphic (
    const char    *path,
    const char    *name,
    const uint8_t *payload,
    size_t        payloadSize)
{
    /* standard Print Shop 88 x 52 graphic
    */
    if (
        payloadSize ==
        PS_DATA_SIZE) {

        return writePackedBmp (
            path,
            payload,
            PS_WIDTH,
            PS_HEIGHT,
            PS_ROW_BYTES);
    }

    /* larger variable-size gallery graphic
    */
    if (
        name[0] == 'L' &&
        payloadSize >= 4) {

        unsigned int rowBytes = payload[0];
        unsigned int height   = payload[1];

        unsigned int width    = (
            unsigned int)payload[2] |
            ((unsigned int)payload[3] << 8
        );

        unsigned int packedWidth = rowBytes * 8u;

        ++width;
        if (width > packedWidth)
            width = packedWidth;

        if (
            rowBytes == 0 ||
            height   == 0 ||
            width    == 0 ||
            4u + (size_t)rowBytes * height > payloadSize)
            return 0;

        return writePackedBmp (
            path,
            payload + 4,
            width,
            height,
            rowBytes
        );
    }

    return 0;
}

/* ========================
** extract art gallery disk
** ========================
*/
static int extractArtGalleryDisk (
    const char *diskPath,
    const char *diskFilename,
    const char *outputRoot)
{
    uint8_t       *disk    = NULL;
    size_t        diskSize = 0;
    const uint8_t *vtoc;
    unsigned int  catalogTrack, catalogSector;
    unsigned int  guard = 0;
    char          stem[MAX_PATH_LEN];
    char          diskOutput[MAX_PATH_LEN];
    int           written = 0;

    /* disk 1A contains index / converter data only
    */
    if (
        strcmp (
            diskFilename,
            "American History Art Gallery disk 1A.dsk"
        ) == 0) {

        printf (
            "[index] %s: converter/index disk; no gallery bitmaps\n",
            diskFilename
        );
        return 0;
    }

    if (
        !readFile (
            diskPath,
            &disk,
            &diskSize) ||
            diskSize != DSK_SIZE) {

        printf (
            "[error] %s: cannot read DOS 3.3 disk\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    /* locate DOS catalog through VTOC
    */
    vtoc = diskSector (
        disk,
        DOS_VTOC_TRACK,
        DOS_VTOC_SECTOR
    );

    if (
        !vtoc ||
        vtoc[1] >= TRACKS ||
        vtoc[2] >= SECTORS_PER_TRACK) {

        printf (
            "[error] %s: bad DOS 3.3 VTOC\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    diskStem (
        stem,
        sizeof (stem),
        diskFilename
    );

    if (
        !joinPath (
            diskOutput,
            sizeof (diskOutput),
            outputRoot, stem) ||
            !makeDirectory (diskOutput)) {

        printf (
            "[error] %s: cannot create output folder\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    catalogTrack  = vtoc[1];
    catalogSector = vtoc[2];

    printf (
        "[hist ] %s\n",
        diskFilename
    );

    /* walk DOS catalog sectors
    */
    while (
        catalogTrack != 0 &&
        guard++ < 32) {

        const uint8_t *catalog = diskSector (
            disk,
            catalogTrack,
            catalogSector
        );

        unsigned int slot;

        if (!catalog)
            break;

        /* inspect each file entry in this catalog sector
        */
        for (
            slot = 0;
            slot < DOS_CATALOG_SLOTS;
            ++slot) {

            size_t pos =  DOS_CATALOG_ENTRY + slot * DOS_CATALOG_STEP;
            const uint8_t *entry = catalog + pos;
            unsigned int  tsTrack = entry[0];
            unsigned int  tsSector = entry[1];
            char          name[31];
            char          safeName[64];
            char          bmpName[72];
            char          bmpPath[MAX_PATH_LEN];
            uint8_t       *payload = NULL;
            size_t        payloadSize = 0;
            unsigned int  i;

            if (
                tsTrack  == 0      ||
                tsTrack  == 0xff   ||
                tsTrack  >= TRACKS ||
                tsSector >= SECTORS_PER_TRACK)
                continue;

            for (
                i = 0;
                i < 30;
                 ++i)

                name[i] = (char)(entry[3 + i] & 0x7f
             );

            name[30] = '\0';

            for (
                i  = 30;
                i != 0 &&
                name[i - 1] == ' '; --i)

                name[i - 1] = '\0';

            /* load graphic file through its T/S list
            */
            if (
            !readDosBinary (
                disk,
                tsTrack,
                tsSector,
                &payload,
                &payloadSize))
                continue;

            sanitizeName (
                safeName,
                sizeof (safeName),
                name
            );

            snprintf (
                bmpName,
                sizeof (bmpName),
                "%s.bmp",
                safeName
            );

            if (
                joinPath (bmpPath, sizeof (bmpPath),
                diskOutput, bmpName) &&
                writeArtGalleryGraphic (
                    bmpPath,
                    name,
                    payload,
                    payloadSize)) {
                ++written;
            } else {
                printf (
                    "        %-24s ERROR\n",
                    name
                );
            }

            free (payload);
        }

        catalogTrack  = catalog[1];
        catalogSector = catalog[2];
    }

    printf (
        "        wrote %d BMPs\n",
        written
    );

    free (disk);
    return written;
}

/* ========================
** handle graphics expander
** ========================
*/
static int handleGraphicsExpander (
    const char *diskFilename)
{
    /* explicitly recognize supplied program side
    */
    if (
        strcmp (
            diskFilename,
            "NEWSROOM_GRAPHICS_EXP_S1.dsk") == 0) {

        printf (
            "[ge   ] %s: program/demo side; no unique gallery dump\n",
               diskFilename
        );
        return 1;
    }

    /* explicitly recognize supplied protected data side
    */
    if (
        strcmp (
            diskFilename,
            "NEWSROOM_GRAPHICS_EXP_S2.dsk") == 0) {

        printf (
            "[ge   ] %s: protected duplicate Clip Art collection; "
            "already covered by SSI CLIP disks\n",
            diskFilename
        );
        return 1;
    }

    return 0;
}

/* =====================
** render newsroom shape
** =====================
*/
static int renderShape (
    const uint8_t *disk,
    size_t        diskSize,
    unsigned int  track,
    unsigned int  sector,
    unsigned int  byteOffset,
    uint8_t       page[PAGE_HEIGHT][PAGE_WIDTH])
{
    size_t        pos;
    unsigned int  y1, y2, x1, x2;
    unsigned int  width, height, byteColumns;
    size_t        decodedSize;
    uint8_t       *decoded;
    size_t        out = 0;
    unsigned int  bx, yy, bit;

    if (
        track >= TRACKS ||
        sector >= SECTORS_PER_TRACK ||
        byteOffset >= SECTOR_SIZE)
        return 0;

    /* locate shape record and read bounding box
    */
    pos = shapeOffset (
        track,
        sector,
        byteOffset
    );

    if (pos + 4 > diskSize)
        return 0;

    y1 = disk[pos + 0];
    y2 = disk[pos + 1];
    x1 = disk[pos + 2];
    x2 = disk[pos + 3];
    pos += 4;

    if (
        y2 < y1 ||
        x2 < x1)
        return 0;

    width       = x2 - x1 + 1;
    height      = y2 - y1 + 1;
    byteColumns = (width + 6) / 7;

    if (
        height == 0 ||
        height > PAGE_HEIGHT ||
        byteColumns == 0 ||
        byteColumns > 40)
        return 0;

    /* allocate exact uncompressed shape buffer
    */
    decodedSize = (size_t)byteColumns * height;
    decoded     = (uint8_t *)malloc (decodedSize);

    if (!decoded)
        return 0;

    /* unpack literal bytes and repeat runs
    */
    while (out < decodedSize) {
        uint8_t value;

        if (pos >= diskSize) {
            free (decoded);
            return 0;
        }

        value = disk[pos++];

        if (value != 0) {
            decoded[out++] = value;
        } else {
            unsigned int run;
            uint8_t repeated;

            if (pos + 2 > diskSize) {
                free (decoded);
                return 0;
            }

            run      = disk[pos++];
            repeated = disk[pos++];

            while (
                run != 0 &&
                out < decodedSize) {

                decoded[out++] = repeated;
                --run;
            }
        }
    }

    /* paint column-major seven-pixel bytes into page
    */
    for (
        bx = 0;
        bx < byteColumns;
        ++bx) {

        for (
            yy = 0;
            yy < height;
            ++yy) {

            uint8_t pixels = decoded[(size_t)bx * 
                height + yy] & 0x7f;

            for (
                bit = 0;
                bit < 7;
                ++bit) {

                unsigned int localX = bx * 7 + bit;
                int          pageX;
                unsigned int pageY;

                if (localX >= width)
                    break;

                if ((pixels & (1u << bit)) == 0)
                    continue;

                pageX = (int)x1 + (int)localX - PAGE_X_ORIGIN;
                pageY = y1 + yy;

                if (pageX >= 0 && pageX < PAGE_WIDTH &&
                    pageY < PAGE_HEIGHT)
                    page[pageY][pageX] = 1;
            }
        }
    }

    free (decoded);
    return 1;
}

/* =======================
** write little endian u16
** =======================
*/
static void putU16 (
    FILE     *fp,
    uint16_t v)
{
    /* BMP fields are little endian
    */
    fputc (
        (int)(v & 0xff),
        fp
    );
    fputc (
        (int)((v >> 8) & 0xff),
        fp
    );
}

/* =======================
** write little endian u32
** =======================
*/
static void putU32 (
    FILE *fp,
    uint32_t v)
{
    /* BMP fields are little endian
    */
    fputc ((int)(v  &  0xff), fp);
    fputc ((int)((v >> 8)  & 0xff), fp);
    fputc ((int)((v >> 16) & 0xff), fp);
    fputc ((int)((v >> 24) & 0xff), fp);
}

/* =====================
** write newsroom bitmap
** =====================
*/
static int writeBmp (
    const char *path,
    uint8_t page[PAGE_HEIGHT][PAGE_WIDTH])
{
    FILE *fp;
    const unsigned int rowBytes    = (PAGE_WIDTH + 7) / 8;
    const unsigned int rowStride   = (rowBytes + 3) & ~3u;
    const uint32_t     pixelBytes  = rowStride * PAGE_HEIGHT;
    const uint32_t     pixelOffset = 14 + 40 + 8;
    const uint32_t     fileSize    = pixelOffset + pixelBytes;
    unsigned int       y;

    /* create complete 245 x 192 page bitmap
    */
    fp = fopen (path, "wb");
    if (!fp)
        return 0;

    /* BITMAPFILEHEADER
    */
    fputc ('B', fp);
    fputc ('M', fp);
    putU32 (fp, fileSize);
    putU16 (fp, 0);
    putU16 (fp, 0);
    putU32 (fp, pixelOffset);

    /* BITMAPINFOHEADER
    */
    putU32 (fp, 40);
    putU32 (fp, PAGE_WIDTH);
    putU32 (fp, PAGE_HEIGHT);
    putU16 (fp, 1);
    putU16 (fp, 1);
    putU32 (fp, 0);
    putU32 (fp, pixelBytes);
    putU32 (fp, 0);
    putU32 (fp, 0);
    putU32 (fp, 2);
    putU32 (fp, 2);

    /* Palette: 0 = white, 1 = black.
    */
    fputc (255, fp); fputc (255, fp); fputc (255, fp); fputc (0, fp);
    fputc (0,   fp); fputc (0,   fp); fputc (0,   fp); fputc (0, fp);

    /* BMP rows are bottom-up.
    */
    for (
        y = PAGE_HEIGHT;
        y != 0;
         --y) {

        unsigned int x;
        unsigned int written = 0;
        uint8_t      byte    = 0;
        unsigned int bits    = 0;

        for (
            x = 0;
            x < PAGE_WIDTH;
            ++x) {

            byte <<= 1;

            if (page[y - 1][x])
                byte |= 1;

            ++bits;

            if (bits == 8) {
                fputc (
                    byte,
                    fp
                );
                ++written;
                byte = 0;
                bits = 0;
            }
        }

        if (bits != 0) {
            byte <<= (8 - bits);
            fputc (byte, fp);
            ++written;
        }

        while (written < rowStride) {
            fputc (0, fp);
            ++written;
        }
    }

    if (ferror (fp)) {
        fclose (fp);
        return 0;
    }

    fclose (fp);
    return 1;
}

/* =====================
** extract newsroom disk
** =====================
*/
static int extractNewsroomDisk (
    const char *diskPath,
    const char *diskFilename,
    const char *outputRoot)
{
    uint8_t *disk = NULL;
    size_t  diskSize = 0;
    char    names[MAX_PAGES][MAX_PAGE_NAME];
    int     pageCount;
    const   uint8_t *meta;
    size_t  ptrPos;
    char    stem[MAX_PATH_LEN];
    char    diskOutput[MAX_PATH_LEN];
    int     pageIndex;
    int     written = 0;

    /* load candidate disk image
    */
    if (
        !readFile (
            diskPath,
            &disk,
            &diskSize)) {

        printf (
            "[error] %s: cannot read\n",
            diskFilename
        );
        return 0;
    }

    /* accept only SSI CLIP disk layout
    */
    if (
        !isClipDisk (
            disk,
            diskSize)) {

        printf (
            "[skip ] %s\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    /* read clip-art page directory
    */
    if (
        !readPageNames (
            disk,
            names,
            &pageCount)) {

        printf (
            "[error] %s: bad Newsroom directory\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    diskStem (
        stem,
        sizeof (stem),
        diskFilename
    );

    if (
        !joinPath (
            diskOutput,
            sizeof (diskOutput),
            outputRoot,
            stem) ||
        !makeDirectory (diskOutput)) {

        printf (
            "[error] %s: cannot create output folder\n",
            diskFilename
        );
        free (disk);
        return 0;
    }

    meta   = disk + CLIP_TRACK_OFFSET;
    ptrPos = POINTER_START_OFFSET;

    printf (
        "[clip ] %s: %d pages\n",
        diskFilename,
        pageCount
    );

    /* rebuild one complete Newsroom page at a time
    */
    for (
        pageIndex = 0;
        pageIndex < pageCount;
        ++pageIndex) {

        uint8_t page[PAGE_HEIGHT][PAGE_WIDTH];
        int     shapeCount = 0;
        int     pageOk = 1;
        char    safePage[MAX_PAGE_NAME];
        char    bmpName[MAX_PAGE_NAME + 8];
        char    bmpPath[MAX_PATH_LEN];

        memset (page, 0, sizeof (page));

        /* render every shape belonging to current page
        */
        while (ptrPos < CLIP_TRACK_SIZE &&
            meta[ptrPos] != 0xff) {

            unsigned int track, sector, offset;

            if (ptrPos + 3 > CLIP_TRACK_SIZE) {
                pageOk = 0;
                break;
            }

            track  =  meta[ptrPos  + 0];
            sector =  meta[ptrPos + 1];
            offset =  meta[ptrPos + 2];
            ptrPos += 3;

            if (
                !renderShape (
                    disk,
                    diskSize,
                    track,
                    sector,
                    offset,
                    page)) {
                pageOk = 0;
                break;
            }
            ++shapeCount;
        }

        if (ptrPos >= CLIP_TRACK_SIZE) {
            pageOk = 0;
        } else {
            ++ptrPos;  /* skip FF page separator */
        }

        if (
            !pageOk ||
            shapeCount == 0) {

            printf (
                "        %-16s ERROR\n",
                names[pageIndex]
            );
            continue;
        }

        sanitizeName (
            safePage,
            sizeof (safePage),
            names[pageIndex]
        );

        snprintf (
            bmpName,
            sizeof (bmpName),
            "%s.bmp",
            safePage
        );

        if (
            !joinPath (
                bmpPath,
                sizeof (bmpPath),
                diskOutput,
                bmpName) ||
                !writeBmp (
                    bmpPath,
                    page)) {

            printf (
                "        %-16s WRITE ERROR\n",
                names[pageIndex]
            );
            continue;
        }

        ++written;
    }

    printf (
        "        wrote %d BMPs\n",
        written
    );
    free (disk);
    return written;
}

/* =======================
** dispatch disk extractor
** =======================
*/
static int extractAnyDisk (
    const char *diskPath,
    const char *diskFilename,
    const char *outputRoot)
{
    /* dispatch known single-use disk families
    */
    if (
        startsWith (
            diskFilename,
            "American History Art Gallery disk "))

        return extractArtGalleryDisk (
            diskPath,
            diskFilename,
            outputRoot
        );

    if (handleGraphicsExpander (diskFilename))
        return 0;

    return extractNewsroomDisk (
        diskPath,
        diskFilename,
        outputRoot
    );
}

/* ======================
** process disk directory
** ======================
*/
static int processDirectory (const char *inputDir)
{
    char outputRoot[MAX_PATH_LEN];
    int  total = 0;
    int  seen  = 0;

    /* create common extraction root
    */
    if (
        !joinPath (
            outputRoot,
            sizeof (outputRoot),
            inputDir,
            "extracted") ||
        !makeDirectory (outputRoot)) {

        fprintf (
            stderr,
            "Cannot create extracted folder.\n"
        );
        return 1;
    }

#if defined(_WIN32)
    {
        WIN32_FIND_DATAA fd;
        HANDLE           find;
        char             pattern[MAX_PATH_LEN];

        if (
            !joinPath (
                pattern,
                sizeof (pattern),
                inputDir,
                "*.dsk")) {

            fprintf (
                stderr,
                "Input path is too long.\n"
            );
            return 1;
        }

        /* enumerate Windows .dsk files
        */
        find = FindFirstFileA (
            pattern,
            &fd
        );

        if (find == INVALID_HANDLE_VALUE) {
            printf (
                "No .dsk files found in %s\n",
                inputDir
        );
            return 0;
        }

        do {
            char diskPath[MAX_PATH_LEN];

            if (
                fd.dwFileAttributes & 
                FILE_ATTRIBUTE_DIRECTORY)
                continue;

            if (
                !hasDskExtension (fd.cFileName))
                continue;
            if (
                !joinPath (
                    diskPath,
                    sizeof (diskPath),
                    inputDir,
                    fd.cFileName))
                continue;

            ++seen;

            total += extractAnyDisk (
                diskPath,
                fd.cFileName,
                outputRoot
            );

        } while (FindNextFileA (find, &fd));

        FindClose (find);
    }
#else
    {
        DIR *dir;
        struct dirent *entry;

        /* enumerate POSIX .dsk files
        */
        dir = opendir (inputDir);
        if (!dir) {

            fprintf (
                stderr,
                "Cannot open directory: %s\n",
                inputDir
            );
            return 1;
        }

        while ((entry = readdir (dir)) != NULL) {
            char diskPath[MAX_PATH_LEN];

            if (!hasDskExtension (entry->d_name))
                continue;
            if (!joinPath (
                diskPath,
                sizeof (diskPath),
                inputDir,
                entry->d_name
            ))
                continue;

            ++seen;

            total += extractAnyDisk (
                diskPath,
                entry->d_name,
                outputRoot
            );
        }

        closedir (dir);
    }
#endif

    if (!seen)
        printf (
            "No .dsk files found in %s\n",
            inputDir
        );
    else
        printf (
            "\nDone: %d BMPs in %s\n",
            total,
            outputRoot
        );

    return 0;
}

/* =============
** program entry
** =============
*/
int main (
    int  argc,
    char **argv)
{
    const char *inputDir = ".";

    /* validate command line
    */
    if (argc > 2) {

        fprintf (
            stderr,
            "Usage: savethewhales [dsk-folder]\n"
        );
        return 1;
    }

    /* optional input folder
    */
    if (argc == 2)
        inputDir = argv[1];

    return processDirectory (inputDir);
}
