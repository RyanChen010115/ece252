#include <stdio.h>
#include <stdio.h>
#include <string.h>


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
    for(int i = 0; i < 13; i++){
        printf("%c", chunk[i]);
    }
    out->width = (int)chunk[0];
    out->height = (int)chunk[1];
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
    data_IHDR data = {0};
    get_png_data_IHDR(&data, f, 0, 0);
    if(is_png(buf, 8) == 1){
        char* tld = strrchr(argv[1], '/');
        printf("%s: %d x %d", tld, data.width, data.height);
    } else{

    }
    fclose(f);
    return 0;
}