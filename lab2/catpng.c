
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "./lab_png.h"
#include "./crc.c"
#include "./zutil.c"



typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;

U32 swap(U32 value)
{
    value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
    return (value << 16) | (value >> 16);
}

void getDataIHDR(FILE *f, data_IHDR_p res, U32* IHDRwidth, U32* IHDRheight){
    U32 *width_ptr = malloc(sizeof(U32));
    U32 *length_ptr = malloc(sizeof(U32));
    U8 *temp_ptr = malloc(sizeof(U8));
    fread(width_ptr, sizeof(U32), 1, f);
    res->width = ((*width_ptr>>24)&0xff) | ((*width_ptr<<8)&0xff0000) | ((*width_ptr>>8)&0xff00) | ((*width_ptr<<24)&0xff000000); 
    *IHDRwidth = *width_ptr;
    fread(length_ptr, sizeof(U32), 1, f);
    res->height = ((*length_ptr>>24)&0xff) | ((*length_ptr<<8)&0xff0000) | ((*length_ptr>>8)&0xff00) | ((*length_ptr<<24)&0xff000000);
    *IHDRheight += *length_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->bit_depth = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->color_type = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->compression = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->filter = *temp_ptr;
    fread(temp_ptr, sizeof(U8), 1, f);
    res->interlace = *temp_ptr;
    free(width_ptr);
    free(length_ptr);
    free(temp_ptr);
    return;
}

U32 getIDAT(FILE *f, chunk_p res){
    U32 *length_ptr = malloc(sizeof(U32));
    U32 *crc_ptr = malloc(sizeof(U32));
    fread(length_ptr, sizeof(U32), 1, f);
    res->length = ((*length_ptr>>24)&0xff) | ((*length_ptr<<8)&0xff0000) | ((*length_ptr>>8)&0xff00) | ((*length_ptr<<24)&0xff000000);
    fread(res->type, sizeof(res->type), 1, f);
    res->p_data = (U8*)malloc(res->length);
    // for(int i = 0; i < res->length; i++){
    //     fread(res->p_data + i, sizeof(U8), 1, f);
    // }
    fread(res->p_data, sizeof(U8)*res->length, 1, f);
    fread(crc_ptr, sizeof(U32), 1, f);
    res->crc = *crc_ptr;
    free(length_ptr);
    free(crc_ptr);
    return *length_ptr;
}

U32 getIHDRcrc(data_IHDR_p IDHRdata, U32* IHDRtype, U32* width, U32* height){
    FILE *write = fopen("./IHDR.png", "wb+");
    fwrite(IHDRtype, sizeof(U32), 1, write);
    fwrite(width, sizeof(U32), 1, write);
    fwrite(height, sizeof(U32), 1, write);
    fwrite(&IDHRdata->bit_depth, sizeof(U8), 1, write);
    fwrite(&IDHRdata->color_type, sizeof(U8), 1, write);
    fwrite(&IDHRdata->compression, sizeof(U8), 1, write);
    fwrite(&IDHRdata->filter, sizeof(U8), 1, write);
    fwrite(&IDHRdata->interlace, sizeof(U8), 1, write);
    fclose(write);
    FILE *read = fopen("./IHDR.png", "rb");
    U8 IHDRcrcdata[17];
    fread(IHDRcrcdata, sizeof(IHDRcrcdata), 1, read);
    U32 crc_val = crc(IHDRcrcdata, 17);
    fclose(read);
    return crc_val;
}

U32 getIDATcrc(chunk_p IDATchunk, U64 length){
    U8 *IDATall = malloc(sizeof(U8) * length + 4);
    IDATall[0] = IDATchunk->type[0];
    IDATall[1] = IDATchunk->type[1];
    IDATall[2] = IDATchunk->type[2];
    IDATall[3] = IDATchunk->type[3];
    for(int i = 4; i < length + 4; i++){
        IDATall[i] = IDATchunk->p_data[i-4];
    }
    U32 crc_val = crc(IDATall, length + 4);
    return crc_val;
}


int main(int argc, char *argv[]){

    const int NUM_FILES = argc - 1;

    U32 totalHeight = 0;
    U32 totalLength = 0;
    U32 totalLengthN = 0;
    U64 totalDecompLength = 0;
    

    unsigned char header[8];
    U32* IHDRlength = malloc(sizeof(U32));
    U32* IHDRtype = malloc(sizeof(U32));
    U32* IHDRCRC = malloc(sizeof(U32));
    U32* IHDRwidth = malloc(sizeof(U32));
    U32* IHDRheight = malloc(sizeof(U32));
    data_IHDR_p IHDRdata = malloc(sizeof(struct data_IHDR));

    chunk_p IDATchunk = malloc(sizeof(struct chunk));

    chunk_p chunkPTR[NUM_FILES];
    U64 lenArr[NUM_FILES];
    U32 IEND[3];

    for(int i = 1; i < argc; i++){
        printf("%s", argv[i-1]);
        FILE *f = fopen(argv[i], "rb");
        if(f == NULL){
            printf("File not found");
            return -1;
        }

        //Read IDHR
        fread(header, sizeof(header), 1, f);
        fread(IHDRlength, sizeof(U32), 1, f);
        fread(IHDRtype, sizeof(U32), 1, f);
        getDataIHDR(f, IHDRdata, IHDRwidth, IHDRheight);
        fread(IHDRCRC, sizeof(U32), 1, f);
        totalHeight += IHDRdata->height;
        //printf("%d, %d, %x\n", IHDRdata->width, IHDRdata->height, *IHDRCRC);


        //Read IDAT
        totalLength += getIDAT(f, IDATchunk);
        totalLengthN += IDATchunk->length;
        //printf("%d, %x\n", IDATchunk->length, IDATchunk->crc);

        //Uncompress data
        chunk_p tempChunk = malloc(sizeof(struct chunk));
        tempChunk->crc = IDATchunk->crc;
        tempChunk->length = IDATchunk->length;
        tempChunk->type[0] = IDATchunk->type[0];
        tempChunk->type[1] = IDATchunk->type[1];
        tempChunk->type[2] = IDATchunk->type[2];
        tempChunk->type[3] = IDATchunk->type[3];
        U64 decompLength = (U64)((IHDRdata->height)*(IHDRdata->width * 4 + 1));
        //printf("%ld\n", decompLength);
        tempChunk->p_data = (U8*)malloc(decompLength);
        mem_inf(tempChunk->p_data, &decompLength, IDATchunk->p_data, (U64)IDATchunk->length);
        totalDecompLength += decompLength;
        lenArr[i - 1] = decompLength;
        chunkPTR[i - 1] = tempChunk;
        //printf("%ld, %x\n", lenArr[i-1], chunkPTR[i-1]->crc);
        // for(int j = 0; j < IDATchunk->length; j++){
        //     printf("%x", chunkPTR[i-1]->p_data[j]);
        // }
        
        //Read IEND
        fread(IEND, sizeof(IEND), 1, f);
        //printf("%x\n", IEND[2]);

        free(IDATchunk->p_data);
        fclose(f);
    }
    

    U8* uIDATdata = malloc(sizeof(U8)*totalDecompLength);
    int k = 0;
    for(int i = 0; i < NUM_FILES; i++){
        for(int j = 0; j < lenArr[i]; j++){
            uIDATdata[k] = chunkPTR[i]->p_data[j];
            k++;
        }
    }

    U8* fIDATdata = malloc(sizeof(U8)*totalDecompLength);
    U64 IDATcomplength = 0;
    mem_def(fIDATdata, &IDATcomplength, uIDATdata, totalDecompLength, Z_BEST_COMPRESSION);
    //printf("%ld\n", IDATcomplength);
    // for(int i = 0; i < IDATcomplength; i++){
    //     printf("%x", fIDATdata[i]);
    // }

    U32 IHDRcrc = getIHDRcrc(IHDRdata, IHDRtype, IHDRwidth, IHDRheight);
    //printf("%x\n", IHDRcrc);
    //fwrite(&IHDRcrc, sizeof(U32), 1, all);

    chunk_p fIDATchunk = malloc(sizeof(struct chunk));
    
    fIDATchunk->length = IDATcomplength;
    fIDATchunk->type[0] = chunkPTR[0]->type[0];
    fIDATchunk->type[1] = chunkPTR[0]->type[1];
    fIDATchunk->type[2] = chunkPTR[0]->type[2];
    fIDATchunk->type[3] = chunkPTR[0]->type[3];
    fIDATchunk->p_data = fIDATdata;
    U32 IDATcrc = getIDATcrc(fIDATchunk, IDATcomplength);
    //printf("%x\n", IDATcrc);

    //writing to file
    FILE *all = fopen("all.png", "wb+");

    //write header
    fwrite(header, sizeof(header), 1, all);

    //write IHDR
    fwrite(IHDRlength, sizeof(U32), 1, all);
    fwrite(IHDRtype, sizeof(U32), 1, all);
    fwrite(IHDRwidth, sizeof(U32), 1, all);
    fwrite(IHDRheight, sizeof(U32), 1, all);
    fwrite(&IHDRdata->bit_depth, sizeof(U8), 1, all);
    fwrite(&IHDRdata->color_type, sizeof(U8), 1, all);
    fwrite(&IHDRdata->compression, sizeof(U8), 1, all);
    fwrite(&IHDRdata->filter, sizeof(U8), 1, all);
    fwrite(&IHDRdata->interlace, sizeof(U8), 1, all);
    IHDRcrc = swap(IHDRcrc);
    //printf("%x", IHDRcrc);
    fwrite(&IHDRcrc, sizeof(U32), 1, all);

    //write IDAT
    U32 IDATlength = swap(fIDATchunk->length);
    IDATcrc = swap(IDATcrc);
    fwrite(&IDATlength, sizeof(U32), 1, all);
    fwrite(fIDATchunk->type, sizeof(fIDATchunk->type), 1, all);
    fwrite(fIDATchunk->p_data, IDATcomplength, 1, all);
    fwrite(&IDATcrc, sizeof(U32), 1, all);

    //write IEND
    fwrite(IEND, sizeof(IEND), 1, all);


    fclose(all);

    for(int i = 0; i < NUM_FILES; i++){
        free(chunkPTR[i]);
    }

    free(IHDRwidth);
    free(IHDRheight);
    free(fIDATchunk);
    free(fIDATdata);
    free(uIDATdata);
    free(IHDRCRC);
    free(IHDRlength);
    free(IHDRtype);
    free(IHDRdata);


    /*
    const int NUM_FILES = argc - 1;
    FILE *wr = fopen("./all.png", "wb+");
    FILE *IDAT = fopen("./IDAT.png", "wb+");
    U64 width = 0;
    U32 tHeight = 0;
    U64 tLength = 0;
    U64 tLengthUC = 0;
    U32 tL = 0;
    
    U32 IEND[3];
    
    U32* widthPTR = malloc(sizeof(U32));
    U32* heightPTR = malloc(sizeof(U32));
    U8 IHDRData[5];
    U8 ICRC[4];
    U64 lenArr[NUM_FILES];
    // unsigned char length[4];
    // unsigned char buf4[4];
    // unsigned char IHDR[17];
    chunk_p chunkPTR[NUM_FILES];

    unsigned char header[8];
    U32* IHDRlength = malloc(sizeof(U32));
    U32* IHDRtype = malloc(sizeof(U32));


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
        data_IHDR_p IHDR_data = getDataIHDR(f);
        fread(ICRC, sizeof(ICRC), 1, f);

        




        fread(IHDRlength, sizeof(U32), 1, f);
        fread(IHDRtype, sizeof(U32), 1, f);
        fread(widthPTR, sizeof(U32), 1, f);
        fread(heightPTR, sizeof(U32), 1, f);
        fread(IHDRData, sizeof(IHDRData), 1, f);
        fread(ICRC, sizeof(ICRC), 1, f);
        U64 height = *heightPTR;
        width = *widthPTR;
        height = ((height>>24)&0xff) | 
                    ((height<<8)&0xff0000) | 
                    ((height>>8)&0xff00) | 
                    ((height<<24)&0xff000000); 
        width = ((width>>24)&0xff) | 
                    ((width<<8)&0xff0000) | 
                    ((width>>8)&0xff00) | 
                    ((width<<24)&0xff000000); 
        tHeight += *heightPTR;
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
        tL += *length;
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
        lenArr[i] = lenUnComp;

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
    fwrite(IHDRtype, sizeof(U32), 1, wr);
    fwrite(widthPTR, sizeof(U32), 1, wr);
    U32* temp = &tHeight ;
    fwrite(temp, sizeof(temp), 1, wr);
    fwrite(IHDRData, sizeof(IHDRData), 1, wr);
    fwrite(ICRC, sizeof(ICRC), 1, wr);
    //write chunk to file
    fwrite(&tL, sizeof(U32), 1, wr);
    fwrite(chunkPTR[0]->type, sizeof(U32), 1, wr);

    for(int i = 0; i < NUM_FILES; i++){
        fwrite(chunkPTR[i]->p_data, sizeof(U8) * lenArr[i], 1, IDAT);
    }
    fclose(IDAT);
    FILE *IDAT2 = fopen("./IDAT.png", "rb");
    const int uclength = tLengthUC;
    U8 finalChunk[uclength];
    fread(finalChunk, sizeof(finalChunk), 1, IDAT);
    U8 cChunk[tLength];
    mem_def(cChunk, &tLength, finalChunk, uclength, Z_DEFAULT_COMPRESSION);
    fwrite(cChunk, tLength, 1, wr);
    fwrite(&chunkPTR[0]->crc, sizeof(U32), 1, wr);
    //write IEND to file
    fwrite(IEND, sizeof(IEND), 1, wr);

    for(int i = 0; i < NUM_FILES; i++){
        free(chunkPTR[i]);
    }
    fclose(wr);
    fclose(IDAT2);
    free(widthPTR);
    free(heightPTR);
    free(IHDRtype);
    free(IHDRlength);
    */
    return 0;
}
