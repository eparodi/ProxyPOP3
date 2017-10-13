//
// Created by flacu on 13/10/17.
//

#include <stdio.h>
#include <malloc.h>
#include <memory.h>


char *parseMail(char *mail, int size, const char *filter, const char *replacement) {
    int i = 0, j = 0;
    int flag = 1;
    int sizeFilter = strlen(filter);
    int sizeTextPlain = strlen(replacement);
    char *buffer = malloc(strlen(mail));
    strcpy(buffer, mail);
    while (i < size) {
        flag = 1;
        j = 0;
        for (int k = i; j < sizeFilter && flag; k++) {
            if (mail[k] != filter[j++]) {
                j = 0;
                flag = 0;
            }
        }
        if (flag == 1) {
            for (int k = 0; k < sizeTextPlain; k++) {
                mail[i + k] = replacement[k];
            }
            for (int k = 0; k < sizeFilter; k++) {
                mail[i + sizeTextPlain + k] = buffer[i + sizeFilter + k];
            }
        }
        i++;
    }
    strcpy(buffer, mail);
    return buffer;

}

FILE* parser(FILE *fd, char *filter, char *replacement) {
    char *mail;
    long length;
    FILE *returnFile = fopen("parsedMail", "w+");
    fseek(fd, 0, SEEK_END);
    length = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    mail = malloc(length+1);
    mail[length+1] = '\0';
    if (mail) {
        fread(mail, 1, length, fd);
    }
    fclose(fd);

    if (mail) {
        int size = strlen(mail);
        char *parsedMail = parseMail(mail, size, filter, replacement);
        printf(parsedMail);
        fwrite(mail, 1, size, returnFile);
        fclose(returnFile);
        return returnFile;

    }
}
