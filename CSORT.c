#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "CSORT.h"

/**
 * Design strategy:
 * 1. Create shared memory and attach to shared memory
 * 2. Create and initialize semaphore
 * 3. Copy array of chars to lowercase into shared memory
 * 4. Fork 3 process and associate w/ an integer id
 * 5. Attach forked processes to shared memory
 * 6. 
 */

static int lower_sem_id, upper_sem_id;
static int process_num = 0;
static pid_t child_pids[3];
static int lower, upper;

union semun {
    int val;
    struct semi_ds *buf;
    unsigned short *array;
};

int main() {
    int num_inputs = 6;
    char manual_input[8];
    char test_inputs[][8] = {
        "XAzMWyD",
        "QHtbUSa",
        "Samuele",
        "Harvard",
        "Boomere",
        "Cabelas",
        "Doggggo",
        "AAAAAAA"
    };
    char expected_outputs[][8] = {
        "admwxyz",
        "abhqstu",
        "aeelmsu",
        "aadhrrv",
        "beemoor",
        "aabcels",
        "dggggoo",
        "aaaaaaa"
    };

    int i = 0;
    printf("Input string: ");
    scanf("%s", manual_input);
    printf("\n");
    concurrent_sort(manual_input);

    for (int i=0; i<num_inputs; i++) {
        printf("\n---TESTING INPUT %s---\n", test_inputs[i]);
        concurrent_sort(test_inputs[i]);
        printf("Expected output: %s\n", expected_outputs[i]);
        assert(strcmp(test_inputs[i], expected_outputs[i]) == 0);
    }
}

/**
 * Concurrently sorts an array of
 * 7 characters. 
 * 
 * @param AR    char[7], string of 7 letters
 */
void concurrent_sort(char AR[8]) {
    int shmid;
    struct shared_use_st *shared_memory = (void*)0;
    lower = 2;
    upper = 4;

    // Get semaphores
    lower_sem_id = semget((key_t)1234, 1, 0666 | IPC_CREAT);
    if (!set_semvalue(lower_sem_id))
    {
        fprintf(stderr, "Failed to initialize semaphore\n");
        exit(EXIT_FAILURE);
    }

    upper_sem_id = semget((key_t)1235, 1, 0666 | IPC_CREAT);
    if (!set_semvalue(upper_sem_id))
    {
        fprintf(stderr, "Failed to initialize semaphore\n");
        exit(EXIT_FAILURE);
    }

    // Create and attach shared memory for parent
    create_shared_memory(&shmid);
    attach_shared_memory(&shared_memory, shmid);

    // initialize shared mem
    for (int i=0; i<8; i++) {
        shared_memory->letters[i] = tolower(AR[i]);
    }
    //shared_memory->letters[7] = '\0';

    create_processes();
    if (process_num == 0) {
        // parent
        int is_sorted = 0;
        while (!is_sorted) {
            is_sorted = 1;
            semaphore_p(lower_sem_id);
            semaphore_p(upper_sem_id);
            for (int i=0; i<6; i++) {
                if (shared_memory->letters[i] > shared_memory->letters[i+1]) {
                    is_sorted = 0;
                }
            }
            semaphore_v(lower_sem_id);
            semaphore_v(upper_sem_id);
        }

        // Send kill signal to all children
        for (int i=0; i<3; i++) {
            kill(child_pids[i], SIGKILL);
        }

        // Copy shared mem letters to input memory
        printf("Input: %s\n", AR);
        for (int i=0; i<8; i++) {
            AR[i] = shared_memory->letters[i];
        }
        printf("Output: %s\n", AR);

        del_semvalue(lower_sem_id);
        del_semvalue(upper_sem_id);
        detach_shared_memory(shared_memory);
        delete_shared_memory(shmid);
        
        return;
    }
    else {
        // children
        while(1) {
            sort_subset(shared_memory->letters, lower, upper);
        }
    }


}

/**
 * Sorts the subset in the array
 * between the lower & upper (inclusive)
 * 
 * @param AR 
 * @param lower 
 * @param upper 
 */
void sort_subset(char AR[8], int lower, int upper) {
    /**
     * Insertion sort pseudocode:
     * for i in range(lower, upper+1):
     *      j = i
     *      while j > lower and A[j-1] > A[j]:
     *          swap A[j] and A[j-1]
     *          j = j - 1
     */
    // shouldn't i start at lower + 1
    for (int i=lower; i<=upper; i++) {
        int j = i;
        // Critical region start if j == 
        //while (j > lower && AR[j-1] > AR[j]) {
        while (j > lower) {
            //int swap_occurred = 0;
            int involves_lower_crit = involves_critical_element(j, lower);
            int involves_upper_crit = involves_critical_element(j, upper);
            if (involves_lower_crit) {
                // P() semaphore for lower crit
                semaphore_p(lower_sem_id);
            } else if (involves_upper_crit) {
                // P() semaphore for upper crit
                semaphore_p(upper_sem_id);
            }
            // Swap AR[j] & A[j-1]
            if (AR[j-1] > AR[j]) {
#ifdef DEBUG_ENABLED
                printf("Process P%d: performed swapping\n", process_num);
#endif
                swap_elements(AR, j-1, j);
            } else {
#ifdef DEBUG_ENABLED
                if (DEBUG_ENABLED) {
                    printf("Process P%d: No swapping\n", process_num);
                }
#endif
            }

            if (involves_lower_crit) {
                // V() semaphore for lower crit
                semaphore_v(lower_sem_id);
            } else if (involves_upper_crit) {
                // V() semaphore for upper crit
                semaphore_v(upper_sem_id);
            }

            j--;
        }
        // Critical region end
    }

}

/**
 * Returns 1 if index involves
 * critical elements in shared 
 * memory.
 * 
 * @param index     int, index
 * @param lower     int, lower critical index
 * @param upper     int, upper critical index
 * @return          int, index involves critical element
 */
int involves_critical_element(int index, int critical_element) {
    return (index == critical_element) || (index - 1 == critical_element);
}

/**
 * Swaps elements x and y in array
 * AR.
 * 
 * @param AR    char*, array of characters
 * @param x     int, index of element to be swapped
 * @param y     int, index of element to be swapped
 */
void swap_elements(char AR[7], int x, int y) {
    char temp = AR[x];
    AR[x] = AR[y];
    AR[y] = temp;
}

/**
 * Forks three child processes,
 * where each process' global compute_id
 * value is set to the the value of id.
 * 
 * @param   none
 * @return  none
 * */
void create_processes()
{
    pid_t pid;
    for (int i=1; i<=3; i++) {
        pid = fork();
        switch (pid) {
            case -1:
                // fork fails
                perror("fork failed");
                exit(EXIT_FAILURE);
                break;
            case 0:
                // is child
                process_num = i;
                lower = (process_num-1) * 2;
                upper = (process_num) * 2;
                return;
            default:
                // is parent 
                child_pids[i-1] = pid;
                break;
        }
    }

    return;
}

/**
 * Creates shared memory.
 * Exits on failure.
 * 
 * @param   int*, shared memory id pointer
 * @return  none
 * */
void create_shared_memory(int *shmid)
{
    // Create shared memory
    //int shmid
    *shmid = shmget((key_t)1245, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    if (*shmid == -1)
    {
        // Exit if shared memory creation fails
        fprintf(stderr, "%d:shmget failed\n", process_num);
        exit(EXIT_FAILURE);
    }
}

/**
 * Attaches shared memory to the
 * address space of current process.
 * Exits on failure.
 * 
 * @param   int**, pointer to shared memory pointer
 * @param   int, shared memory id
 * @return  none
 * */
void attach_shared_memory(struct shared_use_st **shared_memory, int shmid)
{
    // Attach shared memory to address space of all processes
    *shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1)
    {
        fprintf(stderr, "%d:shmat failed\n", process_num);
        exit(EXIT_FAILURE);
    }
    //printf("%d: Memory attached at %X\n", process_num, (int)shared_memory);
}

/**
 * Detaches shared memory segment from
 * current process.
 * Exits on failure.
 * 
 * @param   int*, shared_memory pointer
 * @return  none
 * */
void detach_shared_memory(struct shared_use_st *shared_memory)
{
    if (shmdt(shared_memory) == -1)
    {
        fprintf(stderr, "%d:shmdt failed\n", process_num);
        exit(EXIT_FAILURE);
    }
    //printf("%d: shmdt success\n", process_num);
}

/**
 * Deletes shared memory segment.
 * Exits on failure.
 * 
 * @param   int, shared memory id
 * @return  none
 * */
void delete_shared_memory(int shmid)
{
    if (shmctl(shmid, IPC_RMID, 0) == -1)
    {
        fprintf(stderr, "%d:shmctl(IPC_RMID) failed\n", process_num);
        exit(EXIT_FAILURE);
    }
    //printf("%d:shared mem delete success\n", process_num);
}

static int set_semvalue(int sem_id)
{
    union semun sem_union;
    sem_union.val = 1;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1)
        return (0);
    return (1);
}

static void del_semvalue(int sem_id)
{
    union semun sem_union;
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}

static int semaphore_p(int sem_id)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = -1; /* P() */
    sem_b.sem_flg = SEM_UNDO;
    //printf("\n%d:Waiting...\n", process_num);
    if (semop(sem_id, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_p failed\n");
    }
    //printf("%d:Critical section ENTERED!\n", process_num);
    return (1);
}

static int semaphore_v(int sem_id)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1; /* V() */
    sem_b.sem_flg = SEM_UNDO;
    //printf("\nLeaving critical section!\n");
    if (semop(sem_id, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_v failed\n");
        return (0);
    }
    //printf("\n%d:Critical section EXITED!\n", process_num);
    return (1);
}
