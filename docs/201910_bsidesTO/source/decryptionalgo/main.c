#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void encryption(char password[], FILE* fichierIn, FILE* fichierOut);

void viderBuffer();

int lire(char *chaine, int longueur);

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("\nUsage: ./encryptionMatrix SOURCE DESTINATION [QUICK]\n");
        exit(1);
    }

    FILE *fichierIn = NULL;
    FILE *fichierOut = NULL;
    char defaultPass[] = "Pa55w0rd!";
    char password[64];
    char password2[64];

    fichierIn = fopen(argv[1], "r");
    fichierOut = fopen(argv[2], "w");

    if (fichierIn == NULL)
    {
        printf("Can't open source file! ");
        exit(1);
    }

    printf("\n===Encryption matrix===\n");

    if (argv[3] != NULL)
        encryption(defaultPass, fichierIn, fichierOut);

    else
    {
        do
        {
            printf("\nEnter your encryption password: ");
            lire(password, 64);
            printf("\nRe-enter your password: ");
            lire(password2, 64);

        }while (strcmp(password, password2));

        encryption(password, fichierIn, fichierOut);
    }

    fclose(fichierIn);
    fclose(fichierOut);

    printf("\nFile encrypted!\n");

    return 0;
}

void encryption(char password[], FILE* fichierIn, FILE* fichierOut)
{
    int i = 0;
    int caractere = 0;
    char encryptChar = 0;
    int secret = 0;

    do
    {
        secret += password[i];
        i++;

    } while (password[i] != '\0');

    rewind(fichierIn);
    rewind(fichierOut);

    caractere = fgetc(fichierIn);

    i = 0;

    while (caractere != EOF)
    {
        encryptChar = password[i];

        if (encryptChar == '\0')
        {
            i = 0;
            encryptChar = password[i];
        }

        caractere = caractere - (strlen(password) + (secret % 10)) + encryptChar;

        fputc(caractere, fichierOut);

        caractere = fgetc(fichierIn);

        i++;
    }
}

void viderBuffer()
{
    int c = 0;
    while (c != '\n' && c != EOF)
    {
        c = getchar();
    }
}

int lire(char *chaine, int longueur)
{
    char *positionEntree = NULL;

    if (fgets(chaine, longueur, stdin) != NULL)
    {
        positionEntree = strchr(chaine, '\n');
        if (positionEntree != NULL)
        {
            *positionEntree = '\0';
        }
        else
        {
            viderBuffer();
        }
        return 1;
    }
    else
    {
        viderBuffer();
        return 0;
    }
}
