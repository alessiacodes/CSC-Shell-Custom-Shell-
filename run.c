/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"


// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}


int *execute_line(Command *head){
    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif
    int *exit_code = malloc(sizeof(int));
    *exit_code = 0;
    pid_t pid;
    int *pid_list = NULL;
    int num_pids = 0;
     
    Command * curr = head;
    if (curr == NULL){
        free(exit_code);
        return NULL;
    }
    else if (curr->next == NULL){
        // check for cd explicitly
        
        if (strcmp(curr->args[0], "cd") == 0){\
            pid = cd_cscshell(curr->args[1]);
            *exit_code= pid;
            return exit_code;
        }
        else{
            pid = run_command(curr);
            if (pid == -1) {
                // Handle fork error
                *exit_code = -1;
                return exit_code;
            } 
            else {
                // Wait for the child process to terminate
                int status;
                if (waitpid(pid, &status, 0) == -1) {
                    perror("waitpid");
                    // Handle waitpid error
                    *exit_code = -1;
                    return exit_code;
                }
                if (WEXITSTATUS(status) == -1){
                    ERR_PRINT(ERR_EXECUTE_LINE);
                    *exit_code = -1;
                    return exit_code;
                }
                *exit_code = WEXITSTATUS(status);
                return exit_code;
            }
        }
        
    }
    else{
        while (curr != NULL){
            // if we are not the last command, we need to make any more pipes.
            if (curr->next != NULL){
                int fd[2];
                if (pipe(fd) == -1) {
                    perror("pipe");
                }
                curr->next->stdin_fd = fd[0];
                curr->stdout_fd = fd[1];
            }
            
            pid = run_command(curr);
            if (pid == -1) {
                // Handle fork error
                *exit_code = -1;
                return exit_code;
            } 
            num_pids++;
            pid_list = realloc(pid_list, num_pids);
            pid_list[num_pids - 1] = pid;
            curr = curr->next;
        }

        #ifdef DEBUG
            printf("All children created\n");
        #endif

        int status;
        for (int i = 0; i < num_pids; i++){
            // Wait for the child process to terminate
            
            if (waitpid(pid_list[i], &status, 0) == -1) {
                perror("waitpid");
                // Handle waitpid error
                *exit_code = -1;
                return exit_code;
            }   
            if (WEXITSTATUS(status) == -1){
                ERR_PRINT(ERR_EXECUTE_LINE);
                *exit_code = -1;
                return exit_code;
            }

        }
        *exit_code = WEXITSTATUS(status);
        #ifdef DEBUG
        printf("All children finished\n");
        #endif
        free(head);
        return exit_code;    
    }

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif

    return NULL;
}


/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){
    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           command->stdin_fd, command->stdout_fd);
    #endif
    
    int pid = fork();
    if (pid > 0) {
        if (command->stdin_fd != STDIN_FILENO){
            close(command->stdin_fd);
        }
        
        if (command->stdout_fd != STDOUT_FILENO){
            close(command->stdout_fd);
        }

    } else if (pid == 0) {
        if (command->stdin_fd != STDIN_FILENO){
            dup2(command->stdin_fd, STDIN_FILENO);
            close(command->stdin_fd);
        }
        if (command->stdout_fd != STDOUT_FILENO){
            dup2(command->stdout_fd, STDOUT_FILENO);
            close(command->stdout_fd);
        }

        if (command->redir_in_path != NULL){
            int in_fd = open(command->redir_in_path, O_RDONLY); // change to cmd stdin
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if (command->redir_out_path != NULL){
            if(command->redir_append == 1){
                int out_fd = open(command->redir_out_path, O_APPEND);
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            else{
                int out_fd = open(command->redir_out_path, O_WRONLY);
                dup2(out_fd, command->stdout_fd);
                close(out_fd);
            }
        }
        
        execv(command->exec_path, command->args);
    } else {
        perror("fork");
        return -1;
    }

    #ifdef DEBUG
    //printf("Parent process created child PID [%d] for %s\n", pid, command->exec_path);
    #endif
    return pid;
}

int run_script(char *file_path, Variable **root){
    long error = 0;
    char line[MAX_SINGLE_LINE];
    FILE *directory;
    
    // Put the path as the head of the linked list
    Variable *path = malloc(sizeof(Variable));
    path->name = malloc(sizeof(char) * 5);
    strcpy(path->name, "PATH\0");
    int value_length = strlen(file_path) + 1;
    // TODO: malloc check
    char * var_value = malloc(sizeof(char) * value_length);
    var_value = strncpy(var_value, file_path, value_length - 1);
    var_value[value_length] = '\0';
    path->value = var_value;
    path->next = NULL;
    *root = path;

    // Open file
    directory = fopen(file_path, "r");
        if (directory == NULL){
            ERR_PRINT(ERR_BAD_PATH, file_path);
            return -1;
        }
    
    while (fgets(line, MAX_SINGLE_LINE, directory) != NULL) {
        // kill the newline
        line[strlen(line) - 1] = '\0';

        Command *commands = parse_line(line, root);
        if (commands == (Command *) -1){
            ERR_PRINT(ERR_PARSING_LINE);
            error = -1;
            continue;
        }
        if (commands == NULL) continue;
        int *last_ret_code_pt = execute_line(commands);
        if (last_ret_code_pt == (int *) -1){
            ERR_PRINT(ERR_EXECUTE_LINE);
            free(last_ret_code_pt);
            return -1;
        }
        free(last_ret_code_pt);
    }
    printf("\n");

    if (fclose(directory) != 0) {
        error = -1;
    }

    // 0 on EOF, -1 on other errors
    return (int) error;
}

void free_command(Command *command){
    if (command == NULL){
        return;
    }
    else if (command->next == NULL){
        free(command->exec_path);
        int i = 0;
        while ((command->args)[i] != NULL){
            free((command->args)[i]);
            i++;
        }
        free(command->args);
        free(command->redir_in_path);
        free(command->redir_out_path);
        #ifdef DEBUG
            printf("freed command 2\n");
        #endif
        free(command);

    }
    else{
        free_command(command->next);
        #ifdef DEBUG
            printf("freed command 1\n");
        #endif
        free(command);
    }
}
