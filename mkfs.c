#include <stdio.h>
#include "disk.h"
#include "minithread.h"

int disk_init(int* arg) {
    //TODO initialize filesystem
    printf("Let's set up the filesystem\n");
}

int main(int argc, char** argv) {

    use_existing_disk = 0;
    disk_name = "disk0";
    disk_flags = DISK_READWRITE;
    disk_size = 1000;
    
    minithread_system_initialize(&disk_init, NULL);
    
}