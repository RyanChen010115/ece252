
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "png_util/lab_png.h"
#include "png_util/crc.c"
#include "png_util/zutil.h"


typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;



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

    FILE *wr = fopen("./result.png", "wb+");
    int height = 0;
    int tLength = 0;
    unsigned char header[8];

    // unsigned char length[4];
    // unsigned char buf4[4];
    // unsigned char IHDR[17];
    // chunk_p chunkPTR[NUM_FILES];
    for(int i = 1; i < argc; i++){
        U32* IHDRlength = malloc(sizeof(U32));
        U32* IHDRtype = malloc(sizeof(U32));
        FILE *f = fopen(argv[i], "rb");
        fread(header, sizeof(header), 1, f);
        // Reading from IHDR
        
        U32* widthPTR = malloc(sizeof(U32));
        U32* heightPTR = malloc(sizeof(U32));
        U8 IHDRData[5];
        U8 ICRC[4];

        fread(IHDRlength, sizeof(U32), 1, f);
        fread(IHDRtype, sizeof(U32), 1, f);
        fread(widthPTR, sizeof(U32), 1, f);
        fread(heightPTR, sizeof(U32), 1, f);
        fread(IHDRData, sizeof(IHDRData), 1, f);
        fread(ICRC, sizeof(ICRC), 1, f);
        U64 num = *heightPTR;
        num = ((num>>24)&0xff) | 
                    ((num<<8)&0xff0000) | 
                    ((num>>8)&0xff00) | 
                    ((num<<24)&0xff000000); 
        height += num;
        // Reading from IDAT
        U32* length = malloc(sizeof(U32));
        U8 type[4];
        U32* CRC = malloc(sizeof(U32));
        fread(length, sizeof(U32), 1, f);
        fread(type, sizeof(type), 1, f);
        num = *length;
        num = ((num>>24)&0xff) | 
                    ((num<<8)&0xff0000) | 
                    ((num>>8)&0xff00) | 
                    ((num<<24)&0xff000000); 
        tLength += num;
        U8* IDATdata = malloc(sizeof(U8) * num);
        fread(IDATdata, sizeof(U8) * num, 1, f);
        U8* unComp = malloc(sizeof(U8)*num*2);
        U64 lenUnComp = 0;
        mem_inf(unComp, &lenUnComp, IDATdata, num);
        fread(CRC, sizeof(U32) * num, 1, f);
        

        
        // fread(buf4, sizeof(buf4), 1, f);
        // fread(IHDR, sizeof(IHDR), 1, f);
        // fread(buf4, sizeof(buf4), 1, f);
        // data_IHDR data = {0};
        // get_png_data_IHDR(&data, f, IHDR);
        // printf("\n%d\n", data.height);
        // height += data.height;
        // // end of IHDR
        // fread(buf4, sizeof(buf4), 1, f);
        // int l = (uint32_t)buf4[0] << 24 |
        //     (uint32_t)buf4[1] << 16 |
        //     (uint32_t)buf4[2] << 8  |
        //     (uint32_t)buf4[3];
        // tLength += l;
        // ptrArr[i - 1] = malloc(sizeof(char) * l);
        // // store read value in static array then use the compress tool to store it in dynamic
        // fread(buf4, sizeof(buf4), 1, f);

        free(IHDRlength);
        free(IHDRtype);
        free(widthPTR);
        free(heightPTR);
        free(length);
        fclose(f);
    }
    // for(int i = 0; i < NUM_FILES; i++){
    //     free(ptrArr[i]);
    // }
    fclose(wr);
    return 0;
}
