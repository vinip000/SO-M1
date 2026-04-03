#ifndef DB_H
#define DB_H

#define MAX_NAME 50
#define MAX_MSG 256
#define QUEUE_SIZE 10
#define THREAD_POOL_SIZE 4

#define SHARED_MEMORY_NAME "Local\\DBSimulatorSharedMemory"

typedef struct {
    int id;
    char name[MAX_NAME];
} Record;

typedef struct {
    char command[MAX_MSG];
} Task;

typedef struct {
    Task queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
} TaskQueue;

typedef struct {
    char message[MAX_MSG];
    volatile long ready;
} SharedMemory;

#endif