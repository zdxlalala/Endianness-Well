#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
//
uthread_cond_t m_p;
uthread_cond_t t_m;
uthread_cond_t t_p;
int resource;

struct Smoker {
  int resource;
  uthread_mutex_t mx;
  uthread_cond_t smoking;
};

struct Smoker* createSmoker(int resource, struct Agent* agent) {
  struct Smoker* somker = malloc (sizeof (struct Smoker));
  somker->resource = resource;
  somker->mx = agent->mutex;
  somker->smoking = agent->smoke;
  return somker;
}


/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
  
  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

void re_so_far (int r){
  if(r == 3){
    uthread_cond_signal(m_p);
    resource = 0;
  }
  else if(r == 5){
    uthread_cond_signal(t_m);
    resource = 0;
  }
  else if(r == 6){
    uthread_cond_signal(t_p);
    resource = 0;
  }
  else
    ;
}

void* get_m(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){
    uthread_cond_wait(a->match);
    resource += 1;
    re_so_far(resource);
  }
  uthread_mutex_unlock (a->mutex);
}

void* get_p(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){
    uthread_cond_wait(a->paper);
    resource += 2;
    re_so_far(resource);
  }
  uthread_mutex_unlock (a->mutex);
}

void* get_to(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){
    uthread_cond_wait(a->tobacco);
    resource += 4;
    re_so_far(resource);
  }
  uthread_mutex_unlock (a->mutex);
}

void* smoke (void* sv) {
  struct Smoker* s = sv;
  static const int matching_smoker[] = {MATCH,     PAPER,         TOBACCO};
  uthread_mutex_lock (s->mx);
  while(1){
    int r = s->resource;
    if (r == 1) {
      VERBOSE_PRINT ("need paper and tobacco\n");
      uthread_cond_wait (t_p);
    }
    else if (r == 2) {
      VERBOSE_PRINT ("need match and tobacco\n");
      uthread_cond_wait (t_m);
    }
    else{
      VERBOSE_PRINT ("need match and paper\n");
      uthread_cond_wait (m_p);
    }
    smoke_count [matching_smoker [r-1]] ++;
    VERBOSE_PRINT ("Smoking!\n");
    uthread_cond_signal (s->smoking);
  }
  uthread_mutex_unlock (s->mx);
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (1);
  struct Agent*  a = createAgent();

  struct Smoker* s1 = createSmoker(1, a);
  struct Smoker* s2 = createSmoker(2, a);
  struct Smoker* s3 = createSmoker(3, a);
  m_p = uthread_cond_create(a->mutex);
  t_p = uthread_cond_create(a->mutex);
  t_m = uthread_cond_create(a->mutex);
  // TODO
  uthread_create (smoke, s1);
  uthread_create (smoke, s2);
  uthread_create (smoke, s3);
  uthread_create (get_m, a);
  uthread_create (get_p, a);
  uthread_create (get_to, a);
  uthread_join (uthread_create (agent, a), 0);
  

  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}