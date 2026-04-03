#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "db.h"

int main() {
    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hMapFile == NULL) {
        printf("Erro ao acessar memoria compartilhada. Inicie o servidor antes.\n");
        return 1;
    }

    SharedMemory *shared_data = (SharedMemory *)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(SharedMemory)
    );

    if (shared_data == NULL) {
        printf("Erro ao mapear memoria compartilhada.\n");
        CloseHandle(hMapFile);
        return 1;
    }

    char input[MAX_MSG];

    while (1) {
        printf("Digite um comando (INSERT id=1 name='Joao', SELECT id=1, UPDATE id=1 name='Maria', LIST, DELETE id=1, EXIT): ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        while (shared_data->ready == 1) {
            Sleep(10);
        }

        strcpy(shared_data->message, input);
        shared_data->ready = 1;

        if (strcmp(input, "EXIT") == 0) {
            break;
        }
    }

    UnmapViewOfFile(shared_data);
    CloseHandle(hMapFile);
    return 0;
}