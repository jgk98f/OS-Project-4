#include "header.h"
#define TOTAL_SLAVES 100
#define MAXSLAVE 20
#define INCREMENTER 40000
#define ARRAY_SIZE 18
using namespace std;

char *shmIdArg;
char *processArg;
char *processControlBlockArg;
char *timeArg;
volatile sig_atomic_t cleanEnvIndicator = 0;
pid_t user;
int t = 20;
int numberSlaves = 3;
int status;
int shmid = 0;
int pcbId = 0;
int sId;
int mId;
int nextProcess = 1;
int processNumber = -1;
long long spawnTime = 0;
int messageReceived = 0;
struct sharedStruct *sharedData;
FILE *file;
struct msqid_ds msqid_ds_buf;

int main (int argc, char **argv) {
  int timeout = 30;
  long long startTime;
  long long currentTime;
  key_t mKey = 15010;
  key_t sKey = 15020;
  char *fileName;
  char* standard = "logfile.txt";
  FILE *file;
  int c;
  user = getpid();

  while ((c = getopt(argc,argv,":l:t:m:n:p:")) != -1)
    switch (c) {
      case 'l':
        fileName = optarg;
        break;
      case 'm':
        shmid = atoi(optarg);
        break;
      case 'n':
        processNumber = atoi(optarg);
        break;
      case 'p':
        pcbId = atoi(optarg);
        break;
      case 't':
        timeout = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Invalid arguments have been passed to %d.\n", user);
        exit(-1);
    }

  srand(time(0) + processNumber);

  if((sharedData = (sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("USER SHMAT SHAREDDATA");
    exit(1);
  }

  if((pcb = (PCB *)shmat(pcbId, NULL, 0)) == (void *) -1) {
    perror("USER SHMAT pcb");
    exit(1);
  }

  //Set up the signal handling, overriding, and alarm.
  signal(SIGINT, quitOverride);
  signal(SIGQUIT, quitOverride);
  signal(SIGALRM, killChild);
  alarm(QUIT_TIMEOUT);
  alarm(timeout);

  if((mId = msgget(mKey, IPC_CREAT | 0777)) == -1) {
    perror("USER MSGGET");
    exit(-1);
  }

  int i = 0;
  int j = 0;
  long long duration = 0;
  int finished = 1;

  do {
  
    //Check for IO blocking.
    if(isIO() == 1) {
      duration = getTimeQuantum(); 
    }
    else {
      duration = pcb[processNumber].priority;
    }
    
    //Do some pcb processing.
    printf("USER with pid %d and process number %d got duration %llu out of %llu!\n", user, processNumber, duration, pcb[processNumber].priority);
    pcb[processNumber].lastBurst = duration;
    pcb[processNumber].totalTimeRan += duration;

    //Send msg to OSS Queue.
    sendMessage(mId, 3);

    if(pcb[processNumber].totalTimeRan >= pcb[processNumber].totalScheduledTime) {
      duration -= (pcb[processNumber].totalTimeRan - pcb[processNumber].totalScheduledTime);
      pcb[processNumber].totalTimeRan = pcb[processNumber].totalScheduledTime;
      finished = 0; 
      pcb[processNumber].processID = 0;
    }

    //Update the sharedData.
    sharedData->ossTimer += duration;
    sharedData->scheduledProcess = -1;

    printf("USER with pid %d and processNumber %d has ran for %llu out of %llu\n", user, processNumber, pcb[processNumber].totalTimeRan, pcb[processNumber].totalScheduledTime);
  
  } while (finished && sharedData->sigNotReceived);

  //Remove shared memory.
  if(shmdt(sharedData) == -1) {
    perror("USER SHMDT SHAREDDATA");
  }

  if(shmdt(pcb) == -1) {
    perror("USER SHMDT PCB");
  }

  printf("User %d exiting!\n", processNumber);

  //Sleep one for graceful kill if possible.
  kill(user, SIGTERM);
  sleep(1);
  kill(user, SIGKILL);
}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to determine if process will block for IO.
 **/
int isIO() {
  return 1 + rand() % 5;
}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to get the time quantum.
 **/
long long getTimeQuantum() {
  return rand() % pcb[processNumber].priority + 1;
}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to send message.
 **/
void sendMessage(int queueId, int messageType) {
  struct msgformat msg;

  msg.mType = messageType;
  sprintf(msg.mText, "%d", processNumber);

  if(msgsnd(queueId, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("USER MSGSND");
  }

}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to recieve message.
 **/
void getMessage(int queueId, int messageType) {
  struct msgformat msg;

  if(msgrcv(queueId, (void *) &msg, sizeof(msg.mText), messageType, MSG_NOERROR) == -1) {
    if(errno != ENOMSG) {
      perror("USER MSGRCV");
    }
    printf("USER: No message for msgrcv!\n");
  }
  else {
    printf("Message received by user %d: %s", processNumber, msg.mText);
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to handle SIGQUIT in safe manner.
 **/
void quitOverride(int signum) {
  printf("User %d has received signal %s %d!\n", processNumber, strsignal(signum), signum);
  sigNotReceived = 0;

  if(shmdt(sharedData) == -1) {
    perror("USER SHMDT SHARED");
  }

  kill(user, SIGKILL);
  //Give user 3s to gracefully die.
  alarm(3);
}

/**
 * Author: Jason Klamert
 * Date: 11/5/2016
 * Description: Function to kill off the child in case of timeout occurence.
 **/
void  killChild(int signum) {
  printf("User process %d is dieing from user timeout.\n", processNumber);
  kill(user, SIGTERM);
  sleep(1);
  kill(user, SIGKILL);
}
