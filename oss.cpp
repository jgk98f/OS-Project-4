#include "header.h"
#define TOTAL_SLAVES 100
#define MAXSLAVE 20
#define INCREMENTER 30000
#define ARRAY_SIZE 18
using namespace std;

char *sharedIdArg;
char *processArg;
char *processControlBlockArg;
char *timeArg;
volatile sig_atomic_t cleanIndicator = 0;
pid_t myPid, childPid;
int t = 20;
int numberSlaves = 3;
int status = 0;
int shmid = 0;
int pcbShmid = 0;
int sId = 0;
int mId = 0;
int nextProcess = 1;
int processNumberCreated = -1;
long long spawnTime = 0;
int messageReceived = 0;
struct sharedStruct *sharedData;
FILE *file;
struct msqid_ds msqid_ds_buf;
key_t timerKey = 15000;
key_t pcbKey = 15005;
key_t ossKey = 15010;


int main (int argc, char **argv)
{
  srand(time(0));
  sharedIdArg = (char*) malloc(25);
  processArg = (char*) malloc(25);
  processControlBlockArg = (char*) malloc(25);
  timeArg = (char*) malloc(25);
  int index;
  char *filename = "logfile.txt";
  char *defaultFileName = "logfile.txt";
  int c,i;

  while ((c = getopt(argc,argv,":hs:l:t:")) != -1)
    switch (c) {
      case 'h':
        printHelpMessage();
        break;
      case 's':
        numberSlaves = atoi(optarg);
        if(numberSlaves > MAXSLAVE) {
           numberSlaves = 20;
          fprintf(stderr, "Reverting to 20 due to greater than 20 processes entered.\n");
        }
        break;
      case 'l':
        filename = optarg;
        break;
      case 't':
        t = atoi(optarg);
        break;
      default:
          printHelpMessage();
          return 0;
        
      }

  signal(SIGALRM, signalHandler);
  signal(SIGINT, signalHandler);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  //set alarm.
  alarm(t);

  int sizeArray = sizeof(*pcb) * 18;

  if((shmid = shmget(timerKey, sizeof(sharedStruct), IPC_CREAT | 0777)) == -1) {
    perror("OSS SHMID TIMER");
    exit(-1);
  }

  if((sharedData = (struct sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("OSS SHMAT SHAREDDATA");
    exit(-1);
  }
  
  if((pcbShmid = shmget(pcbKey, sizeArray, IPC_CREAT | 0777)) == -1) {
    perror("OSS SHMGET PCB");
    exit(-1);
  }

  if((pcb = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
    perror("OSS SHMAT PCB");
    exit(-1);
  }

  if((mId = msgget(ossKey, IPC_CREAT | 0777)) == -1) {
    perror("OSS MSGGET OSSKEY");
    exit(-1);
  }

  for (i = 0; i < ARRAY_SIZE; i++) {
    pcb[i].processID = 0;
    pcb[i].priority = 0;
    pcb[i].totalScheduledTime = 0;
    pcb[i].lastBurst = 0;
    pcb[i].totalTimeRan = 0;
  }

  file = fopen(filename, "w");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }

  sharedData->ossTimer = 0;
  sharedData->scheduledProcess = -1;
  sharedData->sigNotReceived = 1;

  createQueues();

  do {

    if(isTime()) {
      createProcesses();
      setTime();
    }

    sharedData->scheduledProcess = scheduleProcess();

    sharedData->ossTimer += incrementTimer();

    updateProcess(waitTurn());
  
  } while (sharedData->ossTimer < 700000 && sharedData->sigNotReceived);

  if(!cleanIndicator) {
    cleanIndicator = 1;
    printf("OSS regular cleanup called!\n");
    cleanEnv();
  }
  return 0;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to determine time to term.
 **/
bool isTime(void) {
  printf("Time allotted = %llu OssTimer =  %llu\n", spawnTime, sharedData->ossTimer);
  fprintf(file, "Time allotted = %llu OssTimer =  %llu\n", spawnTime, sharedData->ossTimer);
 
  if(sharedData->ossTimer >= spawnTime)
    return true;
  else
    return false;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to set process time.
 **/
void setTime(void) {
  spawnTime = sharedData->ossTimer + rand() % 20000;
  printf("OSS trying to create process at time %llu\n", spawnTime);
  fprintf(file, "OSS trying to create process at time %llu\n", spawnTime);
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to create processes
 **/
void createProcesses(void) {

    int i;
    processNumberCreated = -1;
    
    
    for(i = 0; i < ARRAY_SIZE; i++) {
      if(pcb[i].processID == 0) {
        processNumberCreated = i;
        pcb[i].processID = 1;
        break;
      } 
    }

    if(processNumberCreated == -1) {
      printf("PCB full. Process not created!\n");
      fprintf(file, "PCB full. Process not created!\n");
    }

    if(processNumberCreated != -1) {
      printf("About to create process #%d\n", processNumberCreated);
      fprintf(file, "About to create process #%d\n", processNumberCreated);
      
      if((childPid = fork()) < 0) {
        perror("Fork Error");
      }

      if(childPid == 0) {
        pcb[processNumberCreated].priority = getPriority();
        pcb[processNumberCreated].totalScheduledTime = scheduleProcess();
        pcb[processNumberCreated].processID = getpid();
        
        printf("Process %d at pcb location %d was scheduled for time %llu\n", getpid(), processNumberCreated, pcb[processNumberCreated].totalScheduledTime);
        fprintf(file, "Process %d at pcb location %d was scheduled for time %llu\n", getpid(), processNumberCreated, pcb[processNumberCreated].totalScheduledTime);
        sprintf(sharedIdArg, "%d", shmid);
        sprintf(processArg, "%d", processNumberCreated);
        sprintf(processControlBlockArg, "%d", pcbShmid);
        sprintf(timeArg, "%d", t);
        char *opts[] = {"./user", "-m", sharedIdArg, "-n", processArg, "-p", processControlBlockArg, "-t", timeArg, NULL};
        printf("Called exec with -m %s -n %s -p %s -t %s!\n", sharedIdArg, processArg, processControlBlockArg, timeArg);
	execv("./user", opts);
      }
      
    }
    if(processNumberCreated != -1) {
      while(pcb[processNumberCreated].processID <= 1); 
      enqueue(pcb[processNumberCreated].processID, Q1);
    }

}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to increment timer random amount.
 **/
int incrementTimer(void) {
  return rand() % 1001;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to schedule process time.
 **/
int scheduleProcess(void) {
  return rand() % 70001;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to get priority.
 **/
long long getPriority(void) {
  int random = rand() % 20;
  return random == 1 ? queuePriorityHigh : queuePriorityNormal_1;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to wait for our turn to be scheduled and display some info for logging.
 **/
int waitTurn(void) {
  struct msgformat msg;

  while(sharedData->scheduledProcess != -1);

  if(msgrcv(mId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("OSS MSGRCV");
      return -1;
    }
    printf("No message for OSS!\n");
    return -1;
  }
  else {
    int processNum = atoi(msg.mText);
    printf("Message from pid %d with process number %d.\n", pcb[processNum].processID, processNum);
    fprintf(file, "Message from pid %d with process number %d.\n", pcb[processNum].processID, processNum);
    fprintf(file, "User pid %d with process number %d got assigned a duration %llu out of %llu\n", pcb[processNum].processID, processNum, pcb[processNum].lastBurst, pcb[processNum].priority);
    fprintf(file, "User pid %d with process number %d has ran for %llu out of %llu\n", pcb[processNum].processID, processNum, pcb[processNum].totalTimeRan, pcb[processNum].totalScheduledTime);
    return processNum;
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to handle priority scheduling and calling the respective enqueue.
 **/
void updateProcess(int processLocation) {

  if(processLocation == -1) {
    return;
  }

  pid_t id = pcb[processLocation].processID;
  long long Burst = pcb[processLocation].lastBurst;
  long long priority = pcb[processLocation].priority;

  if(id != 0) {
    if(priority == queuePriorityHigh) {
      enqueue(id, Q0);
    }
    else if(Burst < priority) {
      pcb[processLocation].priority = queuePriorityNormal_1;
      enqueue(id, Q1);
    }
    else {
      if(priority == queuePriorityNormal_1) {
        pcb[processLocation].priority = queuePriorityNormal_2;
        enqueue(id, Q2);
      }
      else if(priority == queuePriorityNormal_2) {
        pcb[processLocation].priority = queuePriorityNormal_3;
        enqueue(id, Q3);
      }
      else if(priority == queuePriorityNormal_3) {
        pcb[processLocation].priority = queuePriorityNormal_3;
        enqueue(id, Q3);
      }
      else {
        printf("Priority Error!\n");
      
      }

    }
 
  }
  else {
    printf("Process completed its time.\n");
    fprintf(file, "Process completed its time.\n");
    pcb[processLocation].totalScheduledTime = 0;
    pcb[processLocation].lastBurst = 0;
    pcb[processLocation].totalTimeRan = 0;
  }

}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to schedule the next process.
 **/
pid_t scheduleNextProcess(void) {
  if(!isEmpty(Q0)) {
    return pop(Q0);
  }
  else if(!isEmpty(Q1)) {
    return pop(Q1);
  }
  else if(!isEmpty(Q2)) {
    return pop(Q2);
  }
  else if(!isEmpty(Q3)) {
    return pop(Q3);
  }
  else return -1;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to send a message.
 **/
void sendMessage(int queueId, int msgType) {
  struct msgformat msg;

  msg.mType = msgType;
  sprintf(msg.mText, "OSS Starting User Queue.\n");

  if(msgsnd(queueId, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("OSS MSGSND");
  }

}


/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Check is Queue level provided is empty.
 **/
void createQueues() {
  startQueue0 = startQueue1 = startQueue2 = startQueue3 = NULL;
  endQueue0 = endQueue1 = endQueue2 = endQueue3 = NULL;
  Q0SZ = Q1SZ = Q2SZ = Q3SZ = 0;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Check is Queue level provided is empty.
 **/
bool isEmpty(int level) {
 
    if(level == 0){
      if((startQueue0 == NULL) && (endQueue0 == NULL))
        return true;
    }
    else if(level == 1){
      if((startQueue1 == NULL) && (endQueue1 == NULL))
        return true;
    }
    else if(level == 2){
      if((startQueue2 == NULL) && (endQueue2 == NULL))
        return true;
    }
    else if(level == 3){
      if((startQueue3 == NULL) && (endQueue3 == NULL))
        return true;
    }
    else{
      printf("Invalid Queue\n");
    }
  return false;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to enqueue into provided queue level.
 **/
void enqueue(pid_t pId, int level) {
  printf("Enqueuing pid %d in queue level %d\n", pId, level);
  fprintf(file, "Enqueue pid %d in queue level %d\n", pId, level);
  
    if(level == 0){
      if(endQueue0 == NULL) {
        endQueue0 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue0->next = NULL;
        endQueue0->id = pId;
        startQueue0 = endQueue0;
      }
      else {
        tempQueue0 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue0->next = tempQueue0;
        tempQueue0->id = pId;
        tempQueue0->next = NULL;

        endQueue0 = tempQueue0;
      }
      Q0SZ++;
    }
    else if(level == 1){
      if(endQueue1 == NULL) {
        endQueue1 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue1->next = NULL;
        endQueue1->id = pId;
        startQueue1 = endQueue1;
      }
      else {
        tempQueue1 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue1->next = tempQueue1;
        tempQueue1->id = pId;
        tempQueue1->next = NULL;

        endQueue1 = tempQueue1;
      }
      Q1SZ++;
    }
    else if(level == 2){
      if(endQueue2 == NULL) {
        endQueue2 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue2->next = NULL;
        endQueue2->id = pId;
        startQueue2 = endQueue2;
      }
      else {
        tempQueue2 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue2->next = tempQueue2;
        tempQueue2->id = pId;
        tempQueue2->next = NULL;
        endQueue2 = tempQueue2;
      }
      Q2SZ++;
    }
    else if(level == 3){
      if(endQueue3 == NULL) {
        endQueue3 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue3->next = NULL;
        endQueue3->id = pId;
        startQueue3 = endQueue3;
      }
      else {
        tempQueue3 = (struct queue*)malloc(1 * sizeof(struct queue));
        endQueue3->next = tempQueue3;
        tempQueue3->id = pId;
        tempQueue3->next = NULL;

        endQueue3 = tempQueue3;
      }
      Q3SZ++;
    }
    else{
      printf("Invalid Queue Level to Enqueue!\n");
    }
 
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function that provides the pop functionality for queue at level provided.
 **/
pid_t pop(int level) {
  pid_t popID;
  
    if(level == 0){
      startQueueA0 = startQueue0;
      if(startQueueA0 == NULL) {
        printf("Error: Cannot Pop an Empty Queue!\n");
      }
      else {
        if(startQueueA0->next != NULL) {
          startQueueA0 = startQueueA0->next;
          popID = startQueue0->id;
          free(startQueue0);
          startQueue0 = startQueueA0;
        }
        else {
          popID = startQueue0->id;
          free(startQueue0);
          startQueue0 = NULL;
          endQueue0 = NULL;
        }
        Q0SZ--;
      }
    }
    else if(level == 1){
      startQueueA1 = startQueue1;
      if(startQueueA1 == NULL) {
        printf("Error: Cannot Pop an Empty Queue!\n");
      }
      else {
        if(startQueueA1->next != NULL) {
          startQueueA1 = startQueueA1->next;
          popID = startQueue1->id;
          free(startQueue1);
          startQueue1 = startQueueA1;
        }
        else {
          popID = startQueue1->id;
          free(startQueue1);
          startQueue1 = NULL;
          endQueue1 = NULL;
        }
        Q1SZ--;
      }
    }
    else if(level == 2){
      startQueueA2 = startQueue2;
      if(startQueueA2 == NULL) {
        printf("Error: Cannot Pop an Empty Queue!\n");
      }
      else {
        if(startQueueA2->next != NULL) {
          startQueueA2 = startQueueA2->next;
          popID = startQueue2->id;
          free(startQueue2);
          startQueue2 = startQueueA2;
        }
        else {
          popID = startQueue2->id;
          free(startQueue2);
          startQueue2 = NULL;
          endQueue2 = NULL;
        }
        Q2SZ--;
      }
    }
    else if(level == 3){
      startQueueA3 = startQueue3;
      if(startQueueA3 == NULL) {
        printf("Error: Cannot Pop an Empty Queue!\n");
      }
      else {
        if(startQueueA3->next != NULL) {
          startQueueA3 = startQueueA3->next;
          popID = startQueue3->id;
          free(startQueue3);
          startQueue3 = startQueueA3;
        }
        else {
          popID = startQueue3->id;
          free(startQueue3);
          startQueue3 = NULL;
          endQueue3 = NULL;
        }
        Q3SZ--;
      }
    }
    else{
      printf("Invalid Queue Level!\n");
    }
  
  printf("Got process id %d from queue level %d\n", popID, level);
  fprintf(file, "Got process id %d from queue level %d\n", popID, level);
  return popID;
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to handle signals and cleanEnv if applicable.
 **/
void signalHandler(int signum){
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  if(signum == SIGINT) {
    fprintf(stderr, "CTRL-C received. Signaling to cleanEnvironment!\n");
  }

  if(signum == SIGALRM) {
    fprintf(stderr, "OSS has timed out.\n");
  }

  if(!cleanIndicator) {
    cleanIndicator = 1;
    printf("OSS cleanEnv from a signal!\n");
    cleanEnv();
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to clean the runtime environment.
 **/
void cleanEnv() {
  signal(SIGQUIT, SIG_IGN);
  sharedData->sigNotReceived = 0;

  printf("OSS encountered a SIGQUIT!\n");
  kill(-getpgrp(), SIGQUIT);

  //free up the malloc'd memory for the arguments
  free(sharedIdArg);
  free(processArg);
  free(processControlBlockArg);
  free(timeArg);
  
  printf("OSS waiting for processes to be die gracefully.\n");
  childPid = wait(&status);

  printf("OSS about to free all shared memory\n");
  //Detach and remove the shared memory after all child process have died
  if(freeTimer(shmid, sharedData) == -1) {
    perror("Failed to free timer shared memory");
  }

  if(freepcb(pcbShmid, pcb) == -1) {
    perror("Failed to free PCB shared memory");
  }

  clearQueueLevels();

  printf("OSS about to clear message queues and remove them.\n");
  //Delete the message queues
  msgctl(sId, IPC_RMID, NULL);
  msgctl(mId, IPC_RMID, NULL);

  if(fclose(file)) {
    perror("Couldn't close file");
  }


  printf("OSS killing itself\n");
  //Kill this master process
  kill(getpid(), SIGKILL);
}


/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to clear all queue levels.
 **/
void clearQueueLevels(void) {
  while(!isEmpty(Q0)) {
    pop(Q0);
  }
  while(!isEmpty(Q1)) {
    pop(Q1);
  }
  while(!isEmpty(Q2)) {
    pop(Q2);
  }
  while(!isEmpty(Q3)) {
    pop(Q3);
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to free the timer shared memory.
 **/
int freeTimer(int shmid, sharedStruct *shmaddr) {
  printf("OSS: Removing Timer Shared Memory!\n");
  bool indicator = 0;
  if(shmdt(shmaddr) == -1) {
    perror("TIMER SHMDT");
    indicator = true;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1)) {
    perror("TIMER SHMCTL");
    indicator = true;
  }
  if(indicator) {
       return -1;
  }
  else{
       return 0;
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Function to free the PCB array.
 **/
int freepcb(int shmid, PCB *shmaddr) {
  printf("OSS Removing PCB Shared Memory!\n");
  bool indicator = false;
  if(shmdt(shmaddr) == -1) {
    perror("PCB SHMDT");
    indicator = true;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1)) {
    perror("PCB SHMCTL");
    indicator = true;
  }
  if(indicator) {
       return -1;
  }
  else{
       return 0;
  }
}

/**
 * Author: Jason Klamert
 * Date: 11/4/2016
 * Description: Help Message
 **/
void printHelpMessage(void) {
  printf("[-h], [-help], [-l][logfileName], [-s][#slaves], [-t][timeTillMasterDeath]\n\n");
}
