#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

#define NUM_PROCESSES 10
#define SHM_KEY 12345
#define RUNS_PER_TQ 10

// --- 구조체 정의 ---
#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_BLOCKED 2
#define STATE_TERMINATED 3

typedef struct {
    pid_t pid;
    int id;
    int remaining_time_quantum;
    int state;
    int cpu_burst;
    int io_wait_time;
    int waiting_time;
    int turnaround_time;
    int active;
} PCB;

typedef struct {
    PCB pcbs[NUM_PROCESSES];
    int current_process_index;
    int time_quantum_setting;
    int total_processes_alive;
    int tick_count;
    int context_switches;
    int last_process_index;
} SharedData;

typedef struct {
    int total_ticks;
    int context_switches;
    double avg_turnaround_time;
    double avg_waiting_time;
} RunResult;

// --- 전역 변수 및 함수 프로토타입 ---
SharedData *shared_data;
int shm_id;

RunResult run_single_simulation(int time_quantum);
void signal_handler(int signum);
void child_process_logic(int id);
void cleanup();

// --- 실험 전체를 관리하는 main 함수 ---
int main() {
    srand(time(NULL));

    int time_quantums[] = {1, 2, 5, 12};
    int num_tqs = sizeof(time_quantums) / sizeof(time_quantums[0]);

    printf("운영체제 스케줄러 시뮬레이션을 시작합니다.\n");

    for (int tq_idx = 0; tq_idx < num_tqs; tq_idx++) {
        int current_tq = time_quantums[tq_idx];
        printf("\n타임 퀀텀(TQ) = %d 에 대한 시뮬레이션 실행 중...\n", current_tq);

        RunResult total_results = {0, 0, 0.0, 0.0};
        
        for (int i = 0; i < RUNS_PER_TQ; i++) {
            printf("  - %d / %d 번째 실행...\n", i + 1, RUNS_PER_TQ);
            RunResult result = run_single_simulation(current_tq);
            total_results.total_ticks += result.total_ticks;
            total_results.context_switches += result.context_switches;
            total_results.avg_turnaround_time += result.avg_turnaround_time;
            total_results.avg_waiting_time += result.avg_waiting_time;
        }

        double final_avg_ticks = (double)total_results.total_ticks / RUNS_PER_TQ;
        double final_avg_switches = (double)total_results.context_switches / RUNS_PER_TQ;
        double final_avg_turnaround = total_results.avg_turnaround_time / RUNS_PER_TQ;
        double final_avg_waiting = total_results.avg_waiting_time / RUNS_PER_TQ;

        char filename[100];
        sprintf(filename, "average_results_tq_%d.csv", current_tq);
        FILE *result_file = fopen(filename, "w");
        if (result_file == NULL) {
            perror("결과 파일을 생성할 수 없습니다");
            continue;
        }
        
        // UTF-8 BOM 추가
        fprintf(result_file, "\xEF\xBB\xBF");
        fprintf(result_file, "지표,평균값\n");
        fprintf(result_file, "평균 총 소요 시간 (Ticks),%.2f\n", final_avg_ticks);
        fprintf(result_file, "평균 문맥 교환 수,%.2f\n", final_avg_switches);
        fprintf(result_file, "평균 반환시간 (Ticks),%.2f\n", final_avg_turnaround);
        fprintf(result_file, "평균 대기시간 (Ticks),%.2f\n", final_avg_waiting);
        fclose(result_file);

        printf("  - TQ = %d 평균 결과가 %s 파일에 저장되었습니다.\n", current_tq, filename);
    }

    printf("\n모든 시뮬레이션이 완료되었습니다.\n");
    return 0;
}

// --- 단일 시뮬레이션을 실행하고 결과를 반환하는 함수 ---
RunResult run_single_simulation(int time_quantum) {
    shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); exit(1); }
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (SharedData *)-1) { perror("shmat"); exit(1); }

    memset(shared_data, 0, sizeof(SharedData));
    shared_data->time_quantum_setting = time_quantum;
    shared_data->current_process_index = -1;
    shared_data->total_processes_alive = NUM_PROCESSES;
    shared_data->context_switches = 0;
    shared_data->last_process_index = -1;

    for (int i = 0; i < NUM_PROCESSES; i++) {
        shared_data->pcbs[i].id = i;
        shared_data->pcbs[i].state = STATE_READY;
        shared_data->pcbs[i].remaining_time_quantum = time_quantum;
        shared_data->pcbs[i].cpu_burst = (rand() % 10) + 1;
        shared_data->pcbs[i].active = 1;
        
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) { child_process_logic(i); exit(0); }
        shared_data->pcbs[i].pid = pid;
    }

    signal(SIGALRM, signal_handler);
    
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);

    while (shared_data->total_processes_alive > 0) {
        pause();
    }

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);
    
    RunResult result;
    result.total_ticks = shared_data->tick_count;
    // 첫번째 프로세스가 실행되는 것도 switch로 카운트되므로 1을 빼줌
    result.context_switches = shared_data->context_switches > 0 ? shared_data->context_switches - 1 : 0;

    long long total_turnaround = 0, total_waiting = 0;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        total_turnaround += shared_data->pcbs[i].turnaround_time;
        total_waiting += shared_data->pcbs[i].waiting_time;
    }
    result.avg_turnaround_time = (double)total_turnaround / NUM_PROCESSES;
    result.avg_waiting_time = (double)total_waiting / NUM_PROCESSES;

    cleanup();
    return result;
}

// --- 나머지 함수들 ---
void child_process_logic(int id) {
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    while (1) {
        if (shared_data->pcbs[id].state == STATE_TERMINATED) break;
        usleep(10000);
    }
    shmdt(shared_data);
}

void signal_handler(int signum) {
    if (signum != SIGALRM) return;

    shared_data->tick_count++;

    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (shared_data->pcbs[i].active) {
            shared_data->pcbs[i].turnaround_time++;
            if (shared_data->pcbs[i].state == STATE_READY) {
                shared_data->pcbs[i].waiting_time++;
            }
        }
    }
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (shared_data->pcbs[i].state == STATE_BLOCKED) {
            shared_data->pcbs[i].io_wait_time--;
            if (shared_data->pcbs[i].io_wait_time <= 0) {
                shared_data->pcbs[i].state = STATE_READY;
                shared_data->pcbs[i].remaining_time_quantum = shared_data->time_quantum_setting;
            }
        }
    }

    int current = shared_data->current_process_index;
    
    if (current == -1 || shared_data->pcbs[current].state != STATE_RUNNING) {
         int start = (current == -1) ? 0 : (current + 1) % NUM_PROCESSES;
         for (int i = 0; i < NUM_PROCESSES; i++) {
             int idx = (start + i) % NUM_PROCESSES;
             if (shared_data->pcbs[idx].state == STATE_READY && shared_data->pcbs[idx].active) {
                 current = idx;
                 shared_data->pcbs[current].state = STATE_RUNNING;
                 
                 if (shared_data->last_process_index != current) {
                     shared_data->context_switches++;
                     shared_data->last_process_index = current;
                 }
                 break;
             }
         }
         shared_data->current_process_index = current;
    }

    if (current != -1 && shared_data->pcbs[current].state == STATE_RUNNING) {
        shared_data->pcbs[current].remaining_time_quantum--;
        shared_data->pcbs[current].cpu_burst--;
        
        if (shared_data->pcbs[current].cpu_burst <= 0) {
            if (rand() % 2 == 0) {
                shared_data->pcbs[current].state = STATE_TERMINATED;
                shared_data->pcbs[current].active = 0;
                shared_data->total_processes_alive--;
                waitpid(shared_data->pcbs[current].pid, NULL, WNOHANG);
            } else {
                shared_data->pcbs[current].state = STATE_BLOCKED;
                shared_data->pcbs[current].io_wait_time = (rand() % 5) + 1;
                shared_data->pcbs[current].cpu_burst = (rand() % 10) + 1;
            }
            shared_data->current_process_index = -1;
        }
        else if (shared_data->pcbs[current].remaining_time_quantum <= 0) {
            shared_data->pcbs[current].state = STATE_READY;
            shared_data->current_process_index = -1;
        }
    }
}

void cleanup() {
    shmdt(shared_data);
    shmctl(shm_id, IPC_RMID, NULL);
}
