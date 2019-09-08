#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     100
#define NUM_PEOPLE         20
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum Endianness {UNKNOWN = 2, LITTLE = 0, BIG = 1};
const static enum Endianness oppositeEnd [] = {BIG, LITTLE};


struct Well {
  // TODO
  int t;
  int wb;
  int wl;
  int p;
  enum Endianness e;
  uthread_mutex_t mx;
  uthread_cond_t bE;
  uthread_cond_t lE;
};

struct Well* createWell() {
  struct Well* Well = malloc (sizeof (struct Well));
  // TODO
  Well->t = 0;
  Well->wb = 0;
  Well->wl = 0;
  Well->p = 0;
  Well->e = UNKNOWN;
  Well->mx = uthread_mutex_create();
  Well->bE = uthread_cond_create(Well->mx);
  Well->lE = uthread_cond_create(Well->mx);

  return Well;
}

struct Well* Well;


#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];

void recordWaitingTime (int waitingTime) {
  uthread_mutex_lock (waitingHistogrammutex);
  if (waitingTime < WAITING_HISTOGRAM_SIZE)
    waitingHistogram [waitingTime] ++;
  else
    waitingHistogramOverflow ++;
  uthread_mutex_unlock (waitingHistogrammutex);
}

void enterWell (enum Endianness g) {
  uthread_mutex_lock(Well->mx);

  int startWaiting = entryTicker;

  while(Well->e != UNKNOWN && (Well->e != g || Well->p == MAX_OCCUPANCY)){
    if(g == BIG){
      Well->wb++;
      uthread_cond_wait(Well->bE);
    }
    else{
      Well->wl++;
      uthread_cond_wait(Well->lE);
    }
  }

  int startingTime = entryTicker;

  recordWaitingTime(startingTime-startWaiting);
  
  if (Well->e == UNKNOWN)
    Well->e = g;

  Well->p++;
  Well->t++;

  uthread_mutex_lock (waitingHistogrammutex);
  entryTicker++;
  if (Well->p == 1){
    if (Well->e == LITTLE)
      occupancyHistogram[LITTLE][1]++;
    else
      occupancyHistogram[BIG][1]++;
  }
  else if (Well->p == 2){
    if (Well->e == LITTLE)
      occupancyHistogram[LITTLE][2]++;
    else
      occupancyHistogram[BIG][2]++;
  }
  else{
    if (Well->e == LITTLE)
      occupancyHistogram[LITTLE][3]++;
    else
      occupancyHistogram[BIG][3]++;
  }
  uthread_mutex_unlock (waitingHistogrammutex);

  uthread_mutex_unlock(Well->mx);
}

void leaveWell() {
  uthread_mutex_lock(Well->mx);

  Well->p--;

  int count;
  if (Well->e == LITTLE){
    count = Well->wb;
  }else
    count = Well->wl;

  if (Well->t >= FAIR_WAITING_COUNT &&  count > 0){
    if(Well->p == 0){
      Well->e = oppositeEnd[Well->e];
      Well->t = 0;
      for(int i = 0; i < MAX_OCCUPANCY; i++){
        if(Well->e == BIG)
          uthread_cond_signal(Well->bE);
        else
          uthread_cond_signal(Well->lE);
      }
    }
  }
    
  if (Well->e == BIG)
    uthread_cond_signal(Well->bE);
  else
    uthread_cond_signal(Well->lE);

  uthread_mutex_unlock(Well->mx);
}

//
// TODO
// You will probably need to create some additional produres etc.
//
void* well(void* endv){
  int end = *(int*)endv;
  enum Endianness g;

  if (end == 0)
    g = LITTLE;
  else
    g = BIG;

  for (int i = 0; i < NUM_ITERATIONS; i++){

    enterWell(g);

    for (int i = 0; i < NUM_PEOPLE; i++)
      uthread_yield();

    
    leaveWell();

    for (int i = 0; i < NUM_PEOPLE; i++){
      uthread_yield();
    }
  }
  
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (1);
  Well = createWell();
  uthread_t pt [NUM_PEOPLE];
  waitingHistogrammutex = uthread_mutex_create ();

  int ran [NUM_PEOPLE];
  for (int i = 0; i < NUM_PEOPLE; i++){
    int r = random()%2;
    ran[i] = r;
    pt[i] = uthread_create(well, &ran[i]);
  }

  for (int i = 0; i < NUM_PEOPLE; i++)
    uthread_join(pt[i], NULL);
  
  printf ("Times with 1 little endian %d\n", occupancyHistogram [LITTLE]   [1]);
  printf ("Times with 2 little endian %d\n", occupancyHistogram [LITTLE]   [2]);
  printf ("Times with 3 little endian %d\n", occupancyHistogram [LITTLE]   [3]);
  printf ("Times with 1 big endian    %d\n", occupancyHistogram [BIG] [1]);
  printf ("Times with 2 big endian    %d\n", occupancyHistogram [BIG] [2]);
  printf ("Times with 3 big endian    %d\n", occupancyHistogram [BIG] [3]);
  printf ("Waiting Histogram\n");
  for (int i=0; i<WAITING_HISTOGRAM_SIZE; i++)
    if (waitingHistogram [i])
      printf ("  Number of times people waited for %d %s to enter: %d\n", i, i==1?"person":"people", waitingHistogram [i]);
  if (waitingHistogramOverflow)
    printf ("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
}
