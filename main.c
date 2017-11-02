#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include<assert.h>
#include<unistd.h>
#include<math.h>
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
    int pidx;                   /// index of point in points array ///
};

struct Task * taskQueue;
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
    (taskQueue + fill)->task_type = type;
    if (type == LOCAL_MIN) {
        (taskQueue + fill)->pidx = pointsCount - 1;
    }
    fill = (fill + 1) % MAX_TASK;
    printf("Placed task for point: %d\n", pointsCount - 1);
    taskCount++;
}

struct Task getTask() {
    struct Task task = *(taskQueue + use);
    use = (use + 1) % MAX_TASK;
    taskCount--;
    return task;
}

/// Thread function ///

// local_min function
void local_min(int pidx) {
    struct Point p1 = points[pidx];
    for (int i = (pidx - 1); i >= 1; i++) {
        struct Point p2 = points[i];
        double distSquared = pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2);
        pthread_mutex_lock(&updatePoints);
        if (distSquared < p1.minSquaredDist) {
            p1.minSquaredDist = distSquared;
        }
        if (distSquared < p2.minSquaredDist) {
            p2.minSquaredDist = distSquared;
        }
        pthread_mutex_unlock(&updatePoints);
    }
}

// global_min function
void global_min() {

}

//  worker_routine
void * worker_routine(void * arg) {
    unsigned threadID = (unsigned) arg;
    printf("thread %d started\n", threadID);
    while (1) {
        /// Critical section ///
        pthread_mutex_lock(&work);
        printf("thread %d has work lock\n", threadID);
        while (taskCount == 0) {
            if (!running) {
                printf("thread %d is no longer working an releases work lock\n", threadID);
                pthread_mutex_unlock(&work);
                return NULL;
            }
            printf("thread %d is waiting on task\n", threadID);
            pthread_cond_wait(&taskPlaced, &work);
        }
        struct Task task = getTask();
        printf("thread %d got task for point: %d\n", threadID, task.pidx);
        printf("thread %d signals that task was obtained\n", threadID);
        pthread_cond_signal(&taskObtained);
        printf("thread %d is about to unlock work lock\n", threadID);
        pthread_mutex_unlock(&work);
        /// Critical section ///
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
    } else {
        printf("file is empty.\n");
        return 0;
    }

    /// Read the rest of the points in the file ///
    while (pointsCount <= MAX_POINTS && fgets(line, sizeof(line), fp) != NULL) {
        addPoint(line);
        /// Critical section ///
        pthread_mutex_lock(&work);
        printf("MAIN thread has work lock\n");
        while (taskCount >= MAX_TASK) {
            printf("MAIN is sleeping\n");
            pthread_cond_wait(&taskObtained, &work);
        }
        printf("MAIN thread places task\n");
        putTask(LOCAL_MIN);
        printf("MAIN thread signals task is placed\n");
        pthread_cond_signal(&taskPlaced);
        printf("MAIN thread unlocks work thread\n");
        pthread_mutex_unlock(&work);
        /// Critical section ///
    }

    // call putTask(GLOBAL_MIN)

    printf("Number of points: %d\n", pointsCount);

    fclose(fp);

    /// Join threads ///
    printf("Setting running to false\n");
    running = FALSE;
    pthread_cond_broadcast(&taskPlaced);
    for (int j = 0; j < nworker; j++) {
        printf("Joining thread %d\n", j);
        if (pthread_join(workers[j], NULL) != 0) {
            Error_msg("Joining thread Error_msg!");
        } else {
            printf("Successfully joined\n");
        }
    }

    // See the min dist for each thread
    for(int i = 0; i < pointsCount; i++) {
        printf("(%d,%d) - %f\n", points[i].x, points[i].y, points[i].minSquaredDist);
    }

    return 0;
}