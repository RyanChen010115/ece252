#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


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

int is_png(U8 *buf, size_t n){
    if(buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47){
        return 1;
    }
    return 0;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
    unsigned char chunk[13];
    fread(chunk, sizeof(chunk), 1, fp);
    uint32_t w = (uint32_t)chunk[0] << 24 |
      (uint32_t)chunk[1] << 16 |
      (uint32_t)chunk[2] << 8  |
      (uint32_t)chunk[3];
    out->width = w;
    uint32_t h = (uint32_t)chunk[4] << 24 |
      (uint32_t)chunk[5] << 16 |
      (uint32_t)chunk[6] << 8  |
      (uint32_t)chunk[7];
    out->height = h;
    return 1;
}

int main(int argc, char *argv[]){
    FILE *f = fopen(argv[1], "rb");
    if(f == NULL){
       printf("Failed");
       return -1;
    }
    unsigned char buf[8];
    fread(buf, sizeof(buf), 1, f);
    
    if(is_png(buf, 8) == 1){
        fread(buf, sizeof(buf), 1, f);
        data_IHDR data = {0};
        get_png_data_IHDR(&data, f, 0, 0);
        char* tld = strrchr(argv[1], '/');
        printf("%s: %d x %d\n", tld + sizeof(char), data.width, data.height);
        unsigned char length[4];
        unsigned char crc[4];
        fread(crc, sizeof(crc), 1, f);
        //compare CRC
        printf("%x", crc[0]);
        fread(length, sizeof(length), 1, f); // read length
        uint32_t l = (uint32_t)length[0] << 24 |
            (uint32_t)length[1] << 16 |
            (uint32_t)length[2] << 8  |
            (uint32_t)length[3];
        fread(length, sizeof(length), 1, f); // read type
        for(int i = 0; i < l; i += 4){ // read data
            fread(length, sizeof(length), 1, f);
        }
        fread(crc, sizeof(crc), 1, f); // read crc
        // compare CRC
        printf("%x", crc[0]);
        fread(length, sizeof(length), 1, f);
        fread(length, sizeof(length), 1, f);
        fread(crc, sizeof(crc), 1, f);
        //compare CRC
        printf("%x", crc[0]);
    } else{
        char* tld = strrchr(argv[1], '/');
        printf("%s: Not a PNG file\n", tld + 4);
    }
    
    fclose(f);
    return 0;
}