#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <chrono>
#include <random>


#define GALLERY1_CAPACITY 5
#define CORRIDOR_CAPACITY 3

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define ORANGE "\033[38;5;214m" 
#define PINK "\033[38;5;200m"   
#define PURPLE "\033[38;5;93m"
#define LIGHT_BLUE "\033[38;5;81m"


sem_t gallery1_sem;
sem_t corridor_sem;
pthread_mutex_t step1;
pthread_mutex_t step2;
pthread_mutex_t step3;
pthread_mutex_t access_lock;    
pthread_mutex_t pb;
pthread_mutex_t sc_lock;   
pthread_mutex_t pc_lock; 
pthread_mutex_t write_lock; 
pthread_cond_t no_premium_waiting = PTHREAD_COND_INITIALIZER; 
int sc = 0;                
int pc = 0;
int w, x, y, z;


auto start_time = std::chrono::high_resolution_clock::now();

void init_sem_n_mutex()
{
    sem_init(&gallery1_sem,0,GALLERY1_CAPACITY);
    sem_init(&corridor_sem,0,CORRIDOR_CAPACITY);
    pthread_mutex_init(&step1,NULL);
    pthread_mutex_init(&step2,NULL);
    pthread_mutex_init(&step3,NULL);
    pthread_mutex_init(&access_lock, NULL);
    pthread_mutex_init(&pb, NULL);
    pthread_mutex_init(&sc_lock, NULL);
    pthread_mutex_init(&pc_lock, NULL);
    pthread_mutex_init(&write_lock, NULL);
}

void spend_time(int base) {
    usleep(base * 100000); // Sleep (in microseconds)
}


int get_random_number() {
  std::random_device rd;
  std::mt19937 generator(rd());

  // Lambda value for the Poisson distribution
  double lambda = 10000.234;
  std::poisson_distribution<int> poissonDist(lambda);
  return poissonDist(generator);
}


long long get_time() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  long long elapsed_time_ms = duration.count();
  long long elapsed_time_ds = elapsed_time_ms / 100;
  return elapsed_time_ds;
}


void visit_hallway_and_steps(int id){

    pthread_mutex_lock(&write_lock);
    printf(WHITE "Visitor %d has arrived at A at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(w);

    pthread_mutex_lock(&write_lock);
    printf(LIGHT_BLUE "Visitor %d has arrived at B at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(1);
    pthread_mutex_lock(&step1);

    pthread_mutex_lock(&write_lock);
    printf(MAGENTA "Visitor %d is at step1 at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(1);
    pthread_mutex_lock(&step2);
    pthread_mutex_unlock(&step1);

    pthread_mutex_lock(&write_lock);
    printf(CYAN "Visitor %d is at step2 at timestamp %lld.\n" RESET,  id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(1);
    pthread_mutex_lock(&step3);
    pthread_mutex_unlock(&step2);

    pthread_mutex_lock(&write_lock);
    printf(PINK "Visitor %d is at step3 at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(1);
    sem_wait(&gallery1_sem);
    pthread_mutex_unlock(&step3);

}

void visit_gallery1_and_corridor(int id){
   
    pthread_mutex_lock(&write_lock);
    printf(YELLOW "Visitor %d is at C (entered Gallery 1) at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(x);
    sem_wait(&corridor_sem);
    sem_post(&gallery1_sem);

    pthread_mutex_lock(&write_lock);
    printf(PURPLE "Visitor %d is at D (exiting Gallery 1) at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(get_random_number() % 5);

    sem_post(&corridor_sem);

}

void simulate_premium_ticket_holder(int id){
    pthread_mutex_lock(&pc_lock);
    pc++;
    if (pc == 1) pthread_mutex_lock(&access_lock); 
    pthread_mutex_unlock(&pc_lock);
    spend_time(1);

    pthread_mutex_lock(&pb); 

    pthread_mutex_lock(&write_lock);
    printf(GREEN "Visitor %d is inside the photo booth at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(z);

    pthread_mutex_lock(&write_lock);
    printf(BLUE "Visitor %d is at F (exiting gallery 2) at timestamp %lld.\n" RESET, id, get_time());   
    pthread_mutex_unlock(&write_lock);

    pthread_mutex_unlock(&pb);

    pthread_mutex_lock(&pc_lock);
    pc--;
    if (pc == 0) {
        pthread_cond_broadcast(&no_premium_waiting);
        pthread_mutex_unlock(&access_lock); 
    }
    pthread_mutex_unlock(&pc_lock);
}

void simulate_standard_ticket_holder(int id) {

    spend_time(1);

    pthread_mutex_lock(&pc_lock); 
    while (pc > 0) {                 //This while loop does not cause busy-waiting
        pthread_cond_wait(&no_premium_waiting, &pc_lock); 
    }
    pthread_mutex_unlock(&pc_lock);

    pthread_mutex_lock(&access_lock); 
    pthread_mutex_lock(&sc_lock);
    sc++;
    if (sc == 1) pthread_mutex_lock(&pb); 
    pthread_mutex_unlock(&sc_lock);
    pthread_mutex_unlock(&access_lock);
    
    pthread_mutex_lock(&write_lock); 
    printf(GREEN "Visitor %d is inside the photo booth at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(z);

    pthread_mutex_lock(&write_lock);
    printf(BLUE "Visitor %d is at F (exiting gallery 2) at timestamp %lld.\n" RESET, id, get_time());   
    pthread_mutex_unlock(&write_lock);

    pthread_mutex_lock(&sc_lock);
    sc--;
    if (sc == 0) pthread_mutex_unlock(&pb); 
    pthread_mutex_unlock(&sc_lock);
}


void visit_gallery2_and_photoBooth(int id) {

    pthread_mutex_lock(&write_lock);
    printf(ORANGE "Visitor %d is at E (entered Gallery 2) at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    spend_time(y + get_random_number() % 3);

    pthread_mutex_lock(&write_lock);
    printf(RED "Visitor %d is about to enter the photo booth at timestamp %lld.\n" RESET, id, get_time());
    pthread_mutex_unlock(&write_lock);

    if(id >= 2001 && id <= 2100){
        simulate_premium_ticket_holder(id);
    }

    else if(id >= 1001 && id <= 1100){
        simulate_standard_ticket_holder(id);
    } 

}


void* visit(void* arg) {
    
    int id = (intptr_t)arg;

    spend_time(get_random_number() % 10);

    //Task-1
    visit_hallway_and_steps(id);
  
    //Task-2
    visit_gallery1_and_corridor(id);

    //Task-3
    visit_gallery2_and_photoBooth(id);
   
    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s N M w x y z\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]); // Number of standard ticket holders
    int M = atoi(argv[2]); // Number of premium ticket holders
    w = atoi(argv[3]); // Time in hallway
    x = atoi(argv[4]); // Time in Gallery 1
    y = atoi(argv[5]); // Time in Gallery 2
    z = atoi(argv[6]); // Time in photo booth

    init_sem_n_mutex();

    pthread_t threads[N + M];

    start_time = std::chrono::high_resolution_clock::now(); //reset start time

    for (int i = 0; i < N; ++i) {
        int id = 1001 + i;
        pthread_create(&threads[i], NULL, visit, (void*)(intptr_t)id);
    }

    for (int i = 0; i < M; ++i) {
        int id = 2001 + i;
        pthread_create(&threads[N + i], NULL, visit, (void*)(intptr_t)id);
    }

    for (int i = 0; i < N + M; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}




//g++ 2005117.cpp -o museum -lpthread -std=c++11
//./museum 10 0 2 0 0 0