#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    int pid;
    int pipe1[2], pipe2[2];
    char buf[] = {'a'};
    pipe(pipe1);//father to child
    pipe(pipe2);//child to father
    int ret = fork();
    if ( ret == 0 ) {
        pid = getpid();//get process id
        close(pipe1[1]);
        close(pipe2[0]);
        read(pipe1[0], buf, 1);
        close(pipe1[0]);
        printf("%d: received ping\n", pid);
        write(pipe2[1], buf, 1);
        close(pipe2[1]);
    } 
    else {
        pid = getpid();
        close(pipe2[1]);
        close(pipe1[0]);
        write(pipe1[1], buf, 1);
        close(pipe1[1]);
        read(pipe2[0], buf, 1);
        close(pipe2[0]);
        printf("%d: received pong\n", pid);
    }
    exit(0);
}