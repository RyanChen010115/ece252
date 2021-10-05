#include <lab_png.h>
#include <stdio.h>


int is_png(U8 *buf, size_t n){
    return buf[1] == 0x50 && buff[2] == 0x4E && buf[3] == 0x47;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
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