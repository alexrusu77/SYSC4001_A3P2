#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/stat.h>

#define MAX_LINES 5
#define MAX_TEXT 100
#define MAX_EXAM_TEXT 512

#define SEM_MODE (S_IRUSR | S_IWUSR)

// Wait : decrement semaphore
void sem_wait(int semid, int sem_num) 
{
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = -1;
    op.sem_flg = 0;
    semop(semid, &op, 1);
}

// Signal : increment semaphore
void sem_signal(int semid, int sem_num) 
{
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = +1;
    op.sem_flg = 0;
    semop(semid, &op, 1);
}

// Shared memory structure
typedef struct 
{
    char rubric[MAX_LINES][MAX_TEXT];
    char exam[MAX_EXAM_TEXT];
    int exam_questions_marked[MAX_LINES];
    int current_exam_index;
    int student_number;

    int exam_loaded;

    int semid;   // semaphore set id
} SharedData;

// Load rubric from file
void load_rubric(SharedData *data) 
{
    FILE *f = fopen("rubric.txt", "r");

    if (!f) 
    {
        perror("Failed to open rubric.txt");
        exit(1);
    }

    for (int i = 0; i < MAX_LINES; i++) 
    {
        if (!fgets(data->rubric[i], MAX_TEXT, f)) {break;}

        size_t len = strlen(data->rubric[i]);

        if (len > 0 && data->rubric[i][len-1] == '\n') { data->rubric[i][len-1] = '\0'; }
    }
    fclose(f);
}

// Load an exam
void load_exam(SharedData *data, int index)
{
    char filename[64];
    sprintf(filename, "exams/exam_%04d.txt", index);

    FILE *f = fopen(filename, "r");
    if (!f) 
    {
        perror("Failed to open exam file");
        exit(1);
    }

    memset(data->exam, 0, MAX_EXAM_TEXT);
    size_t bytes = fread(data->exam, 1, MAX_EXAM_TEXT - 1, f);
    data->exam[bytes] = '\0';
    fclose(f);

    // First line is student number
    char *newline = strchr(data->exam, '\n');

    if (newline) 
    {
        *newline = '\0';
        data->student_number = atoi(data->exam);
        memmove(data->exam, newline+1, strlen(newline+1)+1);
    } 
    else 
    {
        data->student_number = atoi(data->exam);
    }

    for (int i = 0; i < MAX_LINES; i++)
    {
        data->exam_questions_marked[i] = 0;
    }
}

// Save rubric 
void save_rubric(SharedData *data) 
{
    FILE *f = fopen("rubric.txt", "w");

    if (!f) 
    {
        perror("rubric save");
        return;
    }

    for (int i = 0; i < MAX_LINES; i++)
    {
        fprintf(f, "%s\n", data->rubric[i]);
    }
    
    fclose(f);
}

void ta_process(int id, SharedData *data) 
{
    srand(getpid());

    int sem_mutex = 0;      // semaphore 0 = mutex
    int sem_print = 1;      // semaphore 1 = print lock

    while (1) 
    {

        // Check for final exam
        sem_wait(data->semid, sem_mutex);
        int student = data->student_number;
        sem_signal(data->semid, sem_mutex);

        if (student == 9999) 
        {
            sem_wait(data->semid, sem_print);
            printf("TA %d detected final exam (9999). Exiting...\n", id);
            sem_signal(data->semid, sem_print);
            exit(0);
        }

        // Review rubric
        for (int i = 0; i < MAX_LINES; i++) 
        {
            usleep(500000 + rand()%500000);

            if (rand()%5 == 0) 
            {
                // modify rubric inside critical section
                sem_wait(data->semid, sem_mutex);
                data->rubric[i][0] += 1;
                save_rubric(data);
                sem_signal(data->semid, sem_mutex);

                sem_wait(data->semid, sem_print);
                printf("TA %d modified rubric line %d\n", id, i+1);
                sem_signal(data->semid, sem_print);
            }
        }

        // Mark a question
        sem_wait(data->semid, sem_mutex);
        int marked = -1;
        for (int q = 0; q < MAX_LINES; q++) 
        {
            if (data->exam_questions_marked[q] == 0) 
            {
                data->exam_questions_marked[q] = 1;
                marked = q;
                break;
            }
        }

        // Check if exam complete
        int load_next = 0;
        int next_index = 0;

        int all_marked = 1;
        for (int q = 0; q < MAX_LINES; q++) 
        {
            if (data->exam_questions_marked[q] == 0) 
            {
                all_marked = 0;
                break;
            }
        }

        if (all_marked && !data->exam_loaded) 
        {
            data->exam_loaded = 1;
            next_index = data->current_exam_index + 1;
            load_next = 1;
        }

        sem_signal(data->semid, sem_mutex);

        if (marked != -1) 
        {
            sem_wait(data->semid, sem_mutex);
            printf("TA %d marked Question %d for Student %d\n", id, marked+1, student);
            sem_signal(data->semid, sem_mutex);
        }

        if (marked != -1) { usleep(1000000 + rand()%1000000);}

        // Load next exam
        if (load_next) 
        {
            sem_wait(data->semid, sem_mutex);
            data->current_exam_index = next_index;
            load_exam(data, next_index);
            data->exam_loaded = 0;
            sem_signal(data->semid, sem_mutex);

            sem_wait(data->semid, sem_print);
            printf("TA %d loaded exam %d (student %d)\n", id, next_index, data->student_number);
            sem_signal(data->semid, sem_print);
        }
    }
}

int main(int argc, char *argv[]) 
{
    if (argc != 2) 
    {
        printf("Usage: %s <num_TAs>\n", argv[0]);
        return 1;
    }

    int num_TAs = atoi(argv[1]);
    if (num_TAs < 2) { num_TAs = 2; }

    srand(time(NULL));

    // Shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | SEM_MODE);
    if (shmid < 0) { perror("shmget"); exit(1); }

    SharedData *data = (SharedData*)shmat(shmid, NULL, 0);

    // Create semaphore set with 2 semaphores
    data->semid = semget(IPC_PRIVATE, 2, IPC_CREAT | SEM_MODE);
    if (data->semid < 0) { perror("semget"); exit(1); }

    // Initialize both semaphores
    union semun 
    {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;

    unsigned short values[2] = {1, 1};   // mutex = 1, print_lock = 1
    arg.array = values;

    if (semctl(data->semid, 0, SETALL, arg) == -1) 
    {
        perror("semctl");
        exit(1);
    }

    // Load rubric + exam 1
    load_rubric(data);
    data->current_exam_index = 1;
    load_exam(data, 1);
    data->exam_loaded = 0;

    printf("Parent loaded exam 1 (student %d)\n", data->student_number);

    // Fork TA processes
    for (int i = 0; i < num_TAs; i++) 
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            ta_process(i+1, data);
            exit(0);
        }
    }

    // Wait for all children
    for (int i = 0; i < num_TAs; i++)
    { 
        wait(NULL);
    }

    // Cleanup
    semctl(data->semid, 0, IPC_RMID);
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    printf("All TA processes finished. Parent exiting.\n");
    return 0;
}
