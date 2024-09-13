#include "cscshell.h"

int main(int argc, char *argv[]){
    char *command = "hello hi 3";
    Command *commands = parse_line(command, NULL);
    printf("%s\n", commands->exec_path);
    printf("%s\n", (commands->args)[0]);
    printf("%s\n", (commands->args)[1]);
    printf("%s\n", (commands->args)[2]);
}
