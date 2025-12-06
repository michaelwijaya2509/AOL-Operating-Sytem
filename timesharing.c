#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PROCS 10 //max processes constant
#define TIME_SLICE 1  //time slice or quantum in second

typedef enum{ NEW, RUNNING, STOPPED, TERMINATED} State;

typedef struct{
    pid_t pid;
    State state;
}PCB;

PCB processes[MAX_PROCS]; //instantiate processes sebagai PCB sebanyak max_procs

int nprocs =0;
int curr = -1;

//function yang diexecute oleh child process
void run_child(int id){
    while(1){ //infinite loop
        printf("Ini proses %d dengan pid %d", id, getpid());
        fflush(stdout); //mengeluarkan hasil printf langsung ke layar tanpa lewat buffer

        //busy wait, semacam sleep, tetapi CPU tetap bekerja, jadi bisa simulasi proses preemptive
        for(volatile long i = 0; i < 100000000; i++);
    }
}

int find_next_process(){
    if (nprocs == 0) return -1; //proses tidak ada
    int start = curr; //dari -1
    for(int i = 1; i < nprocs; i++){
        int idx = (start + i) % nprocs; //modulo agar circular (bergantian)
        if(processes[idx].state != TERMINATED){
            return idx; //index proses yang dicari
        }
    }
    return -1;
}

void timer(int sig){ //jika function ini dipanggil proses yang lagi running(di sini curr dinamakan jadi prev) akan distop, 
// kemudian akan menjalankan proses berikutnya hasil panggil function find_next_process
    int prev = curr;
    int next = find_next_process(); //cari proses selanjutnya pakai function yang sudah dibuat
    
    if(next == -1){
        printf("Proses Tidak Diteukan!\n");
        exit(0);
    }

    if(prev != -1 && processes[prev].state != TERMINATED){
        kill(processes[prev].pid, SIGSTOP);
        processes[prev].state = STOPPED; //update state proses jadi stopped
        printf("Proses pid %d, stopped\n", processes[prev].pid);
    }

    curr = next; //posisi current dipindahkan ke proses berikutnya
    if(processes[curr].state == NEW || processes[curr].state == STOPPED){
        kill(processes[curr].pid, SIGCONT); //lanjutkan/jalankan proses baru
        processes[curr].state = RUNNING;
        printf("Proses Pid %d, sedang dijalankan", processes[curr].pid);
    }

}

int main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <num_processes", argv[0]); //kirim pesan error karena argumen tidak tepat
        exit(1);
    }

    nprocs = atoi(argv[1]); //mengubah input argumen menjadi integer (./timeshare 4) 4 diubah jadi integer sebagai jumlah process
    if(nprocs <= 0 || nprocs > MAX_PROCS){
        fprintf(stderr, "Proses harus berjumlah antara 1 sampai %d\n", MAX_PROCS);
        exit(1);
    }

     for (int i = 0; i < nprocs; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } //forking sebanyak jumlah proses yang diinput, misal kalau 4 maka jadi 8

        if(pid == 0){ //pid 0 berarti lagi di child process
            kill(getpid(), SIGSTOP); //buat state child jadi stopped dulu, parent yang akan atur alur proses
            run_child(i);
            exit(0);
        }
        else {
            processes[i].pid = pid;
            processes[i].pid = NEW;
            printf("Created child %d with pid=%d\n", i, pid);
        }
    }
      // Pasang signal handler untuk SIGALRM
    struct sigaction sa;
    sa.sa_handler = timer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Set timer: first tick after TIME_SLICE, lalu interval sama
    struct itimerval timer;
    timer.it_value.tv_sec = TIME_SLICE;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = TIME_SLICE;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        exit(1);
    }

    printf("Starting scheduler...\n");

    // Mulai dari proses pertama
    curr = 0;
    kill(processes[curr].pid, SIGCONT);
    processes[curr].state = RUNNING;

    // Parent hanya menunggu sinyal dan child exit
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            // Tandai proses sebagai TERMINATED
            for (int i = 0; i < nprocs; i++) {
                if (processes[i].pid == pid) {
                    processes[i].state = TERMINATED;
                    printf("Process pid=%d terminated\n", pid);
                    break;
                }
            }
        }

        pause();  // tunggu sinyal (SIGALRM)
    }

    return 0;
}

