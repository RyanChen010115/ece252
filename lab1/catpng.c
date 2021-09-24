#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "starter/png_util/crc.c"
#include "starter/png_util/zutil.h"


int main(int argc, char *argv[]){
    FILE *wr = fopen("./result.png", "wb+");
    int height = 0;
    for(int i = 0; i < argc; i++){
        FILE *f = fopen(argv[i], "rb");

        fclose(f);
    }
    fclose(wr);
    return 0;
}