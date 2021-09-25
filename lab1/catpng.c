
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "./starter/png_util/lab_png.h"
#include "./starter/png_util/crc.c"
#include "./starter/png_util/zutil.c"



typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;



int main(int argc, char *argv[]){
    const int NUM_FILES = argc - 1;
    FILE *wr = fopen("./result.png", "wb+");
    FILE *IHDR = fopen("./IHDR.png", "wb+");
    U32 tHeight = 0;
    int tLength = 0;
    U64 tLengthUC = 0;
    unsigned char header[8];
    U32 IEND[3];
    U32* IHDRlength = malloc(sizeof(U32));
    U32* IHDRtype = malloc(sizeof(U32));
    U32* widthPTR = malloc(sizeof(U32));
    U32* heightPTR = malloc(sizeof(U32));
    U8 IHDRData[5];
    U8 ICRC[4];
    // unsigned char length[4];
    // unsigned char buf4[4];
    // unsigned char IHDR[17];
    chunk_p chunkPTR[NUM_FILES];
    for(int i = 1; i < argc; i++){
        
        
        FILE *f = fopen(argv[i], "rb");
        if(f == NULL){
            printf("File not found");
            return -1;
        }
        fread(header, sizeof(header), 1, f);
        // Reading from IHDR
        
        

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
        U8 IDATtype[4];
        U32* CRC = malloc(sizeof(U32));
        fread(length, sizeof(U32), 1, f);
        fread(IDATtype, sizeof(IDATtype), 1, f);
        U32 num = *length;
        
        num = ((num>>24)&0xff) | 
                    ((num<<8)&0xff0000) | 
                    ((num>>8)&0xff00) | 
                    ((num<<24)&0xff000000); 
        tLength += num;
        U8* IDATdata = (U8*)malloc( num);
        fread(IDATdata, sizeof(U8) * num, 1, f);
        // Do compression stuff
        // U8* unComp = (U8*)malloc(height*(width*4+1));
        U64 lenUnComp = 0;
        
        chunk_p chunk = malloc(sizeof(chunk_p));
        chunk->length = num;
        chunk->type[0] = IDATtype[0];
        chunk->type[1] = IDATtype[1];
        chunk->type[2] = IDATtype[2];
        chunk->type[3] = IDATtype[3];
        chunk->p_data = (U8*)malloc(height*(width*4+1));
        mem_inf(chunk->p_data, &lenUnComp, IDATdata, num);
        tLengthUC += lenUnComp;


        fread(CRC, sizeof(U32), 1, f);
        chunk->crc = *CRC;
        chunkPTR[i-1] = chunk;

        //Get End Chunk
        fread(IEND, sizeof(IEND), 1, f);

        
        
        
        free(length);
        free(CRC);
        free(IDATdata);
        
        fclose(f);
    }
    //write header to file
    fwrite(header, sizeof(header), 1, wr);
    //write IHDR to file
    fwrite(IHDRlength, sizeof(U32), 1, wr);
    fwrite(IHDRtype, sizeof(U32), 1, IHDR);
    fwrite(widthPTR, sizeof(U32), 1, IHDR);
    fwrite(&tHeight, sizeof(U32), 1, IHDR);
    //write chunk to file
    //write IEND to file
    printf("\n%d", tHeight);
    printf("\n%x", tHeight);
    printf("\n%x", chunkPTR[0]->crc);
    // for(int i = 0; i < NUM_FILES; i++){
    //     free(ptrArr[i]);
    // }
    for(int i = 0; i < NUM_FILES; i++){
        free(chunkPTR[i]);
    }
    fclose(wr);
    fclose(IHDR);
    free(widthPTR);
    free(heightPTR);
    free(IHDRtype);
    free(IHDRlength);
    return 0;
}
