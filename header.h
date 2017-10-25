#ifndef HEADER_H
#define HEADER_H
#define Q0 0
#define Q1 1
#define Q2 2
#define Q3 3
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>

typedef struct sharedStruct {
  long long ossTimer;
  int sigNotReceived;
  pid_t scheduledProcess;
} sharedStruct;

typedef struct msgformat {
  long mType;
  char mText[80];
} msgformat;

typedef struct PCB {
  pid_t processID;
  long long totalScheduledTime;
  long long totalTimeRan;
  long long lastBurst;
  long long priority;
} PCB;

void createProcesses(void);
bool isTime(void);
void setTime(void);
int incrementTimer(void);
long long getPriority(void);
int scheduleProcess(void);
pid_t scheduleNextProcess(void);
int waitTurn(void);
void updateProcess(int);
void signalHandler(int);
void cleanEnv(void);
void sendMessage(int, int);
int freeTimer(int, sharedStruct*);
int freepcb(int, PCB*);
void printHelpMessage(void);
void createQueues(void);
bool isEmpty(int);
void enqueue(pid_t, int);
pid_t pop(int);
void clearQueueLevels(void);
void killChild(int);
int isIO();
long long getTimeQuantum();
void quitOverride(int signum);

struct queue {
  pid_t id;
  struct queue *next;

} *startQueue0, *startQueue1, *startQueue2, *startQueue3,
  *endQueue0, *endQueue1, *endQueue2, *endQueue3,
  *tempQueue0, *tempQueue1, *tempQueue2, *tempQueue3,
  *startQueueA0, *startQueueA1, *startQueueA2, *startQueueA3;

int Q0SZ;
int Q1SZ;
int Q2SZ;
int Q3SZ;
const long long queuePriorityHigh = 4000;
const long long queuePriorityNormal_1 = 3000;
const long long queuePriorityNormal_2 = 6000;
const long long queuePriorityNormal_3 = 12000;
volatile sig_atomic_t sigNotReceived = 1;
long long *ossTimer;
struct sharedStruct *myStruct;
int masterQueueId;
const int QUIT_TIMEOUT = 10;
struct msqid_ds msqid_buf;
struct PCB *pcb;
static const long long NANO_MODIFIER = 1000000000;

#endif
