#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef struct StrWord {
    char *word;
    int memory, length;
} SWord;

typedef struct StrCommand {
    char **command;
    int is_background;
    char *file_in;
    char *file_out;
    int amount_words;
    int redirect_in, redirect_out; // in - def:0, <:1  |  out - def:0, >:1, >>:2
} SCom;

typedef struct StrPipe {
    SCom *com;
    int is_empty, number, is_last;
    
} SPipe;


void InitWord(SWord *tmp) {
    tmp->word = (char *) malloc(2);
    tmp->word[0] = '\0';
    tmp->memory = 2;
    tmp->length = 0;
}

void InitCommand(SCom *tmp) {
    tmp->command = (char **) malloc(sizeof(char *) * 3);
    tmp->command[0] = NULL;
    tmp->is_background = 0;
    tmp->file_in = (char *) malloc(sizeof(char) * 20);
    tmp->file_out = (char *) malloc(sizeof(char) * 20);
    tmp->amount_words = 0;
    tmp->redirect_in = 0;
    tmp->redirect_out = 0;
}

void FreeWord(SWord *tmp) {
    free(tmp->word);
    tmp->word = NULL;
    free(tmp);
}

void FreeCommand(SCom *tmp) {
    for (int i = 0; tmp->command[i] != NULL; i++) {
        free(tmp->command[i]);
        tmp->command[i] = NULL;
    }
    free(tmp->command);
    tmp->command = NULL;
    free(tmp->file_in);
    free(tmp->file_out);
    tmp->file_in = NULL;
    tmp->file_out = NULL;
}

void AddInWord(SWord *wrd, char c) {
    if (wrd->length >= wrd->memory) {
        wrd->memory *= 2;
        char *tmp = (char *) malloc(sizeof(char) * wrd->memory);
        strcpy(tmp, wrd->word);
        free(wrd->word);
        wrd->word = tmp;
        tmp = NULL;
    }
    wrd->word[wrd->length] = c;
    wrd->word[wrd->length + 1] = '\0';
    wrd->length++;
}

void AddInCommand(SCom *com, SWord *wrd) {
    char *tmp = (char *) malloc(sizeof(char) * (wrd->length + 1));
    strcpy(tmp, wrd->word);
    com->command[com->amount_words] = tmp;
    com->command[com->amount_words + 1] = NULL;
    com->amount_words++;
}

SWord *ProcWord(char ch, int *fl, SCom *com) {
    SWord *wrd = (struct StrWord *) malloc(sizeof(struct StrWord));
    InitWord(wrd);
    char c = ch;
    while (c == ' ') c = getchar();
    while ((c != ' ') && (c != '\n')) {
        switch (c) {
            case '&':
                if ((c = getchar() == '\n')) *fl = 1;
                com->is_background = 1;
                return wrd;
                
            case '\'':
                while ((c = getchar()) != '\'') AddInWord(wrd, c);
                c = getchar();
                continue;
            
            case '\"':
                while ((c = getchar()) != '\"') {
                    if (c == '\\') {
                        if (((c = getchar()) != '\\') && (c != '\"')) AddInWord(wrd, '\\');
                    }
                    AddInWord(wrd, c);
                }
                c = getchar();
                continue;
                
            case '<':
                com->redirect_in = 1;
                c = getchar();
                while ((c == ' ') || (c == '\'')) { // lishnie probeli
                    c = getchar();
                }
                SWord *file_input = (struct StrWord *) malloc(sizeof(struct StrWord));
                InitWord(file_input);
                while ((c != '\'') && (c != ' ') && (c != '\n')) {
                    AddInWord(file_input, c);
                    c = getchar();
                }
                if (c == '\'') c = getchar();
                strcpy(com->file_in, file_input->word);
                FreeWord(file_input);
                break;
                
            case '>':
                if ((c = getchar()) == '>') {
                    com->redirect_out = 2;
                    c = getchar();
                }
                else com->redirect_out = 1;
                while ((c == ' ') || (c == '\'')) { // lishnie probeli
                    c = getchar();
                }
                SWord *file_output = (struct StrWord *) malloc(sizeof(struct StrWord));
                InitWord(file_output);
                while ((c != ' ') && (c != '\n') && (c != '\'')) {
                    AddInWord(file_output, c);
                    c = getchar();
                }
                if (c == '\'') c = getchar();
                strcpy(com->file_out, file_output->word);
                FreeWord(file_output);
                break;
            
            case '\\':
                if ((c = getchar()) == '\n') {
                    c = getchar();
                    continue;
                }
                if ((c != '\\') && (c != '\'') && (c != '\"')) AddInWord(wrd, '\\');
                
            default:
                AddInWord(wrd, c);
                c = getchar();
                break;
        }
    }
    if ((c == '\n') || (c == EOF)) *fl = 1;
    return wrd;
}

SCom *GetCom(int *fl) {
    char c;
    SWord *wrd;
    SCom *our_com = (struct StrCommand *) malloc(sizeof(struct StrCommand));
    InitCommand(our_com);
    while (!(*fl)) {
        c = getchar();
        wrd = ProcWord(c, fl, our_com);
        if (our_com->is_background != 0) {
            FreeWord(wrd);
            return our_com;
        }
        if ((wrd->word[0] != '\0') && (wrd->word[0] != ' ')) AddInCommand(our_com, wrd);
        FreeWord(wrd);
    }
    return our_com;
}

void CommandCD(char **com) {
    if (com[1] == NULL) chdir(getenv("HOME"));
    else chdir(com[1]);
}

void OtherCommand(SCom com) {
    pid_t pid = fork();
    if (pid == -1) { //error
        perror("Unable to create new process");
        return;
    }
    if (pid == 0) { //child
        if (com.redirect_in != 0) {
            int fd_in = open(com.file_in, O_RDONLY);
            if (fd_in < 0) {
                perror("We can\'t open file");
                exit(-1);
            }
            else {
                dup2(fd_in, 0);
                close(fd_in);
            }
        }
        else if (com.redirect_out != 0) {
            int fd_out;
            if (com.redirect_out == 1) fd_out = open(com.file_out, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
            else fd_out = open(com.file_out, O_WRONLY | O_CREAT | O_APPEND, S_IWUSR);
            if (fd_out < 0) {
                perror("We can\'t open file");
                exit(-1);
            }
            else {
                dup2(fd_out, 1);
                close(fd_out);
            }
        }
        execvp(com.command[0], com.command);
        perror("Incorrect command");
        exit(-1);
    }
    else { //parent
        if (com.is_background == 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) == 0) perror("Child process couldn\'t complete successfully"); //error
        }
        else {
            printf("Process: %i\n", pid);
            usleep(5000);
        }
    }
}

int main(int argc, const char * argv[]) {
    SCom *our_command;
    int fl;
    printf("Enter the text $: ");
    while (1) {
        fl = 0;
        our_command = GetCom(&fl);
        if (our_command->command[0] != NULL) {
            if (strcmp(*our_command->command, "cd") == 0) CommandCD(our_command->command);
            else if (strcmp(*our_command->command, "exit") == 0) exit(0);
            else OtherCommand(*our_command);
        }
        FreeCommand(our_command);
        if (((!fl) && (our_command->is_background == 0)) || (fl)) printf("Enter the text $: ");
    }
    return 0;
}
