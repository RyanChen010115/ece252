
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "starter/png_util/lab_png.h"
#include "starter/png_util/crc.c"
#include "starter/png_util/zutil.h"


typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;



int main(int argc, char *argv[]){

    FILE *wr = fopen("./result.png", "wb+");
    int tHeight = 0;
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
        U64 height = *heightPTR;
        U64 width = *widthPTR;
        height = ((height>>24)&0xff) | 
                    ((height<<8)&0xff0000) | 
                    ((height>>8)&0xff00) | 
                    ((height<<24)&0xff000000); 
        width = ((width>>24)&0xff) | 
                    ((width<<8)&0xff0000) | 
                    ((width>>8)&0xff00) | 
                    ((width<<24)&0xff000000); 
        tHeight += height;
        // Reading from IDAT
        U32* length = malloc(sizeof(U32));
        U8 type[4];
        U32* CRC = malloc(sizeof(U32));
        fread(length, sizeof(U32), 1, f);
        fread(type, sizeof(type), 1, f);
        U64 num = *length;
        num = ((num>>24)&0xff) | 
                    ((num<<8)&0xff0000) | 
                    ((num>>8)&0xff00) | 
                    ((num<<24)&0xff000000); 
        tLength += num;
        U8* IDATdata = malloc(sizeof(U8) * num);
        fread(IDATdata, sizeof(U8) * num, 1, f);
        printf("%d, %d", height, width);
        // U8* unComp = malloc(sizeof(U8)*num*2);
        // U64 lenUnComp = 0;
        //mem_inf(unComp, &lenUnComp, IDATdata, num);
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
