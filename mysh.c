#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>


#define INPUT_SIZE 510
#define CUTTING_WORD " \n"
#define ENDING_WORD "exit"
#define RESET 0

char *getcwd(char *buf,size_t size);
void DisplayPath();
char **parseCommand(char *input,int *sizeOfArray);
void garbageFree(char **argv,int size);
char *combinedInfo;

void my_ps() {
    DIR *dp;
    struct dirent *entry;
    char path[1024];
    char stat_path[1024];
    FILE *stat_file;
    char stat_line[1024];
    
    dp = opendir("/proc");
    if (dp == NULL) {
        perror("Error");
        return ;
    }
    
    printf("%6s %6s %10s %s\n", "PID", "PPID", "STATUS", "CMD");
    
    while ((entry = readdir(dp))) {
        if (atoi(entry->d_name) != 0) {
            snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
            stat_file = fopen(path, "r");
            
            if (stat_file) {
                if (fgets(stat_line, sizeof(stat_line), stat_file)) {
                    char cmd[1024];
                    int pid, ppid;
                    char status;
                    sscanf(stat_line, "%d %s %c %d", &pid, cmd, &status, &ppid);
                    printf("%6d %6d %10c %s\n", pid, ppid, status, cmd);
                }
                fclose(stat_file);
            }
        }
    }
    
    closedir(dp);
}

void listDirectoryContents(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        perror("opendir");
        exit(1);
    }

    while ((entry = readdir(dp)) != NULL) {
        printf("%s  ", entry->d_name);
    }

    printf("\n");
    closedir(dp);
}

void my_ls(const char *target) {
    if (target == NULL) {
        listDirectoryContents(".");
    } else {
        struct dirent *entry;
        DIR *dp = opendir(target);

        if (dp != NULL) {
            listDirectoryContents(target);
            closedir(dp);
        } else {
            dp = opendir(".");
            int found = 0;

            if (dp != NULL) {
                while ((entry = readdir(dp)) != NULL) {
                    if (strcmp(entry->d_name, target) == 0) {
                        printf("%s\n", entry->d_name);
                        found = 1;
                        break;
                    }
                }

                closedir(dp);
            }

            if (!found) {
                fprintf(stderr, "No such file or directory: %s\n", target);
            }
        }
    }
}

void my_echo(char **message, int s){
    for(int i=1;i<=s;i++) printf("%s ", message[i]);
    printf("\n");
}

void my_cat(char *file_name) {
    int fd;
    char buffer[1024];
    
    if ((fd = open(file_name, O_RDONLY)) == -1) {
        perror("Error");
        return ;
    }
    
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        if (write(STDOUT_FILENO, buffer, n) != n) {
            perror("Error");
            close(fd);
            return ;
        }
    }
    
    close(fd);
}

void my_wc(char *file_name, char *flag){
    int fd;
    char buffer[1024];
    int line_count = 0;
    int word_count = 0;
    int char_count = 0;

    if ((fd = open(file_name, O_RDONLY)) == -1) {
        perror("Error");
        return ;
    }

    ssize_t n;
    int in_word = 0;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < n; i++) {
            char_count++;
            if (buffer[i] == '\n') {
                line_count++;
            }
            if (buffer[i] == ' ' || buffer[i] == '\n' || buffer[i] == '\t') {
                in_word = 0;
            } else if (in_word == 0) {
                in_word = 1;
                word_count++;
            }
        }
    }

    if(strcmp(flag,"-l")==0)printf("%d %s\n", line_count, file_name);
    else if(strcmp(flag,"-w")==0)printf("%d %s\n", word_count, file_name);
    else if(strcmp(flag,"-m")==0)printf("%d %s\n", char_count, file_name);

    close(fd);
}

void my_mkdir(char *dir) {
    if (mkdir(dir,0755) == -1){
        perror("Error");
        return;
    }
}

typedef struct {
    const char *name;
    const char *description;
} CommandInfo;

CommandInfo Commands[]={
    {"help", "Display all Supported commands.\nUsage: help [command]\n"},
    {"exit", "Exit the shell"},
    {"cd", "Change the current directory"},
    {"ls", "List files and directories"},
    {"ps", "List snapshot of processes or running processes"},
    {"echo", "Display a line of text"},
    {"cat", "Display the file data."},
    {"wc", "Print the word counts or print the character counts or print the newline counts of any file.\n"},
    {"mkdir", "Create the DIRECTORY(ies), if they do not already exist."},
};

CommandInfo Command[]={
    {"help", "[command]\nDisplay commands and its description.\n"},
    {"exit", "- Exit the shell.\n"},
    {"cd", "[foldername] or [path]\nChange the current shell working directory\n"},
    {"ls", "[FILE]...\nUsage2: ls | <command>\nList files and directories.\n"},
    {"ps", "[OPTION]... \nAvailable Options\n-e,  prints running processes.\nList processes\n"},
    {"echo", "[STRING]...\nDisplay a line of text\n"},
    {"cat", "[FILE]...\nDisplay the file data.\n"},
    {"wc", "[OPTION]... [FILE]...\nAvailable Options\n-w, --words            print the word counts\n-m, --chars            print the character counts\n-l, --lines            print the newline counts\n"},
    {"mkdir", "DIRECTORY...\nCreate the DIRECTORY(ies), if they do not already exist.\n"},
};


void CommandUsage(const char *commandName){
    for (int i=0;i<sizeof(Command)/sizeof(Command[0]);i++){
        if (strcmp(commandName,Command[i].name)==0) {
            printf("Usage: %s %s\n",Command[i].name,Command[i].description);
            return;
        }
    }
    fprintf(stderr,"Error: Command '%s' not found. Use 'help' for a list of supported commands.\n\n",commandName);
}

void CorrectUsage(const char *commandName){
    for (int i=0;i<sizeof(Command)/sizeof(Command[0]);i++) {
        if (strcmp(commandName, Command[i].name)== 0){
            fprintf(stderr,"Error: Incorrect usage of '%s'.\n", commandName);
            fprintf(stderr,"Correct usage: %s %s\n",Command[i].name,Command[i].description);
            return;
        }
    }
    fprintf(stderr, "Error: Command '%s' not found. Use 'help' for a list of supported commands.\n\n",commandName);
}

int main(){
    char *input,*path;
    DisplayPath();
    pid_t id;
    int fd[2];
    char **argv1,**argv2;
    int size1,size2;
    int original_stdout = dup(1);
    
    while(1){
        input=readline(combinedInfo);
        if(input==NULL) {
            printf("\n");
            break;
        }
        if(strlen(input)==0) {
            continue;
        }
        add_history(input);
        if(strcmp(input, ENDING_WORD)== RESET){
            printf("Exit\n");
            break;
        }
        
        if (strstr(input, "cat") && strstr(input, ">")) {
            char *cmd1 = strtok(input, ">");
            char *file2 = strtok(NULL, ">");
            file2 = strtok(file2, CUTTING_WORD); 
            int output_fd = open(file2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd == -1) {
                perror("Error");
                continue;
            }
            dup2(output_fd, STDOUT_FILENO);
            system(cmd1);
            dup2(original_stdout, STDOUT_FILENO);
            close(output_fd);
        } else if (strstr(input, "ps") && strstr(input, ">")) {
            char *cmd1 = strtok(input, ">");
            char *file2 = strtok(NULL, ">");
            file2 = strtok(file2, CUTTING_WORD); 
            int output_fd = open(file2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd == -1) {
                perror("Error");
                continue;
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
            input = cmd1;
        }
        
        if (strstr(input, "cat") && strstr(input, "|") || strstr(input, "ls") && strstr(input, "|")) {
            char *cmd1=strtok(input,"|");
            char *cmd2=strtok(NULL,"|");
            int size1,size2;
            char **argv1=parseCommand(cmd1, &size1);
            char **argv2=parseCommand(cmd2, &size2);
            int pipe_fd[2];
            if (pipe(pipe_fd)== -1) {
                perror("Pipe creation failed");
                exit(1);
            }
            pid_t id1,id2;
            id1=fork();
            if (id1 < RESET) {
                perror("Fork failed");
                exit(1);
            } else if (id1==RESET){
                close(pipe_fd[0]);  
                dup2(pipe_fd[1],STDOUT_FILENO);
                close(pipe_fd[1]);  
                if (execvp(argv1[0],argv1)== -1) {
                    fprintf(stderr,"Error: Command not found, Use 'help' for a list of supported commands.\n");
                    continue;
                }
            } else {
                id2 = fork();
                if (id2 < RESET) {
                    perror("Fork failed");
                    exit(1);
                } else if (id2== RESET) {
                    close(pipe_fd[1]);  
                    dup2(pipe_fd[0], STDIN_FILENO); 
                    close(pipe_fd[0]);  
                    if (execvp(argv2[0],argv2) == -1) {
                        fprintf(stderr,"Error: Command not found, Use 'help' for a list of supported commands.\n");
                        continue;
                    }
                } else {
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                    waitpid(id1,NULL,0);
                    waitpid(id2,NULL,0);
                    garbageFree(argv1,size1);
                    garbageFree(argv2,size2);
                }
            }
        }
        else{
        argv1=parseCommand(input,&size1);
        if(strcmp(argv1[0],"ls")==0){
            if (size1 > 2) {
                CorrectUsage("ls");
                garbageFree(argv1,size1); 
            }
            else if(size1==2)my_ls(argv1[1]);
            else if(size1==1)my_ls(NULL);
            continue;
        }
        if (strcmp(argv1[0],"ps")==0 && size1>1) {
            if (size1>2){
                CorrectUsage("ps");
                garbageFree(argv1, size1);
            }
            else{
             if(strcmp(argv1[1],"-e")==0) my_ps();
             else CorrectUsage(argv1[1]);
            }
            continue;   
        }
        if (strcmp(argv1[0],"help")==0) {
            if (size1 > 2) CorrectUsage("help");
            else if (size1 > 1) CommandUsage(argv1[1]);
            else {
                printf("Supported commands:\n");
                for (int i=0;i<sizeof(Commands)/sizeof(Commands[0]);i++)  printf("%s - %s\n",Commands[i].name,Commands[i].description);
            }
            garbageFree(argv1, size1);
            continue; 
        }
        if (strcmp(argv1[0],"exit") ==RESET) {
            printf("Exit\n");
            break;
        }
        if (strcmp(argv1[0],"cd") ==0){
            if (size1 > 2){
                CorrectUsage("cd");
                garbageFree(argv1,size1);
                continue;
            }
        }
        if (strcmp(argv1[0],"cd")==RESET) {
            struct passwd *pwd;
            char *path=argv1[1];
            if (path==NULL) {
                pwd=getpwuid(getuid());
                path=pwd->pw_dir;
            }
            if (path[0]=='/') path++;
            if (chdir(path)==-1)  perror("Error changing directory");
            DisplayPath();
            continue;
        }
        if (strcmp(argv1[0],"echo")==0) {
            if(strcmp(argv1[1],"|")==0 || strcmp(argv1[1],">")==0) {
                CorrectUsage("echo");
                garbageFree(argv1, size1);
            }
            else my_echo(argv1,size1-1);
            continue;
            
        }
        if (strcmp(argv1[0],"cat")==0) {
            if (size1>2){
                CorrectUsage("cat");
                garbageFree(argv1, size1);
            }
            else my_cat(argv1[1]);
            continue;
            
        }
        if (strcmp(argv1[0],"wc")==0) {
            if (size1>3){
                CorrectUsage("wc");
                garbageFree(argv1, size1);
            }
            else{
             if(strcmp(argv1[1],"-l")==0 || strcmp(argv1[1],"-w")==0 || strcmp(argv1[1],"-m")==0) my_wc(argv1[2], argv1[1]);
             else CorrectUsage("wc");
            }
            continue;   
        }
        if(strcmp(argv1[0],"mkdir")==0){
            if (size1 > 2) {
                CorrectUsage("mkdir");
                garbageFree(argv1,size1); 
            }
            else my_mkdir(argv1[1]);
            continue;
        }
        
        if (pipe(fd) ==-1) {
            perror("Pipe failed");
            exit(1);
        }
        id = fork();
        if (id< RESET){
            perror("Fork failed");
            exit(1);
        }
        else if(id == RESET){
            close(fd[0]);
            dup2(fd[1],STDOUT_FILENO);
            close(fd[1]);
            if (execvp(argv1[0], argv1)==-1){
                fprintf(stderr, "Error: Command not found, Use 'help' for a list of supported commands.\n");
                exit(1);
            }
        }
        else{
            waitpid(id,NULL,0);
            argv2 =parseCommand(input,&size2);
            id =fork();
            if (id< RESET) {
                perror("Fork failed");
                exit(1);
            }
            else if (id == RESET){
                close(fd[1]);
                dup2(fd[0],STDIN_FILENO);
                close(fd[0]);
                if (execvp(argv2[0],argv2) ==-1) {
                    fprintf(stderr,"Error: Command not found, Use 'help' for a list of supported commands.\n");
                    exit(1);
                }
            }
            else{
                waitpid(id,NULL,0);
                close(fd[0]);
                close(fd[1]);
                garbageFree(argv1,size1);
                garbageFree(argv2,size2);
            }
        }
        }dup2(original_stdout, 1);
    }
    close(original_stdout);
    return RESET;
}

void garbageFree(char **argv, int size) {
    for (int i=0;i<size;++i)  free(argv[i]);
    free(argv);
    argv = NULL;
}

char **parseCommand(char *input,int *sizeOfArray) {
    int i =RESET, counter=RESET;
    char inputCopy[INPUT_SIZE];
    strcpy(inputCopy,input);
    char *ptr=strtok(input,CUTTING_WORD);
    while (ptr !=NULL) {
        ptr=strtok(NULL,CUTTING_WORD);
        counter++;
    }
    char **argv=(char **)malloc((counter + 1) * sizeof(char *));
    if (argv==NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    char *ptrCopy=strtok(inputCopy,CUTTING_WORD);
    while (ptrCopy!=NULL) {
        argv[i]=(char *)malloc((sizeof(char)+1) * strlen(ptrCopy));
        if (argv[i] == NULL){
            fprintf(stderr,"Error: Memory allocation failed\n");
            for (int j=i-1;j>=0;j--)  free(argv[j]);
            free(argv);
            exit(1);
        }
        strcpy(argv[i], ptrCopy);
        argv[i][strlen(ptrCopy)] ='\0';
        ptrCopy =strtok(NULL, CUTTING_WORD);
        i++;
    }
    argv[counter] =NULL;
    (*sizeOfArray)=counter;
    return argv;
}

void DisplayPath() {
    char hostname[1024];
    char cwd[1024];
    char *username = getenv("USER"),path[2048];
    getcwd(cwd, sizeof(cwd));
    gethostname(hostname, 1024);
    int totalLength = strlen(username) + strlen(hostname) + strlen(cwd) + 5; 
    combinedInfo = (char *)malloc(totalLength);
     if (combinedInfo == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return;
    }
    snprintf(combinedInfo, totalLength, "%s@%s:%s$ ", username, hostname, cwd);
}

