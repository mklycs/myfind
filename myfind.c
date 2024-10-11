#include <stdio.h>     // for printf()
#include <string.h>    // for strcmp() und strcpy() 
#include <getopt.h>    // for getopt()
#include <ctype.h>     // for tolower()
#include <sys/types.h> // for fork()
#include <unistd.h>
#include <stdlib.h>    // for exit()
#include <dirent.h>    // for dirent
#include <errno.h>     // for errno
#include <sys/stat.h>  // for statbuf
#include <sys/wait.h>

#ifndef MAX_LENGTH
#define MAX_LENGTH 512
#endif

struct node{
    char filename[MAX_LENGTH];
    struct node *next;
};

struct node* addNode(struct node *head, const char *file){
    struct node *newNode = (struct node*)malloc(sizeof(struct node));
    strcpy(newNode->filename, file);
    newNode->next = head;
    return newNode;
}

void freeMem(struct node *head){
    while(head != NULL){
        struct node *temp = head;
        head = head->next;
        free(temp);
    }
}

int parseOptions(int argc, char* argv[], int *case_insensitive, int *recursive){
    int option;
    while((option = getopt(argc, argv, "iR")) != -1){
        if(option == 'i'){
            (*case_insensitive) = 1;
        }else if(option == 'R'){
            (*recursive) = 1;
        }else{
            fprintf(stderr, "Usage: %s [-R] [-i] [searchpath] [filename1] [filename2]...[filenameN]\n", argv[0]); // ansonsten wird gesagt, wie das Programm verwendet wird
            return 1;
        }
    }
    return 0;
}

struct node* parseFilesNPaths(int argc, char* argv[], char *searchpath, struct node *head){
    for(int i = 1; i < argc; i++){                        
        if(argv[i][0] == '-')                             // to ignore arguments which have been handled with getopt
            continue;
        
        int len = strlen(argv[i]);
        if(argv[i][0] == '/'){                            // to remove the backslash from the path
            for(int ii = 0; ii < len; ii++){
                if(ii > len) break;
                searchpath[ii] = argv[i][ii+1];
            }
            searchpath[len] = '\0';
        }else if(argv[i][0] == '.' && argv[i][1] == '/'){ // current path
            strcpy(searchpath, argv[i]);
            searchpath[len] = '\0';
        }else head = addNode(head, argv[i]);              // add the to search filenames to the linked list
    }

    if(strlen(searchpath) == 0){                          // if no path is given as arguments the search starts from the root direcotry
        searchpath[0] = '/';
        searchpath[1] = '\0';
    }

    return head;
}

void makeLowercase(char *string){
    for(size_t i = 0; i < strlen(string); i++)
        string[i] = tolower(string[i]);
}

void searchFiles(const char *searchpath, char *filename, unsigned const int case_insensitive, unsigned const int recursive, int write_fd){
    struct dirent *direntp;
    DIR *dirp;

    char temp_direntpname[MAX_LENGTH];
    char temp_filename[MAX_LENGTH];
    
    if((dirp = opendir(searchpath)) == NULL){ 
        perror("Failed to open directory");
        return;
    }

    while((direntp = readdir(dirp)) != NULL){
        if(strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0) continue; // to skip . und .. 

        strcpy(temp_direntpname, direntp->d_name); // the temps are there to show later how the file name is actually spelled correctly in the output
        strcpy(temp_filename, filename);
        
        if(case_insensitive == 1){
            makeLowercase(temp_direntpname);                        
            makeLowercase(temp_filename);
        }

        if(recursive == 1){
            char newpath[MAX_LENGTH];
            struct stat statbuf;
            snprintf(newpath, MAX_LENGTH, "%s/%s", searchpath, direntp->d_name);       // writes a new path in a new string, which is then used for the recursive search
            if(stat(newpath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)){              // if it is a folder
                searchFiles(newpath, filename, case_insensitive, recursive, write_fd); // then search recursively
            }else if(strcmp(temp_direntpname, temp_filename) == 0){                    // if it is a file and if the file is successfully found it writes its full path into the pipe
                dprintf(write_fd, "%d: %s: %s/%s\n", getpid(), filename, realpath(searchpath, NULL), direntp->d_name);
                break;                                                                 // and it is canceled to avoid having to search any further
            }             
        }else if(strcmp(temp_direntpname, temp_filename) == 0){
            dprintf(write_fd, "%d: %s: %s/%s\n", getpid(), filename, realpath(searchpath, NULL), direntp->d_name);
            break;
        }
    }
    while((closedir(dirp) == -1) && (errno == EINTR)); // folders are closed to release resources again
}

int main(int argc, char* argv[]){
    if(argc < 2){
        printf("Keine Argumente übergeben.\n");
        return 1;
    }

    struct node *files = NULL;
    char searchpath[MAX_LENGTH] = "";
    
    int case_insensitive = 0;
    int recursive = 0; 

    int result = parseOptions(argc, argv, &case_insensitive, &recursive);
    if(result == 1) return 1;

    files = parseFilesNPaths(argc, argv, searchpath, files); 
    struct node *temp = files;

    int pipe_fds[2]; 
    if(pipe(pipe_fds) == -1){            // creates a Pipe
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    while(temp != NULL){                                                                       // creates a fork for each file
        pid_t gabel = fork(); 
        if(gabel == -1){
            printf("Failed to fork.\n");
            continue; // continue instead of return 1, so that other processes can be created and at least a few files can be found
        }else if(gabel == 0){
            close(pipe_fds[0]);                                                                // close the read in the child process to be on the safe side
            searchFiles(searchpath, temp->filename, case_insensitive, recursive, pipe_fds[1]); // searches for the file and passes the write mode for the pipe
            close(pipe_fds[1]);                                                                // closes write after the search
            exit(EXIT_SUCCESS);                                                                // ends the child process after the search
        }
        temp = temp->next;
    }

    close(pipe_fds[1]);                                                // closes the write in the parent process
    char buffer[MAX_LENGTH];
    int nBytes = 0;
    while((nBytes = read(pipe_fds[0], buffer, sizeof(buffer)-1)) > 0){ // outputs the content of the pipe (sizeof(buffer)-1 specifies the maximum number of bytes to be read. -1 for '\0')
        buffer[nBytes] = '\0';
        printf("%s", buffer);
    }

    close(pipe_fds[0]);                                                // closes the read in the parent process

    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) != 0){ // waits for the end of child processes and terminates when no more child processes exist
        if((pid == -1) && (errno != EINTR))
            break;
    }

    freeMem(files);
}