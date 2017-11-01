#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include<assert.h>
#include<unistd.h>

#define MAX_THREADS 10
#define MAX_POINTS 10000
#define MAX_TASK 8
#define LINE_LEN 50

struct Point {
    int x;                      /// x value ///
    int y;                      /// y value ///
    double minSquaredDist;      /// min SQUARED dist of point ///
};

enum TASK_TYPE { LOCAL_MIN, GLOBAL_MIN };

struct Task {
    enum TASK_TYPE task_type;   /// type of task ///
    int pidx;                   /// index of point ins points array ///
};

// struct Task task_queue[MAX_TASK]; // TODO make this a pointer to 8 tasks
struct Task * taskQueue;

// struct Point points[MAX_POINTS + 1];    /// Array to store points in the file ///
struct Point * points;

/// synchronization variables (locks and cv) ///
// locks
pthread_mutex_t work            = PTHREAD_MUTEX_INITIALIZER;    /// Used in manager (main) and worker threads ///
pthread_mutex_t updatePoints    = PTHREAD_MUTEX_INITIALIZER;    /// Used in worker thread when updating points array ///

// condition variables
pthread_cond_t placeTask        = PTHREAD_COND_INITIALIZER;
pthread_cond_t obtainTask       = PTHREAD_COND_INITIALIZER;

unsigned taskCount = 0;         /// main condition variable to check how many tasks are available ///
double globalMin = 0.0;
unsigned pointsCount = 0;

unsigned nworker;

static void Error_msg(const char * msg) {
    printf("%s\n", msg);
    exit(1);
}

/// Producer and Consumer code ///
unsigned fill   = 0;
unsigned use    = 0;

void putTask(struct Task task) {
    // task_queue[fill] = task;
    *(taskQueue + fill) = task;
    fill = (fill + 1) % MAX_TASK;
    taskCount++;
}

struct Task get() {
    struct Task task = *(taskQueue + use);
    use = (use + 1) % MAX_TASK;
    taskCount--;
    return task;
}

/// Thread function ///

// local_min function
void local_min() {

}

// global_min function
void global_min() {

}

//  worker_routine
void * worker_routine(void * arg) {

}

void addPoint(char line[LINE_LEN]) {
    char * x = strtok(line, " ");
    char * y = strtok(0, " ");

    (points + pointsCount)->x = (int) strtol(x, NULL, 10);
    (points + pointsCount)->y = (int) strtol(y, NULL, 10);
    (points + pointsCount)->minSquaredDist = 0.0;
}

int main(int argc, char * argv[]) {

    if (argv[1] == NULL || argv[2] == NULL || argc != 3) {
        Error_msg("Usage: ./p4 <thread num> <list file name>");
    }

    nworker = atoi(argv[1]);
    if (nworker <= 0) {
        Error_msg("worker number should be larger than 0!");
    }
    if (nworker > MAX_THREADS) {
        Error_msg("Worker number reaches max!");
    }

    taskQueue = malloc(MAX_TASK * sizeof(struct Task));
    points = malloc(MAX_POINTS * sizeof(struct Point));

    //synchronization initialization

    pthread_t workers[nworker];

    unsigned i;
    unsigned worker_index[nworker];
    for (i = 0; i < nworker; i++) {
        worker_index[i] = i;
        if (pthread_create(&workers[i], NULL, worker_routine, (void *) &worker_index[i]) != 0) {
            Error_msg("Creating thread Error_msg!");
        }
    }

    /// manager routine ///
    FILE * fp;
    char * fname = NULL;

    fp = fopen(fname, "r");

    char line[LINE_LEN];

    /// Get the first point in the file ///
    if (fgets(line, sizeof(line), fp) != NULL) {
        addPoint(line);
    } else {
        printf("file is empty.\n");
        return 0;
    }

    /// Read the rest of the points in the file ///

    fclose(fp);

    for (i = 0; i < nworker; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            Error_msg("Joining thread Error_msg!");
        }

        //synchronization destruction
        //other resource destruction

        return 0;
    }
}