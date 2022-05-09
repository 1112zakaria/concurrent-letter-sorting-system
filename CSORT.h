#ifndef CSORT_H
#define CSORT_H

    struct shared_use_st {
        char letters[8];
        //idk
    };

    void concurrent_sort(char[8], int);
    void sort_subset(char[8],int,int);
    void swap_elements(char[7], int, int);
    int involves_critical_element(int,int);
    void create_processes(void);
    void create_shared_memory(int*);
    void attach_shared_memory(struct shared_use_st**,int);
    void detach_shared_memory(struct shared_use_st*);
    void delete_shared_memory(int);
    static int set_semvalue(int);
    static void del_semvalue(int);
    static int semaphore_p(int);
    static int semaphore_v(int);
#endif
