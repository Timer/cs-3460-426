/**
 * Project 4B (Bumper Cars!)
 *
 * Joseph Haddad
 * Emily Trenka
 * Caleb Kupetz
 */

#include <pthread.h>
#include <semaphore.h>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <thread>

#define N_CARS 2
#define N_RIDERS 5
#define T_WANDER 100 /* each wandering time is between 0 to T_WANDER */
#define T_BUMP 40    /* each bumping time is between 0 to T_BUMP */
#define T_DISPLAY 2  /* time between situational display */

sem_t RiderPresent;
// Mutex to prevent concurrent writes to list
sem_t LineMutex;
sem_t CountMutex;

sem_t WaitForRideBegin[N_RIDERS];
sem_t WaitForRideOver[N_RIDERS];

int COUNT_DOWN = 10; /* Simulation time: Total number of bumper car rides */
std::deque<int> WaitArea;

int CarPerson[N_CARS];
bool IsWaiting[N_RIDERS];

void P(sem_t *s) {
  while (sem_wait(s) != 0) {}
}
void V(sem_t *s) { sem_post(s); }

bool finish() {
  return COUNT_DOWN <= 0;
}

void Wander(int rid, int interval) {
  printf("Rider %d is wandering around the park.\n", rid);
  std::this_thread::sleep_for(std::chrono::seconds(interval));
}

void GetInLine(int rid) {
  P(&LineMutex);
  WaitArea.push_back(rid);
  printf("Rider %d gets in the waiting line.\n", rid);
  V(&LineMutex);

  V(&RiderPresent);
}

void TakeASeat(int rid) {
  IsWaiting[rid] = true;
  P(&WaitForRideBegin[rid]);
  if (finish()) {
    printf("Rider %d has left because we're shutdown.\n", rid);
    pthread_exit(0);
  }
}

void TakeARide(int rid) {
  IsWaiting[rid] = false;
  printf("Rider %d is taking the ride.\n", rid);
  P(&WaitForRideOver[rid]);
}

/* Rider thread. rid is a number between 0 to N_RIDERS - 1. */
void *Rider(void *_rid) {
  int rid = *(int *) _rid;
  while (true) { /* wander around for a random amount of time */
    Wander(rid, rand() % T_WANDER);
    GetInLine(rid);
    TakeASeat(rid);
    TakeARide(rid);
    if (finish()) {
      printf("Rider %d has left because we're shutdown.\n", rid);
      break;
    }
  }
  return nullptr;
}

int Load(int cid) {
  P(&RiderPresent);
  if (finish()) {
    pthread_exit(0);
    return -1;
  }
  P(&LineMutex);
  if (WaitArea.size() < 1) assert(false);
  int this_guy = WaitArea.front();
  WaitArea.pop_front();
  CarPerson[cid] = this_guy;
  printf("Car %d takes the rider %d.\n", cid, this_guy);
  V(&LineMutex);

  V(&WaitForRideBegin[this_guy]);
  return this_guy;
}

void Bump(int cid, int interval) {
  printf("Car %d is bumping around the arena.\n", cid);
  std::this_thread::sleep_for(std::chrono::seconds(interval));
}

void Unload(int cid, int rid) {
  printf("This ride of Car %d is over. Get out %d.\n", cid, rid);
  CarPerson[cid] = -1;
  V(&WaitForRideOver[rid]);
}

/* Bumper car thread. cid is a number between 0 to N_CARS - 1. */
void *Car(void *_cid) {
  int cid = *(int *) _cid;
  while (true) {
    int rid = Load(cid);
    if (rid < 0) break;
    Bump(cid, rand() % T_BUMP);
    Unload(cid, rid);
    P(&CountMutex);
    --COUNT_DOWN;
    V(&CountMutex);
    if (finish()) break;
  }
  return nullptr;
}

/* Displaying thread */
void *Display(void *dummy) {
  while (!finish()) {
    printf("The current situation in the park is:\n");
    for (int i = 1; i <= N_CARS; ++i) {
      auto rid = CarPerson[i - 1];
      if (rid >= 0) {
        printf("Car %d is running. The rider is %d\n", i - 1, rid);
      } else {
        printf("Car %d is not running\n", i - 1);
      }
    }

    for (int i = 1; i <= N_RIDERS; ++i) {
      bool inCar = false;
      for (int j = 0; j < N_CARS && !inCar; ++j) {
        inCar = CarPerson[j] == i - 1;
      }
      bool inLine = IsWaiting[i - 1];
      if (inCar) {
        printf("Rider %d is in a car.\n", i - 1);
      } else if (inLine) {
        printf("Rider %d is waiting in line\n", i - 1);
      } else {
        printf("Rider %d is wandering\n", i - 1);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(T_DISPLAY));
  }
}

int main(int argc, char *argv[]) {
  srand(time(NULL));

  assert(sem_init(&RiderPresent, 0, 0) == 0);
  assert(sem_init(&CountMutex, 0, 1) == 0);
  assert(sem_init(&LineMutex, 0, 1) == 0);
  for (int i = 0; i < N_RIDERS; ++i) {
    assert(sem_init(&WaitForRideBegin[i], 0, 0) == 0);
  }
  for (int i = 0; i < N_RIDERS; ++i) {
    assert(sem_init(&WaitForRideOver[i], 0, 0) == 0);
  }

  pthread_t cars[N_CARS];
  for (int i = 0; i < N_CARS; ++i) {
    CarPerson[i] = -1;

    int *id = new int;
    *id = i;
    pthread_create(&cars[i], NULL, Car, (void *) id);
  }
  pthread_t riders[N_RIDERS];
  for (int i = 0; i < N_RIDERS; ++i) {
    IsWaiting[i] = false;

    int *id = new int;
    *id = i;
    pthread_create(&riders[i], NULL, Rider, (void *) id);
  }
  pthread_t displayThread;
  pthread_create(&displayThread, NULL, Display, nullptr);
  // Wait for the cars to run out of gas (?)
  for (int i = 0; i < N_CARS; ++i) { pthread_join(cars[i], NULL); }
  pthread_join(displayThread, NULL);
  // Un-wait all riders so that they die
  for (int i = 0; i < N_RIDERS; ++i) { V(&WaitForRideBegin[i]); }
  for (int i = 0; i < N_RIDERS; ++i) { pthread_join(riders[i], NULL); }
  return 0;
}
