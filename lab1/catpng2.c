#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include "./starter/png_util/lab_png.h"
#include "./starter/png_util/crc.c"
#include "./starter/png_util/zutil.c"

U32 swap_uint32(U32 val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

void output_png_bytes(int *a, size_t length)
{
    for (int i = 0; i < length; i++)
    {
        printf("%x", a[i]);
    }
}

void output_all_chunk_info(chunk_p png)
{
    printf(" Length %x", png->length);
    for (int i = 0; i < 4; i++)
    {
        printf(" %x ", png->type[i]);
    }
    printf(" CRC %x ", png->crc);
    printf("\n");
}

void output_chunk_p_data(chunk_p data)
{
    printf("\nPRINTING CHUNK DATA: ");
    for (int i = 0; i < data->length; i++)
    {
        printf("%x", data->p_data[i]);
    }
    printf("\n");
}

size_t PNG_SIG[] = {2303741511, 218765834};

int is_png2(U32 *buf)
{
    for (int i = 0; i < 2; i++)
    {
        buf[i] = swap_uint32(buf[i]);
        if (buf[i] != PNG_SIG[i])
            return 0;
    }
    return 1;
}

int get_png_height(struct data_IHDR *buf)
{
    return swap_uint32(buf->height);
}
int get_png_width(struct data_IHDR *buf)
{
    return swap_uint32(buf->width);
}
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence)
{
    fseek(fp, offset, whence);
    fread(out, 1, DATA_IHDR_SIZE, fp);
    rewind(fp);
    return 0;
}

//this function is to copy values of chunk type
void copy_type(chunk_p chunk_p, U8 *type)
{
    for (int i = 0; i < 4; i++)
    {
        chunk_p->type[i] = type[i];
    }
}

//fread updates where the file pointer currently is
// no need to big endian type
chunk_p get_png_chunk_data(chunk_p chunk, FILE *fp, long offset, int whence)
{
    U32 *length_p = malloc(sizeof(U32));
    U8 *type_p = malloc(sizeof(U8) * 4);
    U32 *crc_p = malloc(sizeof(U32));

    fseek(fp, offset, whence);
   

    //read and convert length
    fread(length_p, 1, sizeof(U32), fp);
   
    U32 length = swap_uint32(*length_p);

    fread(type_p, 1, sizeof(U32), fp);
    
    chunk = (chunk_p)malloc(sizeof(chunk_p) + sizeof(length_p) + sizeof(type_p)+ sizeof(crc_p) + length);
    chunk->length = *length_p;
    copy_type(chunk, type_p);
  
    chunk->p_data = (U8 *)malloc(length);
    fread(chunk->p_data, 1, length, fp);

    fread(crc_p, 1, sizeof(U32), fp);
    chunk->crc = *crc_p;

    free(length_p);
    free(type_p);
    free(crc_p);


    return chunk;
}

void instantiate_and_validate_png_signature(FILE *png_file, U32 *png_sig)
{
    fread(png_sig, 1, PNG_SIG_SIZE, png_file);
    fseek(png_file, 8, SEEK_SET);
    if (!is_png2(png_sig))
    {
        printf("\nNOT A PNG\n");
    }
    rewind(png_file);
}

simple_PNG_p initialize_simple_png(FILE *fp)
{
    fseek(fp, 0L, SEEK_END);
    U64 file_size = ftell(fp);
    rewind(fp);
    simple_PNG_p simple_png = (simple_PNG_p)malloc(file_size);

    simple_png->p_IHDR = get_png_chunk_data(simple_png->p_IHDR, fp, 8, SEEK_SET);

    simple_png->p_IDAT = get_png_chunk_data(simple_png->p_IDAT, fp, 0, SEEK_CUR);

    simple_png->p_IEND = get_png_chunk_data(simple_png->p_IEND, fp, 0, SEEK_CUR);

    return simple_png;
}

void write_data_to_file(U32 *png_signature, simple_PNG_p simple_png, FILE *final_png, U64 IDAT_DATA_SIZE)
{

    for (int i = 0; i < 2; i++)
    {
        png_signature[i] = htonl(png_signature[i]);
        //printf("%x ", png_signature[i]);
    }
    fwrite(png_signature, 1, PNG_SIG_SIZE, final_png);

    int IHDR_crc = swap_uint32(simple_png->p_IHDR->crc);
     printf("%x", IHDR_crc);
    fwrite(&simple_png->p_IHDR->length, 1, CHUNK_LEN_SIZE, final_png);
    fwrite(simple_png->p_IHDR->type, 1, CHUNK_TYPE_SIZE, final_png);
    fwrite(simple_png->p_IHDR->p_data, 1, DATA_IHDR_SIZE, final_png);
    fwrite(&IHDR_crc, 1, CHUNK_CRC_SIZE, final_png);

    int IDAT_length = swap_uint32(IDAT_DATA_SIZE);
    fwrite(&IDAT_length, 1, CHUNK_LEN_SIZE, final_png);
    fwrite(simple_png->p_IDAT->type, 1, CHUNK_TYPE_SIZE, final_png);
    fwrite(simple_png->p_IDAT->p_data, 1, IDAT_DATA_SIZE, final_png);

    int IDAT_crc = swap_uint32(simple_png->p_IDAT->crc);
    fwrite(&IDAT_crc, 1, CHUNK_CRC_SIZE, final_png);

    fwrite(&simple_png->p_IEND->length, 1, CHUNK_LEN_SIZE, final_png);
    fwrite(simple_png->p_IEND->type, 1, CHUNK_TYPE_SIZE, final_png);
    fwrite(&simple_png->p_IEND->crc, 1, CHUNK_CRC_SIZE, final_png);
}

simple_PNG_p merge_IHDR(U32 *png_signature, simple_PNG_p simple_png, simple_PNG_p simple_png_2)
{
    simple_PNG_p simple_png_merged = simple_png;


    data_IHDR_p IHDR_1 = (data_IHDR_p)simple_png->p_IHDR->p_data;
    data_IHDR_p IHDR_2 = (data_IHDR_p)simple_png_2->p_IHDR->p_data;
    data_IHDR_p IHDR_merged = (data_IHDR_p)simple_png->p_IHDR->p_data;

    U32 total_height = IHDR_1->height + IHDR_2->height;
    IHDR_merged->height = total_height;

    simple_png_merged->p_IHDR->p_data = (U8 *)IHDR_merged;

    return simple_png_merged;
}

U8 *merge_uncompressed_IDAT(U8 *decompressed_IDAT_data, U64 IDAT_length_decompressed, U8 *decompressed_IDAT_data_2, U64 IDAT_length_decompressed_2)
{
    int total_length = IDAT_length_decompressed + IDAT_length_decompressed_2;
    U8 *combined_IDAT = (U8 *)malloc(total_length);

    for (int i = 0; i < IDAT_length_decompressed; i++)
    {
        combined_IDAT[i] = decompressed_IDAT_data[i];
    }
    for (int i = 0; i < IDAT_length_decompressed_2; i++)
    {
        combined_IDAT[i + IDAT_length_decompressed] = decompressed_IDAT_data_2[i];
    }

    return combined_IDAT;
}
simple_PNG_p update_IHDR_CRC(simple_PNG_p simple_png)
{
    U32 total_length = (DATA_IHDR_SIZE + CHUNK_TYPE_SIZE);
    U8 *combined_data = (U8 *)malloc(total_length);
    U32 crc_val = 0;

    for (int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        combined_data[i] = simple_png->p_IHDR->type[i];
    }
    for (int i = 0; i < DATA_IHDR_SIZE; i++)
    {
        combined_data[i + CHUNK_TYPE_SIZE] = simple_png->p_IHDR->p_data[i];
    }
    crc_val = crc(combined_data, total_length);

    simple_png->p_IHDR->crc = crc_val;

    free(combined_data);

    return simple_png;
}

simple_PNG_p update_IDAT_CRC(simple_PNG_p simple_png)
{
    U32 IDAT_length = simple_png->p_IDAT->length;
    U32 total_length = (IDAT_length + CHUNK_TYPE_SIZE);
    U8 *combined_data = (U8 *)malloc(total_length);
    U32 crc_val = 0;

    for (int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        combined_data[i] = simple_png->p_IDAT->type[i];
    }
    for (int i = 0; i < IDAT_length; i++)
    {
        combined_data[i + CHUNK_TYPE_SIZE] = simple_png->p_IDAT->p_data[i];
    }
    crc_val = crc(combined_data, total_length);

    simple_png->p_IDAT->crc = crc_val;

    free(combined_data);
    return simple_png;
}

simple_PNG_p update_IDAT_merged(simple_PNG_p simple_png_1, simple_PNG_p simple_png_2, U8 *compressed_merged_IDAT, U32 total_length)
{
    free(simple_png_1->p_IDAT->p_data);
    simple_png_1->p_IDAT->p_data = compressed_merged_IDAT;
    simple_png_1->p_IDAT->length = total_length;
    return simple_png_1;
}

int main(int argc, char *argv[])
{
    //Step 1, find a way to get all the bytes in an organized format.
    simple_PNG_p simple_png;
    simple_PNG_p simple_png_2;
    U32 *png_signature;
    //U32 * png_signature_2;

    png_signature = (U32 *)malloc(PNG_SIG_SIZE);

 


 

    FILE *png_file = fopen(argv[1], "rb");
    FILE *final_png = fopen("all.png", "wb+");
    instantiate_and_validate_png_signature(png_file, png_signature);
    simple_png = initialize_simple_png(png_file);



    U64 IDAT_compressed_length;
    
    for (int i = 2; i < argc; i++)
    {
        FILE *png_file_2 = fopen(argv[i], "rb");
        
        if (png_file == NULL || png_file_2 == NULL)
        {
            //fprintf(stderr, "Couldn't open %s: %s\n", "uweng.png", strerror(errno));
            exit(1);
        }
 
        simple_png_2 = initialize_simple_png(png_file_2);
        
        data_IHDR_p IHDR_data = (data_IHDR_p)simple_png->p_IHDR;
        data_IHDR_p IHDR_data_2 = (data_IHDR_p)simple_png_2->p_IHDR;

        U64 IDAT_length_decompressed = (U64)((IHDR_data->height) * (IHDR_data->width * 4 + 1));
        
        U8 *decompressed_IDAT_data = (U8 *)malloc(IDAT_length_decompressed);
        mem_inf(decompressed_IDAT_data, &IDAT_length_decompressed, simple_png->p_IDAT->p_data, (U64)simple_png->p_IDAT->length);

        U64 IDAT_length_decompressed_2 = (U64)((IHDR_data_2->height) * (IHDR_data_2->width * 4 + 1));
        U8 *decompressed_IDAT_data_2 = (U8 *)malloc(IDAT_length_decompressed_2);
        mem_inf(decompressed_IDAT_data_2, &IDAT_length_decompressed_2, simple_png_2->p_IDAT->p_data, (U64)simple_png_2->p_IDAT->length);

        U8 *merged_IDAT_data = merge_uncompressed_IDAT(decompressed_IDAT_data, IDAT_length_decompressed, decompressed_IDAT_data_2, IDAT_length_decompressed_2);
        U64 total_length = IDAT_length_decompressed + IDAT_length_decompressed_2;
        IDAT_compressed_length = total_length;
        U8 *compressed_merged_IDAT_data = (U8 *)malloc(total_length);
        //Step 3, compress
        mem_def(compressed_merged_IDAT_data, &IDAT_compressed_length, merged_IDAT_data, total_length, Z_BEST_COMPRESSION);
        simple_png = merge_IHDR(png_signature, simple_png, simple_png_2);
        simple_png = update_IDAT_merged(simple_png, simple_png_2, compressed_merged_IDAT_data, IDAT_compressed_length);


        //free(IHDR_data);
        //free(IHDR_data_2);
        free(decompressed_IDAT_data);
        free(decompressed_IDAT_data_2);
        free(merged_IDAT_data);
        free(simple_png_2->p_IEND->p_data);
        free(simple_png_2->p_IDAT->p_data);
        free(simple_png_2->p_IHDR->p_data);
        
        free(simple_png_2->p_IEND);
        free(simple_png_2->p_IDAT);
        free(simple_png_2->p_IHDR);
        
        free(simple_png_2);

        //free(compressed_merged_IDAT_data);
        fclose(png_file_2);
              
    }



    simple_png = update_IHDR_CRC(simple_png);
    simple_png = update_IDAT_CRC(simple_png);
    

    write_data_to_file(png_signature, simple_png, final_png, IDAT_compressed_length);
    free(simple_png->p_IEND->p_data);
    free(simple_png->p_IDAT->p_data);
    free(simple_png->p_IHDR->p_data);
    free(simple_png->p_IEND);
    free(simple_png->p_IDAT);
    free(simple_png->p_IHDR);
    free(png_signature);

    free(simple_png);
    fclose(png_file);
    fclose(final_png);
    
    return 0;
}
