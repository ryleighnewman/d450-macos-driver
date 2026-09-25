/*
 * Native replacement for the rastertolabeltspl CUPS filter used by the D450 and
 * related TSPL label printers. The vendor's copy is Intel-only, so it stops working
 * once Rosetta is gone, which is the state a macOS 27 upgrade leaves behind.
 *
 * cupsModelNumber 20, 21 and 22 are handled here and produce the same bytes as the
 * original. Any other model is passed to the original filter, which install.sh keeps
 * as rastertolabeltspl.intel.
 */

#include <cups/ppd.h>
#include <cups/raster.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EOL "\r\n"

static const int black_levels[] = {192, 160, 144, 128, 112, 96, 64, 0};
static const int gray_levels[] = {192, 160, 144, 128, 96, 64, 16};

static const unsigned char bayer[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};

static volatile sig_atomic_t canceled;

static void on_term(int sig)
{
    (void)sig;
    canceled = 1;
}

static const char *marked(ppd_file_t *ppd, const char *option)
{
    ppd_choice_t *c = ppdFindMarkedChoice(ppd, option);
    return c ? c->choice : NULL;
}

static int marked_int(ppd_file_t *ppd, const char *option, int fallback)
{
    const char *c = marked(ppd, option);
    return c ? atoi(c) : fallback;
}

static int level(const int *levels, int count, int choice)
{
    if (choice < 1)
        choice = 1;
    if (choice > count)
        choice = count;
    return levels[choice - 1];
}

/* The Adjust options are whole mm plus tenths; the original moves the image the
 * opposite way, hence the negative divisor. */
static int adjust(ppd_file_t *ppd, const char *major, const char *minor, unsigned dpi)
{
    const char *c = marked(ppd, major);
    if (!c)
        return 0;
    double mm = atoi(c) + marked_int(ppd, minor, 0) / 10.0;
    return (int)(dpi * mm / -25.4);
}

/* Everything the printer needs before the bitmap, in the original's order. */
static int start_page(ppd_file_t *ppd, int model, const cups_page_header2_t *h)
{
    const char *size = h->cupsPageSizeName, *c;
    int w_mm = (int)(h->cupsWidth * 25.4 / h->HWResolution[0] + 0.5);
    int h_mm = (int)(h->cupsHeight * 25.4 / h->HWResolution[1] + 0.5);
    int rotate = 0;

    printf("SIZE %d mm,%d mm" EOL, w_mm, h_mm);
    printf("REFERENCE 0,0" EOL);
    if ((c = marked(ppd, "Rotate")) != NULL)
        rotate = strcmp(size, "wfw100h100") ? atoi(c) : 1;
    printf("DIRECTION 0,0" EOL);

    if (model == 21 && (strstr(size, "LW") || strstr(size, "DK"))) {
        static const struct { const char *name, *gap; } stock[] = {
            {"LW1744907", "6.1"}, {"LW30252", "4.6"}, {"LW30336", "7.6"}, {"LW30256", "4.3"},
            {"LW30334", "9.5"},   {"DK1201", "6"},    {"DK1202", "6"},    {"DK1241", "6"}};
        const char *gap = "3";
        for (size_t i = 0; i < sizeof stock / sizeof *stock; i++)
            if (!strcmp(size, stock[i].name))
                gap = stock[i].gap;
        printf("GAP %s mm,0 mm" EOL, gap);
    } else if ((c = marked(ppd, "zeMediaTracking")) != NULL) {
        int gh = marked_int(ppd, "GapHeight", 0), go = marked_int(ppd, "GapOffset", 0);
        if (!strcmp(c, "Gap")) {
            double extra = 0.0;
            const char *rl = marked(ppd, "RoundLabel");
            if (model == 20 && abs(h_mm - w_mm) <= 2 && w_mm >= 18 && rl && !strcmp(rl, "Yes"))
                extra = w_mm / 2 - sqrt((int)(((unsigned)(w_mm * w_mm) >> 2) - 64));
            printf("GAP %d.0 mm,%.1f mm" EOL, gh ? gh : 3, go + extra);
        } else if (!strcmp(c, "Continuous")) {
            printf("GAP 0 mm,0 mm" EOL);
        } else if (!strcmp(c, "BLine")) {
            printf("BLINE %d mm,%d mm" EOL, gh, go);
        }
    }

    if (model != 22) {
        printf("SET TEAR ON" EOL);
    } else if ((c = marked(ppd, "AfterPrint")) != NULL) {
        if (!strcmp(c, "none"))
            printf("SET TEAR OFF" EOL);
        else if (!strcmp(c, "tear"))
            printf("SET TEAR ON" EOL);
        else if (!strcmp(c, "peel"))
            printf("SET PEEL ON" EOL);
        else if (!strcmp(c, "cut"))
            printf("SET CUTTER 1" EOL);
    }
    printf("OFFSET %d mm" EOL, marked_int(ppd, "FeedOffset", 0));

    if ((c = marked(ppd, "Darkness")) != NULL && strcmp(c, "Default"))
        printf("DENSITY %d" EOL, atoi(c));
    if ((c = marked(ppd, "zePrintRate")) != NULL && strcmp(c, "Default"))
        printf("SPEED %d" EOL, atoi(c) == 1 ? 2 : atoi(c));
    if ((c = marked(ppd, "PrintMethod")) != NULL) {
        if (!strcmp(c, "thermal"))
            printf("SET RIBBON OFF" EOL);
        else if (!strcmp(c, "thermalTransfer"))
            printf("SET RIBBON ON" EOL);
    }
    printf("CLS" EOL);

    if (rotate > 1)
        printf("BITMAP 0,0,%u,%u,1,", (h->cupsHeight + 7) >> 3, (h->cupsWidth + 7) & ~7u);
    else
        printf("BITMAP 0,0,%u,%u,1,", (h->cupsWidth + 7) >> 3, h->cupsHeight);
    return rotate;
}

/* Reads one page of 8-bit gray and writes it as 1-bit rows, 0 = black. */
static int print_page(cups_raster_t *ras, ppd_file_t *ppd, const cups_page_header2_t *h,
                      int rotate)
{
    unsigned cw = (h->cupsWidth + 7) & ~7u, ch = h->cupsHeight;
    size_t n = (size_t)cw * ch;
    unsigned char *line = malloc(h->cupsBytesPerLine), *a = malloc(n), *b = malloc(n), *t;
    if (!line || !a || !b)
        return 0;

    memset(a, 0xFF, n);
    for (unsigned y = 0; y < ch && !canceled; y++) {
        if (!cupsRasterReadPixels(ras, line, h->cupsBytesPerLine))
            break;
        memcpy(a + (size_t)y * cw, line, h->cupsWidth);
    }

    if (marked(ppd, "ColorOption") && !strcmp(marked(ppd, "ColorOption"), "GrayScale")) {
        int gl = level(gray_levels, 7, marked_int(ppd, "DegreeOfGrayRecognition", 4));
        for (unsigned y = 0; y < ch; y++)
            for (unsigned x = 0; x < cw; x++) {
                unsigned char *p = a + (size_t)y * cw + x;
                int v = *p >= 246 ? 255 : *p < gl ? 0 : *p;
                *p = v > bayer[y & 7][x & 7] * 4 ? 255 : 0;
            }
    } else {
        int bl = level(black_levels, 8, marked_int(ppd, "DegreeOfBlackRecognition", 2));
        for (size_t i = 0; i < n; i++)
            a[i] = a[i] > bl ? 255 : 0;
    }

    if (!(rotate & 1)) {
        for (size_t i = 0; i < n; i++)
            b[i] = a[n - 1 - i];
        t = a, a = b, b = t;
    }
    if (rotate > 1) {
        for (unsigned r = 0; r < cw; r++)
            for (unsigned i = 0; i < ch; i++)
                b[(size_t)r * ch + i] = a[(size_t)(ch - 1 - i) * cw + r];
        t = a, a = b, b = t;
        unsigned s = cw;
        cw = ch, ch = s;
    }

    int hoff = adjust(ppd, "AdjustHoriaontal", "AdjustMinorHoriaontal", h->HWResolution[0]);
    int voff = adjust(ppd, "AdjustVertical", "AdjustMinorVertical", h->HWResolution[1]);
    memset(b, 0xFF, n);
    for (int y = 0; y < (int)ch; y++) {
        int ty = y + voff;
        if (ty < 0 || ty >= (int)ch)
            continue;
        for (int x = 0; x < (int)cw; x++) {
            int tx = x + hoff;
            if (tx >= 0 && tx < (int)cw)
                b[(size_t)ty * cw + tx] = a[(size_t)y * cw + x];
        }
    }

    unsigned wb = (cw + 7) >> 3;
    for (unsigned y = 0; y < ch && !canceled; y++) {
        const unsigned char *row = b + (size_t)y * cw;
        for (unsigned k = 0; k < wb; k++) {
            unsigned char bits = 0;
            for (unsigned bit = 0; bit < 8; bit++) {
                unsigned x = k * 8 + bit;
                if (x < cw && row[x] <= 0xAF)
                    bits |= 0x80 >> bit;
            }
            putchar((unsigned char)~bits);
        }
    }

    free(line), free(a), free(b);
    return !canceled;
}

int main(int argc, char *argv[])
{
    if (argc < 6 || argc > 7) {
        fputs("Usage: rastertolabeltspl job user title copies options [file]\n", stderr);
        return 1;
    }

    ppd_file_t *ppd = ppdOpenFile(getenv("PPD"));
    if (!ppd) {
        fputs("ERROR: Unable to open the PPD file.\n", stderr);
        return 1;
    }

    int model = ppd->model_number;
    if (model < 20 || model > 22) {
        char path[1024];
        const char *bin = getenv("CUPS_SERVERBIN");
        snprintf(path, sizeof path, "%s/filter/rastertolabeltspl.intel",
                 bin ? bin : "/usr/libexec/cups");
        execv(path, argv);
        fprintf(stderr, "ERROR: Printer model %d needs the original filter (%s) and Rosetta.\n",
                model, path);
        return 1;
    }

    cups_option_t *options = NULL;
    int num_options = cupsParseOptions(argv[5], 0, &options);
    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);

    int fd = 0;
    if (argc == 7 && (fd = open(argv[6], O_RDONLY)) < 0) {
        perror("ERROR: Unable to open the raster file");
        return 1;
    }
    signal(SIGTERM, on_term);

    cups_raster_t *ras = cupsRasterOpen(fd, CUPS_RASTER_READ);
    cups_page_header2_t h;
    int page = 0, ok = 1;

    while (ok && !canceled && cupsRasterReadHeader2(ras, &h)) {
        if (h.cupsBitsPerPixel != 8) {
            fputs("ERROR: Expected an 8-bit grayscale raster.\n", stderr);
            ok = 0;
            break;
        }
        fprintf(stderr, "PAGE: %d 1\n", ++page);
        int rotate = start_page(ppd, model, &h);
        if ((ok = print_page(ras, ppd, &h, rotate)))
            printf(EOL "PRINT 1,1" EOL);
        fflush(stdout);
    }

    cupsRasterClose(ras);
    if (fd)
        close(fd);
    cupsFreeOptions(num_options, options);
    ppdClose(ppd);
    return ok ? 0 : 1;
}
