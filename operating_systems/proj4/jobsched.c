#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;

int MAX_INPUT_LEN = 500;
int MAX_WORDS = 3;

typedef struct Job{
    // job info
    int jobid;
    char * in_file;
    time_t in_time;
    size_t in_size;

    time_t start_time;

    char * out_file_name;
    time_t out_time;
    size_t out_size;
    
    /*
    store the status of a job:
    -1 = waiting
    0 = running
    1 = done
    */
    int job_stat;
    char * job_status;
    // only used in balanced scheduling
    int passed_over;

    // pointer for linked list
    struct Job * next;
    struct Job * prev; 
}Job;

typedef struct {
    // head of the doubly linked list
    Job * head;
    Job * tail;

    size_t last_job_id;
    size_t count;
    size_t waiting;
    size_t done;
    size_t total_output_size;
} Job_list;

// function declarations
void * fcfs_worker(void * arg);
void list_jobs( Job_list * queue);
void nthreads(int threads, Job_list * queue, char mode);
void delete_queue(Job_list * queue);
size_t file_size(char * filename);
int submit (char * filename, Job_list * queue);
int process(Job * work);
void waitfor(Job_list * queue, int jobid);
void wait_all(Job_list * queue);
int delete(Job_list * queue, int jobid);
void * sjf_worker(void * arg);
void * balanced_worker(void * arg);


int delete(Job_list * queue, int jobid) {
    // deletes the job with the specified jobid
    // acquire lock
    Job * curr;
    pthread_mutex_lock(&mutex);

    // find the job
    curr = queue->head;
    while (curr) {
        if (jobid == curr->jobid) break;
        curr = curr->next;
    }

    // handle missing job
    if (!curr) {
        printf("jobsched-delete: unable to find job with id: %d. No job was deleted.\n", jobid);
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    else if (curr->job_stat == 0) {
        printf("jobsched-delete: Job %d is currently running, and cannot be deleted!!\n", jobid);
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    // if the job is either waiting
    if (curr->job_stat == -1) {
        queue->waiting--;
    }
    // or done
    else if (curr->job_stat == 1) {
        queue->done--;
        queue->total_output_size -= curr->out_size;

        // remove the output file
        if (remove(curr->out_file_name) < 0) {
            printf("jobsched-delete: Error removing file %s: %s\n", curr->out_file_name, strerror(errno));
            printf("jobsched-delete: still removing job %d\n", jobid);
        }
    }
    // changes that are made every time
    queue->count--;
    // remove from the list 
    if (curr == queue->head) {
        queue->head = curr->next;
    }
    else if (queue->tail == curr) {
        queue->tail = curr->prev;
    }
    if (curr->next) {
        curr->next->prev = curr->prev;
    }
    if (curr->prev) {
        curr->prev->next = curr->next;
    }

    // remove node
    free(curr);
    printf("jobsched-delete: Job %d has been removed\n", jobid);
    pthread_mutex_unlock(&mutex);
    
    return 0;
}

void wait_all(Job_list * queue) {
    // waits for all the jobs 

    pthread_mutex_lock(&mutex);
    while (queue->done < queue->count) {
        pthread_cond_wait(&cond, &mutex);
    }
    printf("All Jobs Are Done!!\n");
    pthread_mutex_unlock(&mutex);
}

void waitfor(Job_list * queue, int jobid) {
    // waits for a specific job and lists all of that information
    Job * curr; 

    pthread_mutex_lock(&mutex);
    // find the address of the job to check 
    curr = queue->head;
    while (curr) {
        if (curr->jobid == jobid) break;
        curr = curr->next;
    }
    if (!curr) {
        printf("jobsched-wait: unable to find job %d\n", jobid);
        return;
    }

    // wait for the job to be done 
    while (curr->job_stat != 1) {
        pthread_cond_wait(&cond, &mutex);
    }

    printf("Job %d was a ", jobid);
    if (curr->out_size == 0) {
        printf("Failure!\n");
        return;
    }
    else {
        printf("Success!\n");
        printf("Job %d was submitted at: %s", jobid, ctime(&curr->in_time));
        printf("Job %d started running at %s", jobid, ctime(&curr->start_time));
        printf("Job %d finished at %s", jobid, ctime(&curr->out_time));
    }
    pthread_mutex_unlock(&mutex);
}

int process(Job * work) {
    // do the actual work 
    // can only read job values without mutex, not list pointers
    // running status prevents other threads from changing things

    pid_t new_pid = fork();
    if (new_pid < 0) {
        printf("jobsched-process: unable to fork: %s\n", strerror(errno));
        return -1;
    }
    else if (new_pid == 0) {
        // child process
        // must redirect stdin
        int file = open(work->in_file, 'r');
        if (dup2(file, STDIN_FILENO) < 0) {
            printf("jobsched-process: unable to redirect %s: %s\n", work->in_file, strerror(errno));
        }
        close(file);

        // redirect stdout 
        file = open("/dev/null", 'w');
        if (dup2(file, STDOUT_FILENO) < 0) {
            printf("jobsched-process: unable to redirect stdout: %s\n", strerror(errno));
        }
        if (dup2(file, STDERR_FILENO) < 0) {
            printf("jobsched-process: unable to redirect stderr: %s\n", strerror(errno));
        }
        close(file);
        
        execl("piper/piper", "piper", "-f", work->out_file_name, "-m", "arctic.onnx", NULL);
    }
    else {
        int status;
        waitpid(new_pid, &status, 0); 

        // handle weird exits
        // if exited normally
        if (WIFEXITED(status)) {
            return 0;
        }
        else {
            printf("jobsched-fcfs: process %d exited abnormally", new_pid);
            if (WIFSIGNALED(status)) {
                printf(" with signal %d", WTERMSIG(status));
                return -1;
            }
            if (WCOREDUMP(status)) {
                printf(": Segmentation Fault.");
                return -1;
            }
        }
    }
    return 0;
}

size_t file_size(char * filename) {
    // return the filesize of a file
    struct stat * st = malloc(sizeof(struct stat));
     // get filesize
    int e_num = lstat(filename, st);
    if (e_num != 0) {
            printf("jobsched: Unable to stat %s: %s\n", filename, strerror(errno));
            free(st);
            return 0;
    }
    size_t out = st->st_size;
    free(st);
    return out;

}

int submit (char * filename, Job_list * queue) {
    // pushes the filename to the struct
    // first allocate and fill in the node and then push it to the linked list within
    // the mutex
    Job * new = malloc(sizeof(Job));

    // copy name
    new->in_file = strdup(filename);
    if (!new->in_file) {
        printf("jobsched-submit: error copying filename: %s", strerror(errno));
        free(new);
        return 1;
    }

    time(&new->in_time);
    new->job_status = malloc(sizeof(char) * 10);
    strcpy(new->job_status, "WAITING");
    new->job_stat = -1;

    new->in_size = file_size(new->in_file);
    // handle a file that doesn't exist
    if (new->in_size == 0) {
        printf("jobsched-submit: empty or non-existent file, not adding to queue\n");
        free(new);
        return 1;
    }

    new->out_file_name = calloc(20, sizeof(char));
    new->out_size = 0;
    new->out_time = 0;
    new->start_time = 0;
    // for handling balanced sjf
    new->passed_over = 0;

    // push to list
    pthread_mutex_lock(&mutex);

    // doesn't matter how many jobs in queue always add one
    // set jobid
    queue->last_job_id++; 
    new->jobid = queue->last_job_id;

    // handle empty list scenario
    if (queue->count == 0) {
        new->next = NULL;
        new->prev = NULL;
        queue->head = new;
        queue->tail = new;
    }
    // add to a queue with stuff in it
    else {
        new->next = NULL;
        queue->tail->next = new;
        new->prev = queue->tail;
        queue->tail = new;
        
    }
    queue->count++;
    queue->waiting++;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    
    // print job id
    printf("jobsched: Job %d started on file %s\n", new->jobid, new->in_file);

    return 0;
}

void list_jobs( Job_list * queue) {
    // list all of the jobs 

    // for calculating summary stats
    size_t total_in_size = 0;
    // header 
    printf("JOBID  STATE    INPUT_FILENAME  INPUT_SIZE  OUTPUT_FILE  OUTPUT_SIZE\n");
    printf("____________________________________________________________________\n");
    time_t turnaround = 0;
    time_t response = 0;
    size_t count = 0;
    // lock the mutex 
    pthread_mutex_lock(&mutex);
    size_t output_size = queue->total_output_size;
    Job * curr = queue->head;
    while (curr) {
        total_in_size += curr->in_size;
        printf("%-7d%-9s%-16s%-9liB  %-13s%-10liB\n"
                , curr->jobid, curr->job_status, curr->in_file, curr->in_size
                , curr->out_file_name, curr->out_size);
        
        // handle done statistics
        if (curr->job_stat == 1) {
            turnaround += curr->out_time - curr->in_time;
            response += curr->start_time - curr->in_time;
            count++;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&mutex);
    printf("____________________________________________________________________\n");
    printf("Total input file size: %li B\n", total_in_size);
    printf("Total output file size: %li B\n", output_size);
    if (count > 0) {
        printf("Average turnaround time: %fs\n", (double) turnaround / count);
        printf("Average response time: %fs\n", (double) response / count);
    }

}

void nthreads(int threads, Job_list * queue, char mode) {
    // runs the threads necessary to create the files
    // f for fcfs
    // s for sjf
    // b for balanced 
    pthread_t * out = malloc(sizeof(pthread_t) * threads);

    if (mode == 'f') {
        for (int i = 0; i < threads; i++) {
            pthread_create(&out[i], 0, fcfs_worker, queue);
        }
    }
    else if (mode == 's') {
        for(int i = 0; i < threads; i++) {
            pthread_create(&out[i], 0, sjf_worker, queue);
        }
    }
    else if (mode == 'b') {
        for(int i = 0; i < threads; i++) {
            pthread_create(&out[i], 0, balanced_worker, queue);
        }
    }
    free(out);
    return;    
}

void * fcfs_worker(void * arg) {
    // worker thread function
    Job_list * queue = arg;

    Job * work;

    while (1) {
    // find an available job 
    pthread_mutex_lock(&mutex);
    while (queue->waiting <= 0 || queue->total_output_size >= (1<<20) * 100) {
        pthread_cond_wait(&cond, &mutex);
    }

    // iterate through the queue and find the first available job
    // they are in order of arrival
    work = queue->head;
    while (work) {
        if (work->job_stat == -1) {
            work->job_stat = 0;
            strcpy(work->job_status, "RUNNING");
            queue->waiting--;
            break;
        }
        work = work->next;
    }
    if (!work) {
        printf("jobsched-fcfsworker: unable to find job: exiting!\n");
        return (void *)-1;
    }

    sprintf(work->out_file_name, "job%d.wav", work->jobid);

    pthread_mutex_unlock(&mutex);

    // do the actual processing
    time_t start;
    time(&start);
    process(work);

    // update the values
    pthread_mutex_lock(&mutex);

    work->start_time = start;
    time(&work->out_time);
    work->out_size = file_size(work->out_file_name);
    work->job_stat = 1;
    strcpy(work->job_status, "DONE");
    queue->total_output_size += work->out_size;
    queue->done++;

    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void * sjf_worker(void * arg) {
    // shortest job first worker
    Job_list * queue = arg;
    
    while (1) {
        // find available job
        pthread_mutex_lock(&mutex);
        while (queue->waiting <= 0 || queue->total_output_size >= (1<<20) * 100) {
            pthread_cond_wait(&cond, &mutex);
        } 

        // iterate through queue
        Job * curr = queue->head;
        Job * shortest = NULL;
        while (curr) {
            // if job is waiting
            if (curr->job_stat == -1) {
                // if shortest hasn't been set yet
                if (!shortest) {
                    shortest = curr;
                }
                else if (curr->in_size < shortest->in_size) {
                    shortest = curr;
                }
            }
            curr = curr->next;
        }       

        if (!shortest) { 
            printf("jobsched-sjf: unable to find job: EXITING!\n");
            return (void*) -1;
        }

        // start processing job 
        sprintf(shortest->out_file_name, "job%d.wav", shortest->jobid);
        shortest->job_stat = 0;
        strcpy(shortest->job_status, "RUNNING");
        queue->waiting--;
        pthread_mutex_unlock(&mutex);

        // do the actual processing
        time_t start;
        time(&start);
        process(shortest);

        // update the values
        pthread_mutex_lock(&mutex);

        shortest->start_time = start;
        time(&shortest->out_time);
        shortest->out_size = file_size(shortest->out_file_name);
        shortest->job_stat = 1;
        strcpy(shortest->job_status, "DONE");
        queue->total_output_size += shortest->out_size;
        queue->done++;

        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
     }

     return NULL;
}

void * balanced_worker(void * arg) {
    // balanced worker function (same as sjf with passover handling)
    Job_list * queue = arg;
    // if a job has been passed over 4 times, run it the next time it comes up
    int threshold = 3; 

    while (1) {
        // find available job
        pthread_mutex_lock(&mutex);
        while (queue->waiting <= 0 || queue->total_output_size >= (1<<20) * 100) {
            pthread_cond_wait(&cond, &mutex);
        } 

        // iterate through queue
        Job * curr = queue->head;
        Job * shortest = NULL;
        while (curr) {
            // if job is waiting
            if (curr->job_stat == -1) {
                // if shortest hasn't been set yet
                if (!shortest) {
                    shortest = curr;
                }
                if (curr->passed_over >= threshold) {
                    shortest = curr;
                    break;
                }
                else if (curr->in_size < shortest->in_size) {
                    // if passing over, add one to passover count
                    shortest->passed_over++;
                    printf("Job %d passed over %d times\n", shortest->jobid, shortest->passed_over);
                    shortest = curr;
                }
                
            }

            // if passing over, add one to passover count
            curr = curr->next;
        }       

        if (!shortest) { 
            printf("jobsched-sjf: unable to find job: EXITING!\n");
            return (void*) -1;
        }

        // start processing job 
        sprintf(shortest->out_file_name, "job%d.wav", shortest->jobid);
        shortest->job_stat = 0;
        strcpy(shortest->job_status, "RUNNING");
        queue->waiting--;
        pthread_mutex_unlock(&mutex);

        // do the actual processing
        time_t start;
        time(&start);
        process(shortest);

        // update the values
        pthread_mutex_lock(&mutex);

        shortest->start_time = start;
        time(&shortest->out_time);
        shortest->out_size = file_size(shortest->out_file_name);
        shortest->job_stat = 1;
        strcpy(shortest->job_status, "DONE");
        queue->total_output_size += shortest->out_size;
        queue->done++;

        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
     }

     return NULL;
}

void delete_queue(Job_list * queue) {
    Job * curr = queue->head;

    while (curr) {
        free(curr->in_file);
        free(curr->out_file_name);
        Job * temp = curr;
        curr = curr->next;
        free(temp);
    }
    free(queue);
}

int main (int argc, char ** argv) {
    if (argc != 1) {
        printf("jobsched: USAGE: ./jobsched\n");
        return 1;
    }

    //buffer to store lines
    char * input = malloc(MAX_INPUT_LEN * sizeof(char));
    // stores individual words
    char ** words = malloc((MAX_INPUT_LEN + 3)  * sizeof(char));
    // boolean to store nthreads usage
    int nthreads_used = 0;

    // head of the linked list
    Job_list * queue = malloc(sizeof(Job_list));
    queue->head = NULL;
    queue->tail = NULL;
    queue->last_job_id = 0;
    queue->total_output_size = 0;
    queue->done = 0;
    int threads = 1;
    char mode = 'f';
    
    while (1) {
        // break statement
        if (!fgets(input, MAX_INPUT_LEN, stdin)) break;
        
        // split the line into words
        char * word_one;
        char * word_two;

        // split input 
        words[0] = strtok(input, " \t\n");
        // handle empty line
        if (!words[0]) continue;
        // handle other word
        int word_count;
        for (word_count = 1; word_count < MAX_WORDS; word_count++) {
            words[word_count] = strtok(NULL, " \t\n");
            if (!words[word_count]) {
                words[word_count] = 0;
                break;
            }
        }
        word_one = words[0];
        word_two = words[1];
        // handle instructions with more than two words
        if (word_count == MAX_WORDS) {
            printf("jobsched: too many arguments! Must pick from one of the \nspecified arguments, and only use up to two words!\n");
            continue;
        }        

        if (!strcmp(word_one, "quit")) break;
        
        // add to job list
        else if (!strcmp(word_one, "submit")) {
            // handle improper call of submit
            if (word_count != 2) {
                printf("jobsched-submit: must use the format submit <text_filename>!\n");
                continue;
            }

            // submit the file
            submit(word_two, queue);
        }

        // list the jobs
        else if (!strcmp(word_one, "list")) {
            if (word_count != 1) {
                printf("jobsched-list: must use format: list\n");
                continue;
            }
            list_jobs(queue);
        }
        
        // nthreads
        else if (!strcmp(word_one, "nthreads")) {
            if (word_count != 2) {
                printf("jobsched-nthreads: usage must be nthreads <number-of-threads>!\n");
            }
            if (nthreads_used) {
                printf("jobsched-nthreads: Only allowed to use once per runtime!!! no new threads started\n");
                continue;
            }
            nthreads_used = 1;
            threads = atoi(word_two);
            if (threads == 0) {
                printf("jobsched-nthreads: error reading number of threads or invalid number!\n");
                continue;
            }
            nthreads(threads, queue, mode);
        }

        // wait funcs
        else if (!strcmp(word_one, "wait")) {
            if (word_count != 2) {
                printf("jobsched-wait: usage: wait <jobid>\n");
                continue;
            }
            
            int jobid = atoi(word_two);
            if (jobid <= 0) {
                printf("jobsched-wait: error reading jobid!\n");
                continue;
            }
            waitfor(queue, jobid);
        }
        else if (!strcmp(word_one, "waitall")) {
            if (word_count != 1) {
                printf("jobsched-waitall: usage: waitall\n");
                continue;
            }
            wait_all(queue);
        }

        // delete
        else if (!strcmp(word_one, "delete")) {
            if (word_count != 2) {
                printf("jobsched-delete: usage: delete <jobid>\n");
                continue;
            }

            // turn jobid into int
            int jobid = atoi(word_two);
            if (jobid <= 0) {
                printf("jobsched-delete: error reading jobid or invalid jobid!\n");
                continue;
            }

            delete(queue, jobid);
        }

        // schedule command
        else if (!strcmp(word_one, "schedule")) {
            if (word_count != 2) {
                printf("jobsched-schedule: usage: schedule <fcfs|sjf|balanced>\n");
                continue;
            }

            // change mode char 
            if (!strcmp(word_two, "fcfs")) {
                mode = 'f';
            }
            else if (!strcmp(word_two, "sjf")) {
                mode = 's';
            }
            else if (!strcmp(word_two, "balanced")) {
                mode = 'b';
            }
            else {
                printf("jobsched-schedule: must choose from fcfs, sjf, or balanced\n");
            }
        }

        // help command
        else if (!strcmp(word_one, "help")) {
            printf("Jobsched: help\n"
                   "        Usage: help\n"
                   "        displays help message\n\n"
                   "    Jobsched Functions: \n"
                   "        submit: \n"
                   "            usage: submit <filename> \n"
                   "            Submits a file to the job queue\n"
                   "        nthreads: \n"
                   "            usage: nthreads <number of threads>\n"
                   "            starts x worker threads to process the jobs\n"
                   "            CAN ONLY BE CALLED ONCE PER JOBSCHED RUN\n"
                   "        list: \n"
                   "            usage: list\n"
                   "            lists the jobs and their data\n"
                   "        wait: \n"
                   "            usage: wait <jobid>\n"
                   "            waits for the job with the specified jobid \n"
                   "        waitall:\n"
                   "            usage: waitall\n"
                   "            blocks untill all jobs are done\n"
                   "        delete:\n"
                   "            usage: delete <jobid>\n"
                   "            deletes the specified job and the corresponding output file\n"
                   "            WILL NOT DELETE FILES THAT ARE IN THE RUNNIGN STATE\n"
                   "        schedule:\n"
                   "            usage: schedule <fcfs|sjf|balanced>\n"
                   "            selects the scheduling algorithm\n"
                   "        quit:\n"
                   "            usage: quit\n"
                   "            gracefully exits\n");
        }

        // unknown command
        else {
            printf("jobsched: command \"%s\" not found. Try \"help\".\n", input);
        }
    }

    delete_queue(queue);
    free(input);
    free(words);
}
