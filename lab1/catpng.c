#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "starter/png_util/crc.c"
#include "starter/png_util/zutil.h"

typedef unsigned char U8;
typedef unsigned int  U32;


typedef struct data_IHDR {// IHDR chunk data 
    U32 width;        /* width in pixels, big endian   */
    U32 height;       /* height in pixels, big endian  */
    U8  bit_depth;    /* num of bits per sample or per palette index.
                         valid values are: 1, 2, 4, 8, 16 */
    U8  color_type;   /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                         =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8  compression;  /* only method 0 is defined for now */
    U8  filter;       /* only method 0 is defined for now */
    U8  interlace;    /* =0: no interlace; =1: Adam7 interlace */
} data_IHDR;

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, unsigned char* buf){
    uint32_t w = (uint32_t)buf[4] << 24 |
      (uint32_t)buf[5] << 16 |
      (uint32_t)buf[6] << 8  |
      (uint32_t)buf[7];
    out->width = w;
    uint32_t h = (uint32_t)buf[8] << 24 |
      (uint32_t)buf[9] << 16 |
      (uint32_t)buf[10] << 8  |
      (uint32_t)buf[11];
    out->height = h;
    return 1;
}


int main(int argc, char *argv[]){
    const int NUM_FILES = argc - 1;
    FILE *wr = fopen("./result.png", "wb+");
    int height = 0;
    int tLength = 0;
    unsigned char header[8];
    // unsigned char length[4];
    unsigned char buf4[4];
    unsigned char IHDR[17];
    char (*ptrArr)[NUM_FILES];
    for(int i = 1; i < argc; i++){
        FILE *f = fopen(argv[i], "rb");
        fread(header, sizeof(header), 1, f);
        fread(buf4, sizeof(buf4), 1, f);
        fread(IHDR, sizeof(IHDR), 1, f);
        fread(buf4, sizeof(buf4), 1, f);
        data_IHDR data = {0};
        get_png_data_IHDR(&data, f, IHDR);
        printf("\n%d\n", data.height);
        height += data.height;
        // end of IHDR
        fread(buf4, sizeof(buf4), 1, f);
        int l = (uint32_t)buf4[0] << 24 |
            (uint32_t)buf4[1] << 16 |
            (uint32_t)buf4[2] << 8  |
            (uint32_t)buf4[3];
        tLength += l;
        ptrArr[i - 1] = malloc(sizeof(char) * l);
        // store read value in static array then use the compress tool to store it in dynamic
        fread(buf4, sizeof(buf4), 1, f);

        
        fclose(f);
    }
    for(int i = 0; i < NUM_FILES; i++){
        free(ptrArr[i]);
    }
    fclose(wr);
    return 0;
}