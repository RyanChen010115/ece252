#include <lab_png.h>
#include <stdio.h>


int is_png(U8 *buf, size_t n){
    return buf[1] == 0x50 && buff[2] == 0x4E && buf[3] == 0x47;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
    char header[8];
    fread(header, sizeof(header), 1, fp);
    out->width = buf + 24 - 8;
    out->height = buf + 24 - 4;
    return 1;
}