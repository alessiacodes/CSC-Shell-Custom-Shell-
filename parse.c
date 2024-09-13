/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"
#include <ctype.h>
#include <math.h>

#define CONTINUE_SEARCH NULL

/* HELPERS */
/**
 * @brief a node for linked list data 
 * 
 */
typedef struct Node {
    int data;
    struct Node *next;
} Node;
    /**
     * @brief frees all memory allocated for the linked list    
     * 
     * @param first_element points to the first element of the linked list
     */
    void free_linked_list(Node *first_element){
        Node *curr = first_element;
        Node *next;
        while (curr != NULL){
            next = curr->next;
            free(curr);
            curr = next;
        }
    }

/**
 * @brief: Finds the variable in the linked list with the given variable name
 * 
 * @param variables: A pointer to the struct pointer which is the front of the list
 * @param var_name: The name of the variable to search for
 * @return Variable*: returns the variable struct if found, NULL if not found.
 */
Variable *search_for_var(Variable **variables, char *var_name){
    Variable *requested_var = NULL;
    Variable *curr = *variables;

    while (curr != NULL){
        if (strcmp(curr->name, var_name) == 0){
            requested_var = curr;
            break;
        }
        curr = curr->next;
    }

    return requested_var;
}

/**
 * @brief This function is used to help with parse_line, where we have found the location
 * of an equals sign, and want to load the variable into our variables linked list. Also
 * checks for validity of syntax.
 * 
 * @param line: the given command line.
 * @param variables: pointer to the head of the linked list
 * @param i: the index of @param line where the equals sign was found
 * @return int: returns 0 if succesfully added, -1 if failed
 */
int retrieve_variable(char *line, Variable **variables, int i){
    // Error checking, check that there is no space on either side of the equals sign
    if (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t'){
        #ifdef DEBUG
            ERR_PRINT(ERR_VAR_START);
        #endif
        return -1;
    }

    // Check to see if valid variable name, loop backwards until you see some kind of space
    int final_char_index = 0;
    for (int j = i - 1; j >= 0; j--){
        if (line[j] == ' ' || line[j] == '\t'){
            final_char_index = j;
            break;
        }
        else if (!isalpha(line[j]) && line[j] != '_'){
            #ifdef DEBUG
                ERR_PRINT(ERR_VAR_NAME, &(line[j]));
            #endif
            return -1;
        }
    }

    // Load variable name
    // TODO: malloc check
    int length = i - final_char_index;
    char * var_name = malloc(sizeof(char) * (length + 1));
    strncpy(var_name, line + final_char_index, length);
    var_name[length] = '\0';

    // Load value until null terminator
    int value_length = strlen(line + i); // Note: this should work b/c we include 1 extra for \0
    // TODO: malloc check
    char * var_value = malloc(sizeof(char) * (value_length));
    var_value = strncpy(var_value, line + i + 1, value_length);
    var_value[value_length] = '\0';

    // Build variable struct
    // TODO: malloc check
    Variable *new_var = malloc(sizeof(Variable));
    new_var->name = var_name;
    new_var->value = var_value;
    new_var->next = NULL;

    // Add to variables list
    Variable *search = search_for_var(variables, var_name); 
    if (search != NULL){ // Var already exists
        free(search->value);
        search->value = var_value;
    }
    else{ //insert new variable
        if (variables == NULL){
            variables = &new_var;
            new_var->next = NULL;
        }
        else if (strcmp(var_name, "PATH") == 0){
            Variable * old_head = *variables;
            *variables = new_var;
            new_var->next = old_head;
        }
        else { // Just append as 2nd element so you never accidentally replace the head.
            Variable *old_2nd = (*variables)->next;
            (*variables)->next = new_var;
            new_var->next = old_2nd;
        }
    }

    if (strlen(var_value) == 0){
        return -1;
    }
    return 0;
}

void make_command_default_null(Command *commands){
    commands->exec_path = NULL;
    commands->args = NULL;
    commands->next = NULL;
    commands->redir_append = 0;
    commands->redir_out_path = NULL;
    commands->stdin_fd = 0;
    commands->redir_in_path = NULL;
    commands->stdout_fd = 0;
}

void debug_parse_commands(Command *commands){
    if (commands == NULL){
        return;
    }
    else if (commands->next == NULL){

        printf("exec: %s\n",commands->exec_path);
        int i = 0;
        while(commands->args !=NULL && (commands->args)[i] != NULL){
            printf("%d\n", i);
            printf("%s\n", (commands->args)[i]);
            i++;
        }

        printf("redir_app: %d\n", commands->redir_append);
        printf("redir_app: %d\n", commands->redir_append);
        printf("out redir: %s\n",commands->redir_out_path);
        printf("in redir: %s\n",commands->redir_in_path);
    }
    else{
        debug_parse_commands(commands->next);
        printf("exec: %s\n",commands->exec_path);
        int i = 0;
        while(commands->args !=NULL && (commands->args)[i] != NULL){
            printf("%d\n", i);
            printf("%s\n", (commands->args)[i]);
            i++;
        }

        printf("redir_app: %d\n", commands->redir_append);
        printf("redir_app: %d\n", commands->redir_append);
        printf("out redir: %s\n",commands->redir_out_path);
        printf("in redir: %s\n",commands->redir_in_path);
    }
}

/**
 * @brief Loads a single command into a Command object given a section of a line
 * 
 * @param section: the section of the line we're working with  
 * @param commands: the command object we're loading into
 * @return int: returns 0 on success, -1 on failure
 */
int load_single_command(char *section, Command *command, Variable **variables){
    int num_args = 0;
    char **args = NULL;
    char *curr_arg = NULL;
    char *out_redir = NULL;
    char *in_redir = NULL;
    int mode = 0; // mode 0 = load into args, mode 1 = load into in_redir, mode 2 = load into out

    for (int i = 0; i < strlen(section); i++){
        if (section[i] == '<'){
            num_args = 0; 
            mode = 1;
            in_redir = malloc(sizeof(char));
        }
        else if (section[i] == '>'){
            num_args = 0; 
            mode = 2;
            out_redir = malloc(sizeof(char));
            if (i < strlen(section) - 1 && section[i + 1] == '>'){
                i++;
                command->redir_append = 1;
            }
        }

        else if (section[i] == ' ' || section[i] == '\t'){
            curr_arg = NULL;
            
            // Skip all whitespaces
            while(i < strlen(section) - 1 && (section[i + 1] == ' ' || section[i + 1] == '\t')){
                i++;
            }
            if (strlen(section) - 1 == i){
                break;
            }
            // If we make it here, we've found a non-space character, prep to add a new argument
            if (section[i + 1] != '>' && section[i + 1] != '<'){
                num_args += 1;
            }
            
            // First we need to check if we're in a compatible mode for loading in another argument
            // We've loaded no arguments, and we're not loading redirections
            if (args == NULL && mode != 1 && mode != 2){
                //todo malloc check
                args = malloc(sizeof(char *) * 2);
                args[1] = NULL;
            }
            // We've loaded arguments, so need to make space for 1 more (and we're not loading redirections)
            else if (mode != 1 && mode != 2){
                args = realloc(args, sizeof(char *) * (num_args + 1));
                args[num_args] = NULL;
            }
            // We're in the mode of loading redirections, but more than 1 argument appears
            // else if (num_args > 1 && (section[i] != '>' && section[i]) != '<'){
            //     ERR_PRINT("More than 1 argument given to redirection operator.\n");
            //     return -1;
            // }
        }

        else { // We have found nothing interesting and should just load what we found into the current arg

            if (args == NULL){
                //todo malloc check
                args = malloc(sizeof(char *) * 2);
                args[1] = NULL;
            }
            else if (mode != 1 && mode != 2){
                args = realloc(args, sizeof(char *) * (num_args + 1));
                args[num_args] = NULL;
            }

            if (curr_arg == NULL){
                curr_arg = malloc(sizeof(char)*2);
                curr_arg[0] = section[i];
                curr_arg[1] = '\0';
            }

            else{ // make sure args is null terminated
                int curr_length = strlen(curr_arg);
                curr_arg = realloc(curr_arg, curr_length + 2);
                curr_arg[curr_length] = section[i];
                curr_arg[curr_length + 1] = '\0';
            }
            
            if(mode == 0 && num_args == 0){
                num_args += 1;
            }
            
            if (mode == 0){
                args[num_args - 1] = curr_arg;
            }

            else if (mode == 1){
                if (num_args > 1){
                    #ifdef DEBUG
                        ERR_PRINT("More than 1 arg for input redirection.\n");
                    #endif
                    return -1;
                }
                in_redir = curr_arg;
            }

            else if (mode == 2){
                if (num_args > 1){
                    #ifdef DEBUG
                        ERR_PRINT("More than 1 arg for output redirection.\n");
                    #endif
                    return -1;
                }
                out_redir = curr_arg;
            }
        }
        
    }
    command->args = args;
    if (args != NULL){
        char *exceutable = resolve_executable(args[0], *variables);
        #ifdef DEBUG
            printf("%s\n", exceutable);
        #endif
        if(exceutable == NULL){
            return -1;
        }
        else{
            command->exec_path = exceutable;
        }
    }
    command->redir_in_path = in_redir;
    command->redir_out_path = out_redir;
    command->stdin_fd = STDIN_FILENO;
    command->stdout_fd = STDOUT_FILENO;

    return 0;
}

/**
 * @brief 
 * 
 * @param line 
 * @param variables 
 * @param commands 
 * @return int 
 */
int load_commands(char *line, Variable **variables, Command *commands){
    char *toksave1;
    char *toksave2;

    // TODO: make sure u null terminate args
    //discard anything after comments
    char *delim = "#";
    char *section = strtok_r(line, delim, &toksave1); // the section we are going to continue to work with.

    char *token = strtok_r(section, "|", &toksave2);
    while (token != NULL){
        #ifdef DEBUG
            printf("token: %s\n", token);
        #endif
        // Determine if need to make a new Command object
        Command * curr_command;
        if (commands->exec_path == NULL){
            curr_command = commands;
        }
        else{
            curr_command = malloc(sizeof(Command));
            make_command_default_null(curr_command);

            Command *traverse = commands;
            // Add new commands to list
            while (traverse->next != NULL){
                traverse = traverse->next;
            }   
            traverse->next = curr_command;
        }
        
        if (load_single_command(token, curr_command, variables) < 0){
            // free_command(commands); TODO, get this to work
            return -1;
        }

        token = strtok_r(NULL, "|", &toksave2);
    }
    
    return 0;
}

/* OFFICIAL ASSIGNMENT FUNCTIONS */

// COMPLETE
char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}

Command *parse_line(char *line, Variable **variables){
    line = replace_variables_mk_line(line, *variables);
    if (line == NULL){
        return (Command *) -1;
    }

    // store indexes of special characters
    int equal_loc = 2147483647;
    int comment_loc = 2147483647;
    Node * in_redir_loc = NULL;
    Node * out_redir_loc = NULL;
    Node * out_app_redir_loc = NULL;
    Node * pipe_loc = NULL;
    Command *commands = NULL;


    // search for existence of special characters to determine how we're going to handle this line
    for (int i = 0; i < strlen(line); i++){
        // If we see an =, -> we have to deal with variable assignment
        if (line[i] == '#'){
            comment_loc = i;
            break; // Note: we return here b/c once we see an =, theres nothing left to parse.
        }
        // if we see a #, we know a comment follows and can therefore ignore it
        else if (line[i] == '='){
            equal_loc = i;
            break;
        }
        else if (line[i] == '>'){
            // If we notice its an append output redir, we will mark it accordingly.
            if (i < strlen(line) - 1 && line[i + 1] == '>'){
                Node *old_head = out_app_redir_loc;
                Node *new_out_app_redir = malloc(sizeof(Node));
                new_out_app_redir->data = i;
                new_out_app_redir->next = old_head;
                out_app_redir_loc = new_out_app_redir;
                i++;
            }
            else{
                Node *old_head = out_redir_loc;
                Node *new_out_redir = malloc(sizeof(Node));
                new_out_redir->data = i;
                new_out_redir->next = old_head;
                out_redir_loc = new_out_redir;
            }
        }
        else if (line[i] == '<'){
            Node * old_head = in_redir_loc;
            Node *new_in_redir = malloc(sizeof(Node));
            new_in_redir->data = i;
            new_in_redir->next = old_head;
            in_redir_loc = new_in_redir;
        }

        else if (line[i] == '|'){
            Node *old_head = pipe_loc;
            Node *new_pipe = malloc(sizeof(Node));
            new_pipe->data = i;
            new_pipe->next = old_head;
            pipe_loc = new_pipe;
        }
    }

    /* Check validity of special characters*/
    // There exists a variable assignment, and it occurs before any comment location
    if (equal_loc != 2147483647 && equal_loc < comment_loc){
        if (retrieve_variable(line, variables, equal_loc) == -1){
            // TODO: free most recent variable?
            free_command(commands);
            commands = (Command *) -1;
            return commands;
        }
        return NULL; // Note: we return here b/c once we see an =, theres nothing left to parse.
    }
    // if we spot a # immendiately, we can return.
    else if (comment_loc == 0){
        return NULL;
    }
    // Clearly some kind of command or blank, we will handle accordingly
    else{
        if (out_app_redir_loc != NULL && (out_app_redir_loc->next != NULL || out_redir_loc != NULL)){
            commands = (Command *) -1;
        }
        else if (out_redir_loc != NULL && (out_redir_loc->next != NULL || out_app_redir_loc != NULL)){
            commands = (Command *) -1;
        }
        else if (in_redir_loc != NULL && in_redir_loc->next != NULL){
            commands = (Command *) -1;
        }
        else if (in_redir_loc != NULL && pipe_loc != NULL && in_redir_loc->data > pipe_loc->data){
            commands = (Command *) -1;
        }
        else if (out_redir_loc != NULL && pipe_loc != NULL && out_redir_loc->data < pipe_loc->data){
            commands = (Command *) -1;
        }
        else if (out_app_redir_loc != NULL && pipe_loc != NULL && out_app_redir_loc->data < pipe_loc->data){
            commands = (Command *) -1;
        }
    }

    // account for if more than 1 arg is given to a redirection operator
    // TODO mallocc error
    if (commands != (Command *) -1){
        commands = malloc(sizeof(Command));
        make_command_default_null(commands);
    }

    if (commands != (Command *) -1 && load_commands(line, variables, commands) < 0){
        // TODO use free commands to free all recent commands that we tried to parse.
        commands = (Command *) -1;
    }

    if (commands != (Command *) -1 && commands->args == NULL){
        return NULL;
    }

    #ifdef DEBUG
        debug_parse_commands(commands);
    #endif

    // free everything used
    free_linked_list(in_redir_loc);
    free_linked_list(out_app_redir_loc);
    free_linked_list(out_redir_loc);
    free_linked_list(pipe_loc);

    return commands;
}


/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line,
                                Variable *variables){

    char *new_line = malloc(sizeof(char) * (strlen(line) + 1));
    strcpy(new_line, line);
    new_line[strlen(line)] = '\0';

    char *occurrence = strchr(new_line, '$');
    while (occurrence != NULL){

        int start_of_var = occurrence - new_line + 1;
        int end_of_var = 0;
        int front_bracket_found = 0;
        int back_bracket_found = 0;

        // Get variable name
        for (int i = occurrence - new_line + 1; i < strlen(new_line); i++){
            if (new_line[i] == '{'){
                // If { doesn't occur immediately after $ or occurs @ end of string
                if (i != occurrence - new_line + 1 || i + 1 >= strlen(line)){
                    #ifdef DEBUG
                        ERR_PRINT("Invalid variable syntax: unambiguous case\n");
                    #endif
                    free(new_line);
                    return NULL;
                }
                start_of_var = i + 1;
                front_bracket_found = 1;
            }

            else if (new_line[i] == '}'){
                // If placement of doesn't make sense, i.e. must be at index 3 at minimum to
                // make space for proper syntax, like so: ${x}
                // Also must have have found the first bracket.
                if (i < 3 && front_bracket_found == 0){
                    #ifdef DEBUG
                        ERR_PRINT("Invalid variable syntax: unambiguous case\n");
                    #endif
                    free(new_line);
                    return NULL;
                }
                back_bracket_found = 1;
                end_of_var = i - 1;
                break;
            }

            else if (i > 0 && !isalpha(new_line[i]) && new_line[i] != '_'){
                end_of_var = i - 1;
                break;
            }

            else if (i == strlen(new_line) - 1){
                end_of_var = i;
            }
        }

        // Miscellaneous error checks
        // If either both were found or both were not
        if (!((front_bracket_found && back_bracket_found) || (!front_bracket_found && !back_bracket_found))){
            #ifdef DEBUG
                ERR_PRINT("Invalid variable syntax: missing open/close curly brace.\n");
            #endif
            free(new_line);
            return NULL;
        }

        // If the end of the variable is the same as where we found $, we must not
        // have inputted a meaningful variable name
        if (end_of_var == occurrence - new_line){
            #ifdef DEBUG
                ERR_PRINT("Invalid variable syntax: empty or invalid variable name\n");
            #endif
            free(new_line);
            return NULL;
        }

        // extract the name
        char var_name[end_of_var - start_of_var + 1];
        strncpy(var_name, new_line + start_of_var, end_of_var - start_of_var + 1);
        var_name[end_of_var - start_of_var + 1] = '\0';

        // see if var exists
        Variable *actual_var = search_for_var(&variables, var_name);
        if (actual_var == NULL){
            #ifdef DEBUG
                ERR_PRINT("Variable '%s' does not exist.\n", var_name);
            #endif
            free(new_line);
            return NULL;
        }

        // edit the line
        int new_line_length = strlen(new_line) - (strlen(var_name)) + strlen(actual_var->value);
        char * copy_of_new_line = strdup(new_line); // have a copy of the new_line so we can take pieces of it
        int first_chunk = occurrence - new_line;
        int middle_chunk = strlen(actual_var->value);
        int last_chunk;
        // curly brace was last character
        if (back_bracket_found == 1 && end_of_var + 1 == strlen(new_line) - 1){
            last_chunk = 0;
        }
        else if (back_bracket_found){
            last_chunk = strlen(new_line) - end_of_var - 2;
        }
        else{
            last_chunk = strlen(new_line) - end_of_var - 1;
        }
        new_line = realloc(new_line, sizeof(char) * new_line_length);
        memset(new_line, '\0', strlen(new_line));
        strncpy(new_line, copy_of_new_line, first_chunk);
        strncpy(new_line + first_chunk, actual_var->value, middle_chunk);
        strncpy(new_line + first_chunk + middle_chunk, copy_of_new_line + (strlen(copy_of_new_line) - last_chunk), last_chunk);
        new_line[new_line_length - 1] = '\0';
        free(copy_of_new_line);
        occurrence = strchr(new_line, '$');
    } 
    #ifdef DEBUG
        printf("%s\n", new_line);
    #endif
    return new_line;
}


void free_variable(Variable *var, uint8_t recursive){
    // Recursive option
    if (recursive != 0){
        if (var == NULL){
            return;
        }
        else if (var->next == NULL){
            free(var->name);
            free(var->value);
            free(var);
        }
        else{
            free_variable(var->next, recursive);
            free(var);
        }
    }
    
    // Non-recursive option
    else{
        if (var == NULL){
            return;
        }
        else{
            free(var->name);
            free(var->value);
            free(var);
        }
    }
}
