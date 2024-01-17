#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <errno.h> // For error handling

#define NUM_PROCESSES 21
#define MATRIX_SIZE 100

typedef struct {
    int pid;
    int nice_value;
    char start_time[100];
    long start_usec;
    char end_time[100];
    long end_usec;
    double elapsed_seconds;
} ProcessInfo;

void perform_matrix_operation() {
    int A[MATRIX_SIZE][MATRIX_SIZE], B[MATRIX_SIZE][MATRIX_SIZE], result[MATRIX_SIZE][MATRIX_SIZE];
    int i, j, k, count = 0;

    for(i = 0; i < MATRIX_SIZE; i++) {
        for(j = 0; j < MATRIX_SIZE; j++) {
            A[i][j] = i;
            B[i][j] = j;
            result[i][j] = 0;
        }
    }

    while(count < 100) {
        for(k = 0; k < MATRIX_SIZE; k++) {
            for(i = 0; i < MATRIX_SIZE; i++) {
                for(j = 0; j < MATRIX_SIZE; j++) {
                    result[k][j] += A[k][i] * B[i][j];
                }
            }
        }
        count++;
    }
}

int compare_nice(const void *a, const void *b) {
    return ((ProcessInfo *)a)->nice_value - ((ProcessInfo *)b)->nice_value;
}

double total_elapsed_seconds = 0.0;

int main() {
    pid_t child_pids[NUM_PROCESSES];
    int policy;
    struct sched_param param;
    double total_elapsed_time = 0;
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    printf("Choose a scheduling policy:\n");
    printf("1. CFS_DEFAULT\n2. CFS_NICE\n3. RT_FIFO\n4. RT_RR\n0. Exit\n");
    scanf("%d", &policy);

    if (policy < 1 || policy > 4) {
        printf("Exiting program.\n");
        exit(1);
    }

    // Set the time quantum for RT_RR before forking the child processes
    int time_quantum = 0;
    if (policy == 4) {
        printf("Enter Time Slice for RT_RR (10, 100, or 1000 ms): ");
        scanf("%d", &time_quantum);
        FILE *fp = fopen("/proc/sys/kernel/sched_rr_timeslice_ms", "w");
        if (fp == NULL) {
            perror("Failed to open sched_rr_timeslice_ms");
            exit(1);
        }
        fprintf(fp, "%d", time_quantum);
        fclose(fp);
    }

    for(int i = 0; i < NUM_PROCESSES; i++) {
        pid_t pid = fork();

        if(pid == 0) { // Child process
            close(pipefd[0]);

            struct timeval start, end;
            gettimeofday(&start, NULL);

            int current_nice_value = 0; // default value

            switch(policy) {
                case 1: // CFS_DEFAULT
                    break;
                case 2: // CFS_NICE
                    if(i < 7) {
                        setpriority(PRIO_PROCESS, 0, -20);
                        current_nice_value = -20;
                    }
                    else if(i < 14) {
                        setpriority(PRIO_PROCESS, 0, 0);
                        current_nice_value = 0;
                    }
                    else {
                        setpriority(PRIO_PROCESS, 0, 19);
                        current_nice_value = 19;
                    }
                    break;
                case 3: // RT_FIFO
                    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
                    sched_setscheduler(0, SCHED_FIFO, &param);
                    break;
                case 4: // RT_RR
                    param.sched_priority = sched_get_priority_max(SCHED_RR);
                    sched_setscheduler(0, SCHED_RR, &param);
                    break;
                default:
                    printf("Invalid policy choice.\n");
                    exit(1);
            }

            perform_matrix_operation();

            gettimeofday(&end, NULL);
            long elapsed_time = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
            double elapsed_seconds = elapsed_time / 1000000.0;

            ProcessInfo pInfo;
            pInfo.pid = getpid();
            pInfo.nice_value = current_nice_value;
            struct tm *tm_info;

            tm_info = localtime(&start.tv_sec);
            strftime(pInfo.start_time, 26, "%Y-%m-%d %H:%M:%S", tm_info);
            pInfo.start_usec = start.tv_usec;

            tm_info = localtime(&end.tv_sec);
            strftime(pInfo.end_time, 26, "%Y-%m-%d %H:%M:%S", tm_info);
            pInfo.end_usec = end.tv_usec;

            pInfo.elapsed_seconds = elapsed_seconds;

            if(write(pipefd[1], &pInfo, sizeof(ProcessInfo)) == -1) {
                perror("Write to pipe failed");
                exit(EXIT_FAILURE);
            }

            close(pipefd[1]);
            exit(0);
        }
        else if(pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
        else {
            child_pids[i] = pid;
        }
    }

    close(pipefd[1]);

    ProcessInfo pInfoArray[NUM_PROCESSES];

    for(int i = 0; i < NUM_PROCESSES; i++) {
        if(read(pipefd[0], &pInfoArray[i], sizeof(ProcessInfo)) == -1) {
            perror("Read from pipe failed");
            exit(EXIT_FAILURE);
        }
        wait(NULL); // Wait for all child processes to finish
    }

    close(pipefd[0]);

    if(policy == 2) { // CFS_NICE
        qsort(pInfoArray, NUM_PROCESSES, sizeof(ProcessInfo), compare_nice);
    }

    for(int i = 0; i < NUM_PROCESSES; i++) {
    printf("PID: %d", pInfoArray[i].pid);
    // Print Nice value only for CFS_DEFAULT and CFS_NICE
    if (policy == 1 || policy == 2) {
        printf(" | Nice: %d", pInfoArray[i].nice_value);
    }
    printf(" | Start time: %s.%06ld | End time: %s.%06ld | Elapsed time: %.6f\n",
        pInfoArray[i].start_time,
        pInfoArray[i].start_usec,
        pInfoArray[i].end_time,
        pInfoArray[i].end_usec,
        pInfoArray[i].elapsed_seconds);

    total_elapsed_time += pInfoArray[i].elapsed_seconds;
}

    printf("Scheduling Policy: ");
    switch(policy) {
        case 1:
            printf("CFS_DEFAULT");
            break;
        case 2:
            printf("CFS_NICE");
            break;
        case 3:
            printf("RT_FIFO");
            break;
        case 4:
            printf("RT_RR");
            // Print the chosen time quantum for RT_RR
            if (time_quantum > 0) {
                printf(" | Time Quantum: %d ms", time_quantum);
            }
            break;
        default:
            printf("Invalid");
    }
    printf(" | Average elapsed time: %.6lf\n", total_elapsed_time / NUM_PROCESSES);

    return 0;
}

