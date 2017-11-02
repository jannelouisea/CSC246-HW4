#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include<assert.h>
#include<unistd.h>
#include <mach/machine.h>

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
pthread_cond_t taskObtained     = PTHREAD_COND_INITIALIZER;
pthread_cond_t taskPlaced       = PTHREAD_COND_INITIALIZER;

unsigned taskCount = 0;         /// main condition variable to check how many tasks are available ///
double globalMin = 0.0;
unsigned pointsCount = 0;
boolean_t running = TRUE;

unsigned nworker;

static void Error_msg(const char * msg) {
    printf("%s\n", msg);
    exit(1);
}

/// Producer and Consumer code ///
unsigned fill   = 0;
unsigned use    = 0;

void putTask(enum TASK_TYPE type) {
    // task_queue[fill] = task;
    (taskQueue + fill)->task_type = type;
    if (type == LOCAL_MIN) {
        (taskQueue + fill)->pidx = pointsCount - 1;
    }
    fill = (fill + 1) % MAX_TASK;
    taskCount++;
    printf("Task Count: %d\n", taskCount);
}

struct Task getTask() {
    struct Task task = *(taskQueue + use);
    use = (use + 1) % MAX_TASK;
    taskCount--;
    printf("Task Count: %d\n", taskCount);
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
    struct Task task;
    unsigned worker_id = (unsigned) arg;
    while (running) {
        pthread_mutex_lock(&work);
            while (taskCount == 0) {
                /*
                if (!running) {
                    return NULL;
                }
                 */
                pthread_cond_wait(&taskPlaced, &work);
            }
            task = getTask();
            pthread_cond_signal(&taskObtained);
        pthread_mutex_unlock(&work);
    }
    return NULL;
}

void addPoint(char line[LINE_LEN]) {
    char * x = strtok(line, " ");
    char * y = strtok(0, " ");

    (points + pointsCount)->x = (int) strtol(x, NULL, 10);
    (points + pointsCount)->y = (int) strtol(y, NULL, 10);
    (points + pointsCount)->minSquaredDist = 0.0;
    pointsCount++;
}

void printPoint() {
    int pointIdx = pointsCount - 1;
    printf("Point idx: %d\n", pointIdx);
    printf("x: %d\n", (points + pointIdx)->x);
    printf("y: %d\n", (points + pointIdx)->y);
    printf("minDist: %f\n", (points + pointIdx)->minSquaredDist);
}

int main(int argc, char * argv[]) {

    if (argv[1] == NULL || argv[2] == NULL || argc != 3) {
        Error_msg("Usage: ./p4 <thread num> <list file name>");
    }

    nworker = (unsigned) strtol(argv[1], NULL, 10);
    if (nworker <= 0) {
        Error_msg("worker number should be larger than 0!");
    }
    if (nworker > MAX_THREADS) {
        Error_msg("Worker number reaches max!");
    }

    taskQueue = malloc(MAX_TASK * sizeof(struct Task));
    points = malloc(MAX_POINTS * sizeof(struct Point));

    /// Create threads ///
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
    char * fname = argv[2];

    fp = fopen(fname, "r");

    char line[LINE_LEN];

    /// Get the first point in the file ///
    if (fgets(line, sizeof(line), fp) != NULL) {
        addPoint(line);
        // printPoint();
    } else {
        printf("file is empty.\n");
        return 0;
    }

    /// Read the rest of the points in the file ///
    while (pointsCount <= MAX_POINTS && fgets(line, sizeof(line), fp) != NULL) {
        addPoint(line);
        // printPoint();
        /// Critical section ///
        pthread_mutex_lock(&work);
        while (taskCount > (MAX_TASK - 1)) {
            pthread_cond_wait(&taskObtained, &work);
        }
        printf("Placing task for point idx: %d\n", pointsCount - 1);
        putTask(LOCAL_MIN);
        pthread_cond_signal(&taskPlaced);
        pthread_mutex_unlock(&work);
        /// Critical section ///
    }

    // call putTask(GLOBAL_MIN)

    printf("Number of points: %d\n", pointsCount);

    fclose(fp);

    /// Join threads ///
    printf("Setting running to false\n");
    running = FALSE;
    printf("Why is this still going in circles\n");
    for (int j = 0; j < nworker; j++) {
        printf("Joining thread %d\n", j);
        if (pthread_join(workers[j], NULL) != 0) {
            Error_msg("Joining thread Error_msg!");
        } else {
            printf("Thread join successfully\n");
        }

        //synchronization destruction
        //other resource destruction
    }

    return 0;
}