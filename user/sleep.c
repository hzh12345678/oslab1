#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(2, "Wrong usuge! Usuge of sleep : sleep num\n");//output wrong message
        exit(1);
    }
    int time = atoi(argv[1]);//get time = argv[1] 
    sleep(time);
    exit(0);
}