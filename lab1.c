#include "lab1.h"
struct timespec start, end;
double time_taken_send = 0; 
double time_taken_receive = 0;
char *role;
int method = 0;
message_t message;
mailbox_t mailbox;
int shmid;
int semid;
FILE *fp;
key_t key = 1234;

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

void sysv_wait(int semid, int n) {///P
  struct sembuf op = { n, -1, 0 };
  semop(semid, &op, 1);
}
void sysv_signal(int semid, int n) {///V
  struct sembuf op = { n, +1, 0 };
  semop(semid, &op, 1);
}

union semun arg;

void receive(message_t *message_ptr, mailbox_t *mailbox_ptr) {
  /*  TODO:
      1. Use flag to determine the communication method
      2. According to the communication method, receive the message
  */
  arg.val = 1;
  semctl(semid, 0, SETVAL, arg);   // empty = 1
  arg.val = 0; 
  semctl(semid, 1, SETVAL, arg);   // full  = 0
  if(method == 1){//message passing
    while(1){
      sysv_wait(semid, 1);
      clock_gettime(CLOCK_MONOTONIC, &start);
      msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message.msgText), 0, 0);
      clock_gettime(CLOCK_MONOTONIC, &end);
      time_taken_receive += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
      sysv_signal(semid, 0);
      if (strcmp(message_ptr->msgText, "EOF") == 0) break;
      printf("Received: %s", message_ptr->msgText);
    }
    printf("Total time in receiving message: %fs\n", time_taken_receive);
    msgctl(mailbox_ptr->storage.msqid, IPC_RMID, NULL);
  }
  else if(method == 2){//shared memory
    while(1){
      sysv_wait(semid, 1);
      clock_gettime(CLOCK_MONOTONIC, &start);
      //
      strcpy(message_ptr->msgText, mailbox_ptr->storage.shm_addr);
      clock_gettime(CLOCK_MONOTONIC, &end);
      time_taken_receive += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
      sysv_signal(semid, 0);
      if (strcmp(message_ptr->msgText, "EOF") == 0) break;
      printf("Received: %s", message_ptr->msgText);
    }
    printf("Total time in receiving message: %fs\n", time_taken_receive);
    shmdt(mailbox_ptr->storage.shm_addr);
    shmctl(shmid, IPC_RMID, NULL);
  }
  semctl(semid, 0, IPC_RMID);
}

void send(message_t message, mailbox_t *mailbox_ptr) {
  /*  TODO:
      1. Use flag to determine the communication method
      2. According to the communication method, send the message
  */
  
  if(method == 1){//message passing
    while(fgets(message.msgText, MAX_MSG_SIZE, fp) != NULL){
      sysv_wait(semid, 0);
      clock_gettime(CLOCK_MONOTONIC, &start);
      msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0);
      clock_gettime(CLOCK_MONOTONIC, &end);
      time_taken_send += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
      sysv_signal(semid, 1);
      printf("Sent: %s", message.msgText);
    }
    sysv_wait(semid, 0);
    strcpy(message.msgText, "EOF");
    msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0);
    sysv_signal(semid, 1);
    printf("Total time in sending message: %fs\n", time_taken_send);
  }
  else if(method == 2){//shared memory
    while(fgets(message.msgText, MAX_MSG_SIZE, fp) != NULL){
      sysv_wait(semid, 0);
      clock_gettime(CLOCK_MONOTONIC, &start);
      strcpy(mailbox_ptr->storage.shm_addr, message.msgText);
      clock_gettime(CLOCK_MONOTONIC, &end);
      time_taken_send += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
      sysv_signal(semid, 1);
      printf("Sent: %s", message.msgText);
    }
    sysv_wait(semid, 0);                                            
    strcpy(message.msgText, "EOF");
    strcpy(mailbox_ptr->storage.shm_addr, message.msgText);
    sysv_signal(semid, 1);
    printf("Total time in sending message: %fs\n", time_taken_send);
    shmdt(mailbox_ptr->storage.shm_addr);
  }
}

int main(int argc, char *argv[]) {
  //Wrong input////////////////////////////////////////////
  if (argc < 3 || (strcmp(argv[1], "sender") == 0 && argc < 4)) {
    printf("Usage: %s sender <mechanism> input_file \n", argv[0]);
    printf("       %s receiver <mechanism> \n", argv[0]);
    return EXIT_FAILURE;
  } 
  ///Variable initialziation////////////////////////////////////////////
  role = argv[1];
  method = atoi(argv[2]);
  mailbox.flag = atoi(argv[2]);   // 1 = message passing, 2 = shared memory
  if(strcmp(role, "sender") == 0){
    fp = fopen(argv[3], "r");
    if (fp == NULL) { perror("fopen"); return EXIT_FAILURE; }
  }
  /////set queue id/memory ptr/////////////////////////////////////////
  semid = semget(key, 2, IPC_CREAT | 0666);
  if (semid == -1) { perror("semget"); return EXIT_FAILURE; }
  printf(method == 1 ? "Message passing\n" : "Shared memory\n");
  if(mailbox.flag == 1){// message passing
    mailbox.storage.msqid = msgget(key, IPC_CREAT | 0666);
  }
  else if(mailbox.flag == 2){//shared memory
    shmid = shmget(key, MAX_MSG_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) { perror("shmget"); return EXIT_FAILURE; }
    mailbox.storage.shm_addr = shmat(shmid, NULL, 0);
    if (mailbox.storage.shm_addr == (char *)-1) { perror("shmat"); return EXIT_FAILURE; }
  }
  //////////////////////////////////////////////////////////////////
  if(strcmp(role, "sender") == 0){
    message.mType = 1; 
    send(message, &mailbox);
  }
  else if(strcmp(role, "receiver") == 0){
    receive(&message, &mailbox);
  }
  return 0;
}
