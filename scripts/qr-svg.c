/*
 * Writes a QR code for a URL as a crisp, standalone SVG.
 *
 *   cc -O2 -Itests/third_party scripts/qr-svg.c tests/third_party/qrcodegen.c \
 *      -o qr-svg && ./qr-svg "https://example.com" out.svg "Description"
 *
 * scripts/make-site-qr.sh uses it for the install codes on the website.
 */
#include <stdio.h>

#include "qrcodegen.h"

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <url> <output.svg> <description>\n", argv[0]);
        return 2;
    }

    uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    if (!qrcodegen_encodeText(argv[1], temp, qr, qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                              qrcodegen_Mask_AUTO, true)) {
        fprintf(stderr, "text too long for a QR code\n");
        return 1;
    }

    FILE *svg = fopen(argv[2], "w");
    if (svg == NULL) {
        perror(argv[2]);
        return 1;
    }

    const int size = qrcodegen_getSize(qr);
    const int border = 4;
    const int total = size + 2 * border;
    fprintf(svg,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" "
            "shape-rendering=\"crispEdges\" role=\"img\">"
            "<title>%s</title>"
            "<rect width=\"100%%\" height=\"100%%\" fill=\"#fff\"/>"
            "<path fill=\"#000\" d=\"",
            total, total, argv[3]);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qrcodegen_getModule(qr, x, y)) {
                fprintf(svg, "M%d %dh1v1h-1z", x + border, y + border);
            }
        }
    }
    fputs("\"/></svg>\n", svg);
    return fclose(svg) == 0 ? 0 : 1;
}
