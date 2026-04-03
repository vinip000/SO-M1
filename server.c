#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

CRITICAL_SECTION queue_mutex;
CRITICAL_SECTION file_mutex;
CONDITION_VARIABLE queue_cond;

TaskQueue task_queue;
SharedMemory *shared_data = NULL;

void enqueue(Task t) {
    EnterCriticalSection(&queue_mutex);

    if (task_queue.count < QUEUE_SIZE) {
        task_queue.queue[task_queue.rear] = t;
        task_queue.rear = (task_queue.rear + 1) % QUEUE_SIZE;
        task_queue.count++;
        WakeConditionVariable(&queue_cond);
    } else {
        printf("Fila cheia. Comando descartado.\n");
    }

    LeaveCriticalSection(&queue_mutex);
}

Task dequeue() {
    Task t;

    EnterCriticalSection(&queue_mutex);

    while (task_queue.count == 0) {
        SleepConditionVariableCS(&queue_cond, &queue_mutex, INFINITE);
    }

    t = task_queue.queue[task_queue.front];
    task_queue.front = (task_queue.front + 1) % QUEUE_SIZE;
    task_queue.count--;

    LeaveCriticalSection(&queue_mutex);

    return t;
}

void insert_record(Record r) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "ab");
    if (fp != NULL) {
        fwrite(&r, sizeof(Record), 1, fp);
        fclose(fp);
        printf("Registro inserido com sucesso. ID: %d, Name: %s\n", r.id, r.name);
    } else {
        printf("Erro ao abrir db.txt para escrita.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

void delete_record(int id) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    FILE *tmp = fopen("tmp.txt", "wb");
    Record r;
    int found = 0;

    if (fp != NULL && tmp != NULL) {
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            if (r.id != id) {
                fwrite(&r, sizeof(Record), 1, tmp);
            } else {
                found = 1;
            }
        }

        fclose(fp);
        fclose(tmp);

        remove("db.txt");
        rename("tmp.txt", "db.txt");

        if (found) {
            printf("Registro com ID %d removido com sucesso.\n", id);
        } else {
            printf("Registro com ID %d nao encontrado.\n", id);
        }
    } else {
        if (fp) fclose(fp);
        if (tmp) fclose(tmp);
        printf("Erro ao abrir arquivos para delete.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

void list_records() {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    Record r;

    if (fp != NULL) {
        printf("=== LISTA DE REGISTROS ===\n");
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            printf("ID: %d, Name: %s\n", r.id, r.name);
        }
        fclose(fp);
    } else {
        printf("db.txt ainda nao existe ou nao pode ser aberto.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

void select_record(int id) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    Record r;
    int found = 0;

    if (fp != NULL) {
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            if (r.id == id) {
                printf("Registro encontrado -> ID: %d, Name: %s\n", r.id, r.name);
                found = 1;
                break;
            }
        }
        fclose(fp);

        if (!found) {
            printf("Registro com ID %d nao encontrado.\n", id);
        }
    } else {
        printf("db.txt ainda nao existe ou nao pode ser aberto.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

void update_record(int id, const char *new_name) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    FILE *tmp = fopen("tmp.txt", "wb");
    Record r;
    int found = 0;

    if (fp != NULL && tmp != NULL) {
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            if (r.id == id) {
                strncpy(r.name, new_name, MAX_NAME - 1);
                r.name[MAX_NAME - 1] = '\0';
                found = 1;
            }
            fwrite(&r, sizeof(Record), 1, tmp);
        }

        fclose(fp);
        fclose(tmp);

        remove("db.txt");
        rename("tmp.txt", "db.txt");

        if (found) {
            printf("Registro com ID %d atualizado com sucesso. Novo nome: %s\n", id, new_name);
        } else {
            printf("Registro com ID %d nao encontrado.\n", id);
        }
    } else {
        if (fp) fclose(fp);
        if (tmp) fclose(tmp);
        printf("Erro ao abrir arquivos para update.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

DWORD WINAPI thread_worker(LPVOID arg) {
    (void)arg;

    while (1) {
        Task task = dequeue();
        printf("[Thread] Processando: %s\n", task.command);

        if (strncmp(task.command, "INSERT", 6) == 0) {
            Record r;
            memset(&r, 0, sizeof(r));

            if (sscanf(task.command, "INSERT id=%d name='%49[^']'", &r.id, r.name) == 2) {
                insert_record(r);
            } else {
                printf("Comando INSERT invalido.\n");
            }
        }
        else if (strncmp(task.command, "DELETE", 6) == 0) {
            int id;
            if (sscanf(task.command, "DELETE id=%d", &id) == 1) {
                delete_record(id);
            } else {
                printf("Comando DELETE invalido.\n");
            }
        }
        else if (strncmp(task.command, "SELECT", 6) == 0) {
            int id;
            if (sscanf(task.command, "SELECT id=%d", &id) == 1) {
                select_record(id);
            } else {
                printf("Comando SELECT invalido.\n");
            }
        }
        else if (strncmp(task.command, "UPDATE", 6) == 0) {
            int id;
            char new_name[MAX_NAME];

            if (sscanf(task.command, "UPDATE id=%d name='%49[^']'", &id, new_name) == 2) {
                update_record(id, new_name);
            } else {
                printf("Comando UPDATE invalido.\n");
            }
        }
        else if (strncmp(task.command, "LIST", 4) == 0) {
            list_records();
        }
        else {
            printf("Comando desconhecido.\n");
        }
    }

    return 0;
}

int main() {
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedMemory),
        SHARED_MEMORY_NAME
    );

    if (hMapFile == NULL) {
        printf("Erro ao criar memoria compartilhada.\n");
        return 1;
    }

    shared_data = (SharedMemory *)MapViewOfFile(
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

    InitializeCriticalSection(&queue_mutex);
    InitializeCriticalSection(&file_mutex);
    InitializeConditionVariable(&queue_cond);

    shared_data->ready = 0;
    memset(shared_data->message, 0, sizeof(shared_data->message));

    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;

    HANDLE threads[THREAD_POOL_SIZE];

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        threads[i] = CreateThread(NULL, 0, thread_worker, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("Erro ao criar thread %d.\n", i);
        }
    }

    printf("[Servidor] Aguardando comandos do cliente...\n");

    while (1) {
        if (shared_data->ready == 1) {
            if (strcmp(shared_data->message, "EXIT") == 0) {
                shared_data->ready = 0;
                break;
            }

            Task t;
            strcpy(t.command, shared_data->message);
            enqueue(t);
            shared_data->ready = 0;
        }

        Sleep(10);
    }

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (threads[i] != NULL) {
            TerminateThread(threads[i], 0);
            CloseHandle(threads[i]);
        }
    }

    DeleteCriticalSection(&queue_mutex);
    DeleteCriticalSection(&file_mutex);

    UnmapViewOfFile(shared_data);
    CloseHandle(hMapFile);

    return 0;
}