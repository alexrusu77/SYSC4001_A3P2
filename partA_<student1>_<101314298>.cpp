#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_LINES 5
#define MAX_TEXT 100
#define MAX_EXAM_TEXT 512

// Shared memory structure
typedef struct 
{
    char rubric[MAX_LINES][MAX_TEXT];
    char exam_text[MAX_EXAM_TEXT];
    int exam_questions_marked[5];
    int current_exam_index;
    int student_number;
} SharedData;

// Load rubric into shared memory
void load_rubric(SharedData *data) 
{
    FILE *f = fopen("rubric.txt", "r");
    if (!f) 
    {
        perror("rubric");
        exit(1);
    }

    for (int i = 0; i < MAX_LINES; i++)
    {
        fgets(data->rubric[i], MAX_TEXT, f);
    }
    fclose(f);
}

// Write rubric back to file (race conditions allowed)
void save_rubric(SharedData *data) 
{
    FILE *f = fopen("rubric.txt", "w");
    if (!f) 
    {
        perror("rubric save");
        return;
    }

    for (int i = 0; i < MAX_LINES; i++)
        fprintf(f, "%s", data->rubric[i]);

    fclose(f);
}

// Load exam file into shared memory
void load_exam(SharedData *data, int index) 
{
    char filename[64];

    sprintf(filename, "exams/exam_%04d.txt", index);

    FILE *f = fopen(filename, "r");
    if (!f) 
    {
        perror("exam file");
        exit(1);
    }

    memset(data->exam_text, 0, MAX_EXAM_TEXT);
    fread(data->exam_text, 1, MAX_EXAM_TEXT - 1, f);
    fclose(f);

    data->student_number = atoi(data->exam_text);

    for (int i = 0; i < 5; i++)
    {
        data->exam_questions_marked[i] = 0;
    }

    printf("Loaded exam file: %s (student %d)\n", filename, data->student_number);
}

// TA behaviour loop
void ta_process(int id, SharedData *data) 
{

    while (1) 
    {

        printf("TA %d is reviewing the rubric...\n", id);

        // Review and maybe modify the rubric
        for (int i = 0; i < MAX_LINES; i++) 
        {
            usleep(500000 + rand() % 500000);  // 0.5–1.0 seconds

            int modify = rand() % 5 == 0; // 20% chance
            if (modify) 
            {
                printf("TA %d modifying rubric line %d\n", id, i + 1);
                data->rubric[i][3] = data->rubric[i][3] + 1; // Shift ASCII
                save_rubric(data);
            }
        }

        // Mark a question
        printf("TA %d is marking exam for student %d\n", id, data->student_number);

        int marked_one = 0;

        for (int q = 0; q < 5; q++) 
        {
            if (data->exam_questions_marked[q] == 0)
             {
                usleep(1000000 + rand() % 1000000); // 1–2 seconds

                data->exam_questions_marked[q] = 1;

                printf("TA %d marked Question %d for Student %d\n",
                       id, q + 1, data->student_number);

                marked_one = 1;
                break;
            }
        }

        if (!marked_one)
            continue;

        // Check if exam fully marked
        int done = 1;
        for (int q = 0; q < 5; q++)
        {
            if (data->exam_questions_marked[q] == 0)
            {
                done = 0;
            }
        }

        if (done) 
        {
            // Load next exam (race condition allowed)
            data->current_exam_index++;

            load_exam(data, data->current_exam_index);

            if (data->student_number == 9999) 
            {
                printf("TA %d detected final exam (9999). Exiting...\n", id);
                exit(0);
            }
        }
    }
}

// main program
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
        num_TAs = 2;
    }

    srand(time(NULL));

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);

    if (shmid < 0) 
    {
        perror("shmget");
        exit(1);
    }

    SharedData *data = (SharedData* ) shmat(shmid, NULL, 0);

    data->current_exam_index = 1;

    load_rubric(data);
    load_exam(data, 1);

    for (int i = 0; i < num_TAs; i++) 
    {
        pid_t pid = fork();

        if (pid == 0) 
        {
            ta_process(i + 1, data);
            exit(0);
        }
    }

    for (int i = 0; i < num_TAs; i++)
    {
        wait(NULL);
    }


    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    printf("All TAs finished. Program exiting.\n");
    return 0;
}
