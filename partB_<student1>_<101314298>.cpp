#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>

#define MAX_LINES 5
#define MAX_TEXT 100
#define MAX_EXAM_TEXT 512

typedef struct {
    char rubric[MAX_LINES][MAX_TEXT];
    char exam[MAX_EXAM_TEXT];
    int exam_questions_marked[MAX_LINES];
    int current_exam_index;
    int student_number;

    sem_t mutex;
    sem_t print_lock;

    int exam_loaded;
} SharedData;

// Load rubric from file
void load_rubric(SharedData *data) {
    FILE *f = fopen("rubric.txt", "r");
    if (!f) {
        perror("Failed to open rubric.txt");
        exit(1);
    }
    int i = 0;
    while (i < MAX_LINES && fgets(data->rubric[i], MAX_TEXT, f)) {
        size_t len = strlen(data->rubric[i]);
        if (len > 0 && data->rubric[i][len-1] == '\n')
            data->rubric[i][len-1] = '\0';
        i++;
    }
    fclose(f);
}

// Load an exam file into shared memory
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
    size_t bytes = fread(data->exam, 1, MAX_EXAM_TEXT-1, f);
    data->exam[bytes] = '\0';
    fclose(f);

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
        data->exam_questions_marked[i] = 0;
}

// Write rubric to file
void save_rubric(SharedData *data) 
{
    FILE *f = fopen("rubric.txt", "w");

    if (!f) 
    { 
        perror("rubric save"); return; 
    }

    for (int i = 0; i < MAX_LINES; i++)
    {
        fprintf(f, "%s", data->rubric[i]);
    }

    fclose(f);
}

// TA process loop
void ta_process(int id, SharedData *data) 
{
    srand(getpid());
    int last_rubric_line = -1;

    while (1) 
    {
        // 1. Check for final exam
        sem_wait(&data->mutex);
        int student = data->student_number;
        sem_post(&data->mutex);

        if (student == 9999) 
        {
            sem_wait(&data->print_lock);
            printf("TA %d detected final exam (9999). Exiting...\n", id);
            sem_post(&data->print_lock);
            exit(0);
        }

        // 2. Review rubric
        for (int i = 0; i < MAX_LINES; i++) 
        {
            usleep(500000 + rand()%500000);
            if ((rand()%5 == 0) && (i != last_rubric_line)) 
            {
                sem_wait(&data->mutex);
                data->rubric[i][0] += 1;
                save_rubric(data);
                sem_post(&data->mutex);

                sem_wait(&data->print_lock);
                printf("TA %d modified rubric line %d\n", id, i+1);
                sem_post(&data->print_lock);

                last_rubric_line = i;
            }
        }

        // 3. Mark a question
        int marked_question = -1;
        sem_wait(&data->mutex);
        for (int q = 0; q < MAX_LINES; q++) 
        {
            if (data->exam_questions_marked[q] == 0) 
            {
                data->exam_questions_marked[q] = 1;
                marked_question = q;
                break;
            }
        }

        // Check if all questions are marked
        int load_next = 0;
        int next_exam_index = 0;
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
            next_exam_index = data->current_exam_index + 1;
            load_next = 1;
        }
        sem_post(&data->mutex);

        if (marked_question != -1) 
        {
            sem_wait(&data->print_lock);
            printf("TA %d marked Question %d for Student %d\n", id, marked_question+1, student);
            sem_post(&data->print_lock);
        }

        if (marked_question != -1)
        {
            usleep(1000000 + rand()%1000000);
        }

        // Load next exam if needed
        if (load_next) 
        {
            sem_wait(&data->mutex);
            data->current_exam_index = next_exam_index;
            load_exam(data, next_exam_index);
            data->exam_loaded = 0;
            sem_post(&data->mutex);

            sem_wait(&data->print_lock);
            printf("TA %d loaded exam %d (student %d)\n", id, next_exam_index, data->student_number);
            sem_post(&data->print_lock);
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
    if (num_TAs < 2) 
    {
        printf("Number of TAs must be at least 2.\n");
        return 1;
    }

    srand(time(NULL));

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }

    SharedData *data = (SharedData *)shmat(shmid, NULL, 0);

    sem_init(&data->mutex, 1, 1);
    sem_init(&data->print_lock, 1, 1);

    load_rubric(data);

    // Load first exam directly into shared memory
    data->current_exam_index = 1;
    load_exam(data, 1);
    sem_wait(&data->print_lock);
    printf("Initial exam loaded: exam 1 (student %d)\n", data->student_number);
    sem_post(&data->print_lock);
    data->exam_loaded = 0;

    for (int i = 0; i < num_TAs; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            ta_process(i+1, data);
            exit(0);
        }
    }

    for (int i = 0; i < num_TAs; i++)
    {
        wait(NULL);
    }

    sem_destroy(&data->mutex);
    sem_destroy(&data->print_lock);
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    printf("All TA processes finished. Parent exiting.\n");
    return 0;
}

