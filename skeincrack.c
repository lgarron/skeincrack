#include "SHA3api_ref.h"
#include "skein.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <pthread.h>

#define BUF_SIZE 128
#define HASH_BITS 1024

static const char *param = "hashable=";

static const unsigned char target[HASH_BITS/8] = {0x5b, 0x4d, 0xa9, 0x5f, 0x5f, 0xa0, 0x82, 0x80, 0xfc, 0x98, 0x79, 0xdf, 0x44, 0xf4, 0x18, 0xc8, 0xf9, 0xf1, 0x2b, 0xa4, 0x24, 0xb7, 0x75, 0x7d, 0xe0, 0x2b, 0xbd, 0xfb, 0xae, 0x0d, 0x4c, 0x4f, 0xdf, 0x93, 0x17, 0xc8, 0x0c, 0xc5, 0xfe, 0x04, 0xc6, 0x42, 0x90, 0x73, 0x46, 0x6c, 0xf2, 0x97, 0x06, 0xb8, 0xc2, 0x59, 0x99, 0xdd, 0xd2, 0xf6, 0x54, 0x0d, 0x44, 0x75, 0xcc, 0x97, 0x7b, 0x87, 0xf4, 0x75, 0x7b, 0xe0, 0x23, 0xf1, 0x9b, 0x8f, 0x40, 0x35, 0xd7, 0x72, 0x28, 0x86, 0xb7, 0x88, 0x69, 0x82, 0x6d, 0xe9, 0x16, 0xa7, 0x9c, 0xf9, 0xc9, 0x4c, 0xc7, 0x9c, 0xd4, 0x34, 0x7d, 0x24, 0xb5, 0x67, 0xaa, 0x3e, 0x23, 0x90, 0xa5, 0x73, 0xa3, 0x73, 0xa4, 0x8a, 0x5e, 0x67, 0x66, 0x40, 0xc7, 0x9c, 0xc7, 0x01, 0x97, 0xe1, 0xc5, 0xe7, 0xf9, 0x02, 0xfb, 0x53, 0xca, 0x18, 0x58, 0xb6};

static inline int hammingDistance(const unsigned char *hash1, 
                                  const unsigned char *hash2,
                                  size_t len) {
  int total = 0;
  while (len --> 0)
    total += __builtin_popcount(*hash1++ ^ *hash2++);
  return total;
}

void submitData() {
  pid_t pid = fork();
  if (pid) {
    // parent
    int status = -1;
    waitpid(pid, &status, 0);
    if (status != 0)
      perror("Child error");
  } else {
    // child
    char *args[] = {"curl", "-X", "POST", "--data", "@data.txt", "http://almamater.xkcd.com/?edu=stanford.edu", "--header", "Content-Type:application/octet-stream", NULL};
    execvp("curl", args);
    exit(-1);
  }
}

void normalizeBuffer(unsigned char *buf, int size) {
  for (int i = 0; i < size; i++) {
    buf[i] = buf[i] % 62;
    if(buf[i] < 26) {
      buf[i] = buf[i] + 'a';
    } else if(buf[i] < 52) {
      buf[i] = buf[i] - 26 + 'A';
    } else {
      buf[i] = buf[i] - 52 + '0';
    }
  }
}

// We arbitrarily choose to sequence numbers 0-9, a-z, A-Z
void incrementArbitraryPrecision(unsigned char *data, size_t len) {
  for (int i = len - 1; i >= 0; i--) {
    if (data[i] == '9') {
      data[i] = 'a';
      break;
    } else if (data[i] == 'z') {
      data[i] = 'A';
      break;
    } else if (data[i] != 'Z') {
      data[i]++;
      break;
    } else {
      data[i] = '0';
    }
  }
}

int best = HASH_BITS;
pthread_mutex_t bestLock;

void *hashThread(void *aux) {
  int threadNum = *(int*) aux;
  printf("Thread %d started\n", threadNum);

  int threadBest = HASH_BITS;

  int randfd = open("/dev/urandom", O_RDONLY);
  if(randfd < 0)
    perror("can't open random file");

  unsigned char buf[BUF_SIZE + 1];
  buf[BUF_SIZE] = 0;
  read(randfd, buf, BUF_SIZE);
  normalizeBuffer(buf, BUF_SIZE);

  while(true) {
    incrementArbitraryPrecision(buf, BUF_SIZE);
    unsigned char hash[HASH_BITS / CHAR_BIT];
    Hash(HASH_BITS, buf, BUF_SIZE * CHAR_BIT, hash);

    int distance = hammingDistance(target, hash, HASH_BITS/8);
    if (distance < threadBest) {
      threadBest = distance;

      // check if this is actually the best over all threads
      pthread_mutex_lock(&bestLock);
      if(threadBest >= best) {
        threadBest = best;
        pthread_mutex_unlock(&bestLock);
        continue;
      }
      // If here, this thread actually has the best hash

      printf("\n\nDistance: %d\n", distance);
      best = distance;

      printf("Data: %s\n", buf);

      int outfd = open("data.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if(outfd < 0)
        perror("can't open output file");

      write(outfd, param, strlen(param));
      write(outfd, buf, BUF_SIZE);
      fsync(outfd);
      close(outfd);

      submitData();

      pthread_mutex_unlock(&bestLock);
    }
  }
  return NULL; 
}


int main(void) {
  pthread_mutex_init(&bestLock, NULL);

  long numCPUs = sysconf(_SC_NPROCESSORS_ONLN); // Get number of cores

  pthread_t *threads = malloc(sizeof(pthread_t) * numCPUs);
  int *threadNums = malloc(sizeof(int) * numCPUs);

  for(int i = 0; i < numCPUs; i++) {
    threadNums[i] = i;
    int err = pthread_create(threads + i, NULL, hashThread, threadNums + i);
    if(err < 0)
      perror("Child thread");
  }

  // Join threads. Since they never terminate, this will just block, which is fine
  for(int i = 0; i < numCPUs; i++) {
    int err = pthread_join(threads[i], NULL);
    if(err < 0)
      perror("Child join");
  }
  return 0;
}