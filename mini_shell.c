#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#define BUFFSIZE 1024

// Cette fonction vérifie si l'avant-dernier argument est '>'
int is_redir(int argc, char** argv[]){
    if(argc<3){
        return 0;
    }
    if(*((*argv)[argc-2]) == '>'){
        (*argv)[argc-2] = NULL;
        return 1;
    }
    return 0;
}

/**
 * Prend une chaine de caractère s et change tout les 
 * espaces en '\0' pour indiquer la fin d'un mot
 * Met chaque mot dans une case de argv
 * Retourne le nombre de mots(arguments) présent dans s
*/
int parse_line(char *s, char **argv []){
    int i = 0, argc = 0;                //argc : nombre d'arguments
    char* str = s;
    char* p;
    while(p != NULL){
        (*argv)[i] = str;
        argc++;
        p = strpbrk(str, " \f\r\t\n\v");
        if(p != NULL) {
            *p = '\0';
        }
        else{
            (*argv)[i+1] = NULL;
        }
        str = p+1;        
        i++;
    }
    return argc;
}

void handler(){
    printf("\n");
}

int main(int argc, char ** argv){
    struct sigaction action;
    action.sa_handler = handler;
    sigaction(SIGINT, &action, NULL);

    char buf[BUFFSIZE];
    int lus, i, argCount = 0, fd;
    char ** argVector;
    int cpid, status;

    //initialisation du tableau d'arguments
    argVector = malloc(sizeof(int)*16);
    for(i = 0; i<16; i++){
        argVector[i] = malloc(sizeof(char)*BUFFSIZE);
    }

    char* input;
    while(1){
        int fd_pipe[2];
        if(write(STDOUT_FILENO, "$ ", 2)<0){
            perror("write");
            exit(2);
        }
        if((lus = read(STDIN_FILENO, buf, BUFFSIZE))>1){        //on continue seulement s'il y a quelque chose à lire
            input = malloc(sizeof(char)*(lus-1));
            for(i = 0; i<lus-1; i++){
                input[i] = buf[i];
            }
            argCount = parse_line(input, &argVector);
            if(strcmp("exit", input) == 0){
                exit(1);
            }
            cpid = fork();
            switch(cpid){
                case -1:
                    perror("fork");
                    exit(2);
                case 0:
                    if(*argVector[argCount-1] == '|'){
                        pipe(fd_pipe);
                        argVector[argCount-1] = NULL;
                        
                        switch(fork()){
                            case -1:
                                perror("fork2");
                                exit(2);
                            case 0:
                                //fils : commande avant '|'
                                dup2(fd_pipe[1], STDOUT_FILENO);
                                close(fd_pipe[0]);
                                close(fd_pipe[1]);
                                break;
                                
                            default:
                                //père: commande après '|' attends que le fils ait fini
                                wait(NULL);
                                //on demande une seconde commande
                                char** argVector2;
                                argVector2 = malloc(sizeof(int)*16);
                                for(i = 0; i<16; i++){
                                    argVector2[i] = malloc(sizeof(char)*BUFFSIZE);
                                }
                                if((lus = read(STDIN_FILENO, buf, BUFFSIZE))>1){
                                    input = malloc(sizeof(char)*(lus-1));
                                    for(i = 0; i<lus-1; i++){
                                        input[i] = buf[i];
                                    }
                                    parse_line(input, &argVector2);
                                }
                                dup2(fd_pipe[0], STDIN_FILENO);
                                close(fd_pipe[0]);
                                close(fd_pipe[1]);
                                execvp(argVector2[0], argVector2);
                                exit(2);
                        }
                    }
                    else if(is_redir(argCount, &argVector)){
                        if((fd = open(argVector[argCount-1], O_RDWR | O_CREAT | O_TRUNC,  S_IRWXU)) < 0){
                            perror("open");
                            exit(2);
                        }
                        dup2(fd, STDOUT_FILENO);
                    }
                    execvp(argVector[0], argVector);
                    exit(2);
                default:
                    waitpid(cpid, &status, 0);
                    free(input);
                    break;
            }
        }
    }
    for(i = 0; i<16; i++){
        free(argVector[i]);
    }
    free(argVector);
    return EXIT_SUCCESS;
}
