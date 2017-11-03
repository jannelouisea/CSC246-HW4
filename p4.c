/* jfave Janne Louise F Ave */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include<stdbool.h>
#include<math.h>
#include <mach/machine.h>

#define MAX_THREADS 10
#define MAX_POINTS 10000
#define MAX_TASK 8
#define LINE_LEN 50

struct Point {
    int x;                      /* x value */
    int y;                      /* y value */
    double minSquaredDist;      /* min SQUARED dist of point */
};

enum TASK_TYPE { LOCAL_MIN, GLOBAL_MIN };

struct Task {
    enum TASK_TYPE task_type;   /* type of task */
    int pidx;                   /* index of point in points array */
};

struct Task * taskQueue;
struct Point * points;
/* Keeps track of a thread's finished state. Used to signal when thread can perform global_min */
bool threadsFinished[MAX_THREADS];

/// synchronization variables (locks and cv) ///
// locks
pthread_mutex_t work            = PTHREAD_MUTEX_INITIALIZER;    /// Used in manager (main) and worker threads ///
pthread_mutex_t updatePoints    = PTHREAD_MUTEX_INITIALIZER;    /// Used in worker thread when updating points array ///

// condition variables
pthread_cond_t taskObtained     = PTHREAD_COND_INITIALIZER;
pthread_cond_t taskPlaced       = PTHREAD_COND_INITIALIZER;

unsigned taskCount = 0;         /// main condition variable to check how many tasks are available ///
double globalMinSquared = -1.0;
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
    taskCount++;
}

struct Task getTask() {
    struct Task task = *(taskQueue + use);
    use = (use + 1) % MAX_TASK;
    taskCount--;
    return task;
}

/// Thread function ///

// local_min function is implemented in the working_thread

// global_min function
void global_min() {
    for (int i = 0; i < pointsCount; i++) {
        double * currMinSquared = &points[i].minSquaredDist;
        if (*currMinSquared < 0) {}
        else {
            if (globalMinSquared == -1.0) {
                globalMinSquared = *currMinSquared;
            } else if (*currMinSquared < globalMinSquared) {
                globalMinSquared = *currMinSquared;
            }
        }
    }
}

bool otherThreadsFinished(unsigned refThreadID) {
    bool otherthreadsFinished = TRUE;
    for (int i = 0; i < nworker; i++) {
        if (nworker == 1) {     // Check if there is only one thread running
            return threadsFinished[i];
        }
        else if (i == refThreadID) {}
        else {
            otherthreadsFinished = otherthreadsFinished && threadsFinished[i];
        }
    }
    return otherthreadsFinished;
}

//  worker_routine
void * worker_routine(void * arg) {
    unsigned * threadID = (unsigned *) arg;
    double local_min = -1.0;
    while (1) {
        /// Critical section ///
        pthread_mutex_lock(&work);
        while (taskCount == 0) {
            if (!running) {
                *(threadsFinished + *threadID) = TRUE;
                pthread_mutex_unlock(&work);
                return NULL;
            }
            pthread_cond_wait(&taskPlaced, &work);
        }
        struct Task task = getTask();
        pthread_cond_signal(&taskObtained);
        pthread_mutex_unlock(&work);
        /// Critical section ///

        if (task.task_type == LOCAL_MIN) {
            struct Point * p1 = &points[task.pidx];
            for (int i = (task.pidx - 1); i >= 0; i--) {
                struct Point * p2 = &points[i];
                double distSquared = pow(p2->x - p1->x, 2) + pow(p2->y - p1->y, 2);

                /// Critical Section ///
                if (local_min == -1.0 || distSquared <= local_min) {
                    local_min = distSquared;
                    pthread_mutex_lock(&updatePoints);
                    if (p1->minSquaredDist == -1.0 || distSquared < p1->minSquaredDist) {
                        p1->minSquaredDist = local_min;
                    }
                    if (p2->minSquaredDist == -1.0 || distSquared < p2->minSquaredDist) {
                        p2->minSquaredDist = local_min;
                    }
                    pthread_mutex_unlock(&updatePoints);
                }
                /// Critical Section ///
            }
        } else {
            while (otherThreadsFinished(*threadID)) {
                sleep(1);
            }
            global_min();
        }
    }
    *(threadsFinished + *threadID) = TRUE;
    return NULL;
}

void addPoint(char line[LINE_LEN]) {
    char * x = strtok(line, " ");
    char * y = strtok(0, " ");

    (points + pointsCount)->x = (int) strtol(x, NULL, 10);
    (points + pointsCount)->y = (int) strtol(y, NULL, 10);
    (points + pointsCount)->minSquaredDist = -1.0;
    pointsCount++;
}

int main(int argc, char * argv[]) {
    // clock_t begin = clock();
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
        if (pthread_create(&workers[i], NULL, worker_routine, (void *) &i) != 0) {
            Error_msg("Creating thread Error_msg!");
        }
        threadsFinished[i] = FALSE;
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
        while (taskCount >= MAX_TASK) {
            pthread_cond_wait(&taskObtained, &work);
        }
        putTask(LOCAL_MIN);
        pthread_cond_signal(&taskPlaced);
        pthread_mutex_unlock(&work);
        /// Critical section ///
    }

    if (pointsCount < 2) {
        printf("Only one point in file.\n");
        return 0;
    }

    /// Call global task ///
    pthread_mutex_lock(&work);
    while (taskCount >= MAX_TASK) {
        pthread_cond_wait(&taskObtained, &work);
    }
    putTask(GLOBAL_MIN);
    pthread_mutex_unlock(&work);

    fclose(fp);

    /// Join threads ///
    running = FALSE;
    pthread_cond_broadcast(&taskPlaced);    // wake up any threads that are still sleeping and waiting on producer to signal
    for (int j = 0; j < nworker; j++) {
        if (pthread_join(workers[j], NULL) != 0) {
            Error_msg("Joining thread Error_msg!");
        }
    }

    // See the min dist for each thread
    for(int k = 0; k < pointsCount; k++) {
        if (points[k].minSquaredDist == globalMinSquared) {
            printf("(%d,%d) ", points[k].x, points[k].y);
        }
    }
    printf("[%f]\n", sqrt(globalMinSquared));

    free(taskQueue);
    free(points);

    // clock_t end = clock();
    // double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    // printf("time spent: %f", time_spent);

    return 0;
}
