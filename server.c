// Inclusão de Bibliotecas e Variáveis Globais
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

// Mutex para proteger a fila de tarefas (producer-consumer)
CRITICAL_SECTION queue_mutex;

// Mutex para proteger acesso ao arquivo (evitar concorrência)
CRITICAL_SECTION file_mutex;

// Variável de condição para acordar threads quando há tarefas
CONDITION_VARIABLE queue_cond;

// Fila de tarefas compartilhada entre threads
TaskQueue task_queue;

// Memória compartilhada com o cliente
SharedMemory *shared_data = NULL;

volatile int running = 1;

// Adiciona uma tarefa na fila
void enqueue(Task t) {
    EnterCriticalSection(&queue_mutex);

    if (task_queue.count < QUEUE_SIZE) {
        // Inserção circular na fila
        task_queue.queue[task_queue.rear] = t;
        task_queue.rear = (task_queue.rear + 1) % QUEUE_SIZE;
        task_queue.count++;

        // Acorda uma thread que esteja esperando tarefa
        WakeConditionVariable(&queue_cond);
    } else {
        printf("Fila cheia. Comando descartado.\n");
    }

    LeaveCriticalSection(&queue_mutex);
}

// Remove uma tarefa da fila (bloqueia se estiver vazia)
Task dequeue() {
    Task t;

    EnterCriticalSection(&queue_mutex);

    // Espera até existir pelo menos uma tarefa
    while (task_queue.count == 0) {
        SleepConditionVariableCS(&queue_cond, &queue_mutex, INFINITE);
    }

    // Remove da frente da fila
    t = task_queue.queue[task_queue.front];
    task_queue.front = (task_queue.front + 1) % QUEUE_SIZE;
    task_queue.count--;

    LeaveCriticalSection(&queue_mutex);

    return t;
}

// Insere um novo registro no arquivo
void insert_record(Record r) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "ab");  // modo append binário
    if (fp != NULL) {
        fwrite(&r, sizeof(Record), 1, fp);
        fclose(fp);
        printf("Registro inserido com sucesso. ID: %d, Name: %s\n", r.id, r.name);
    } else {
        printf("Erro ao abrir db.txt para escrita.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

// Remove um registro com determinado ID
void delete_record(int id) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    FILE *tmp = fopen("tmp.txt", "wb");
    Record r;
    int found = 0;

    if (fp != NULL && tmp != NULL) {
        // Copia todos os registros exceto o que será removido
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            if (r.id != id) {
                fwrite(&r, sizeof(Record), 1, tmp);
            } else {
                found = 1;
            }
        }

        // Substitui arquivo original pelo temporário
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

// Lista todos os registros do arquivo
void list_records() {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    Record r;
    int has_records = 0;

    if (fp != NULL) {
        printf("=== LISTA DE REGISTROS ===\n");

        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            printf("ID: %d, Name: %s\n", r.id, r.name);
            has_records = 1;
        }

        if (!has_records) {
            printf("Nenhum registro encontrado.\n");
        }

        fclose(fp);
    } else {
        printf("db.txt ainda nao existe ou nao pode ser aberto.\n");
    }

    LeaveCriticalSection(&file_mutex);
}

// Busca registro completo por ID
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

// Retorna apenas o nome de um registro
void select_name(int id) {
    EnterCriticalSection(&file_mutex);

    FILE *fp = fopen("db.txt", "rb");
    Record r;
    int found = 0;

    if (fp != NULL) {
        while (fread(&r, sizeof(Record), 1, fp) == 1) {
            if (r.id == id) {
                printf("Name: %s\n", r.name);
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

// Atualiza o nome de um registro
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

// Cada thread executa esse loop infinito
DWORD WINAPI thread_worker(LPVOID arg) {
    (void)arg;

    while (running) {
        // Aguarda uma tarefa da fila
        Task task = dequeue();
        printf("[Thread] Processando: %s\n", task.command);

        // Parser simples de comandos estilo SQL

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
            char field[20];

            if (sscanf(task.command, "SELECT %19s WHERE id=%d", field, &id) == 2) {
                if (strcmp(field, "name") == 0) {
                    select_name(id);
                } else if (strcmp(field, "*") == 0) {
                    select_record(id);
                } else {
                    printf("Campo SELECT nao suportado.\n");
                }
            }
            else if (sscanf(task.command, "SELECT id=%d", &id) == 1) {
                select_record(id);
            }
            else {
                printf("Comando SELECT invalido.\n");
                printf("Exemplos validos:\n");
                printf("  SELECT id=1\n");
                printf("  SELECT name WHERE id=1\n");
                printf("  SELECT * WHERE id=1\n");
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
        else if (strcmp(task.command, "EXIT") == 0) {
            break;
        }
        else {
            printf("Comando desconhecido.\n");
        }
    }

    return 0;
}

int main() {

    // Cria memória compartilhada
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

    // Mapeia memória no espaço do processo
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

    // Inicializa sincronização
    InitializeCriticalSection(&queue_mutex);
    InitializeCriticalSection(&file_mutex);
    InitializeConditionVariable(&queue_cond);

    // Inicializa memória compartilhada
    shared_data->ready = 0;
    memset(shared_data->message, 0, sizeof(shared_data->message));

    // Inicializa fila
    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;

    // Cria pool de threads
    HANDLE threads[THREAD_POOL_SIZE];

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        threads[i] = CreateThread(NULL, 0, thread_worker, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("Erro ao criar thread %d.\n", i);
        }
    }

    printf("[Servidor] Aguardando comandos do cliente...\n");

    // Loop principal: lê comandos da memória compartilhada
    while (1) {
        if (shared_data->ready == 1) {
            if (strcmp(shared_data->message, "EXIT") == 0) {
                running = 0;  // avisa todas as threads para parar

                // envia EXIT para cada thread
                for (int i = 0; i < THREAD_POOL_SIZE; i++) {
                    Task t;
                    strcpy(t.command, "EXIT");
                    enqueue(t);
                }

                shared_data->ready = 0;
                break;
            }

            Task t;
            strcpy(t.command, shared_data->message);

            // Envia tarefa para fila
            enqueue(t);

            // Libera para o cliente escrever novamente
            shared_data->ready = 0;
        }

        Sleep(10);
    }

    WaitForMultipleObjects(THREAD_POOL_SIZE, threads, TRUE, INFINITE);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        CloseHandle(threads[i]);
    }

    DeleteCriticalSection(&queue_mutex);
    DeleteCriticalSection(&file_mutex);

    UnmapViewOfFile(shared_data);
    CloseHandle(hMapFile);

    return 0;
}