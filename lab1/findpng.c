#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int foundPNG;
typedef unsigned char U8;

int isPng(char *file){
    FILE *f = fopen(file, "rb");

    if(f == NULL){
       printf("Failed: %s\n",file);
       return -1;
    }

    unsigned char buf[16];
    fread(buf, sizeof(buf), 1, f);


    if(buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47 && buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A){
        return 1;
    }
    fclose(f);
    return 0;
}

int open(char *location){
    struct dirent *dir;
    DIR *d;
    d = opendir(location);
    if(d == NULL){
        printf("No such directory path\n");
        return -1;
    }
	struct stat buf;
	char buff[256];
	if (location[0] != '.' || location[1] != '/'){
		strcpy(buff,"./");
	}
    strcat(buff,location);

    while ((dir = readdir(d)) != NULL) {
        char *str_path = dir->d_name;  /* relative path name! */
        char path[256];
        strcpy(path,location);
        if (location[strlen(location)-1]!='/'){
        	strcat(path,"/");
		}
        strcat(path,str_path);
        lstat(path,&buf);

        if (str_path == NULL) {
            fprintf(stderr,"Null pointer found!");
            exit(3);
        } else {
            if (isPng(path)==1){
                printf("%s\n",path);
                foundPNG = 1;
            }

            else if (S_ISDIR(buf.st_mode) && strcmp(str_path, "..") && strcmp(str_path, ".")){
                open(path);
            }
        }
    }

    closedir(d);
    return 0;
}

int main(int argc, char *argv[]){
    foundPNG = 0;
    if(argc <2){
        printf("Include root directory\n");
        return 1;
    }
    open(argv[1]);

    if(foundPNG == 0){
        printf("findpng: No PNG file found\n");
    }
    return 0;
}