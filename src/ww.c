#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include "unbounded_queue.h"

#define BUFFERSIZE 64

struct wrappedString {
    char *string;
    size_t size;
};

struct wrapFileArgs {
    int fd;
    int wfd;
    int status;
    size_t colSize;
};

struct wrapDirectoryArgs {
    bool recursive;
    bool fileThreading;
    int colSize;
    int returnVal;
    int numDirectoryThreads;
    DIR *dir;
    char *dirName;
    struct Queue *directoryQueue;
    struct Queue *fileQueue;
};

void checkWriteSuccess(ssize_t writeValue) {
    if (writeValue == -1) {
        perror("Write failed!");
        exit(2);
    }
}

void checkIfMemoryAllocationFailed(void * ptr) {
    if (ptr == NULL) {
        perror("Memory allocation failed! Exiting!");
        exit(2); // Exit code zero is for if the word is larger than the column width
    }
}

char checkArgs(int argc, char **argv) {

    struct stat sb;
    short recursiveFlag = 0;

    if (argc <= 1 || argc > 4) {
        perror("Missing arguments or too many arguments! Please check your input again!");
        exit(1);
    }

    if (argc == 4) {
        if (strncmp(argv[1], "-r", 2) == 0) { // We can check the integers later making sure they are positive and formatted correctly when we actually need it. We are wasting time here by checking since it is a bit annoying to get the ints here.
            recursiveFlag = 1;
        } else {
            perror("Three arguments given, but first argument does not begin with -r");
            exit(2);
        }
    }

    if (atoi(argv[1 + recursiveFlag]) < 1) { //if column size is zero or negative
        perror("Column size cannot be zero or negative!");
        exit(2);
    }

    if (argc == 3 + recursiveFlag) { //if there are 2 args, first one being for file path, we are not counting that in our documentation, but it is there.
        stat(argv[2 + recursiveFlag], &sb);
        if (S_ISREG(sb.st_mode)) { //second arg is a reg file
            return 'f'; //f for file
        } else if (S_ISDIR(sb.st_mode)) { //second arg is a directory
            return 'd'; //d for directory
        } else { //checks if file or directory exists
            perror("Second argument should be an existing regular file or a directory! Please check your input!");
            exit(2);
        }
    } else if (argc == 2) { //if there is only one arg (this is cute, lol)
        return 'e'; //e for empty
    }

    perror("How did you even get here?");
    exit(2); //if it reaches here for some reason exit

}

char* readPathName(char* dir, char* de){ //function that creates pathname so i can go through files in dir
    int sizeOfString = strlen(dir) + strlen(de) + 2; //add 2 bc terminator and backslash
    char* newString = malloc(sizeOfString*sizeof(char)); // allocate mem
    checkIfMemoryAllocationFailed(newString);

    //make string by strcat
    newString[0]='\0';
    strcat(newString, dir);
    strcat(newString, "/");
    strcat(newString, de);
    return newString;
}

char* writePathName(char* dir, char* de){
    char *prefix = "wrap.";
    int sizeOfString = strlen(dir) + strlen(de) + strlen(prefix) + 2; //add 2 bc terminator and backslash
    char* newString = malloc(sizeOfString*sizeof(char)); // allocate mem
    checkIfMemoryAllocationFailed(newString);

    //make string by strcat
    newString[0]='\0';
    strcat(newString, dir);
    strcat(newString, "/");
    strcat(newString,prefix);
    strcat(newString, de);
    return newString;
}

/*
 * Wrap file will take an input file descriptor, a column size, and a file descriptor to write to (in most cases, it will
 * just be stdout, but any file does work). We tokenize each word in the input file, see if that word with the appropriate white
 * whitespace will fit on the current line given the `colSize`. If it does not, it will go to the next line and print the appropriate word
 * on that line.
 */

void* wrapFile(void *args) { // The function that actually wraps the file contents
    int status = 0; // Status indicates whether or not the word size is larger than the colSize. In that case, status becomes 1. By default, it is zero.

    if (args == NULL) {
        return NULL;
    }

    struct wrapFileArgs *wfa = args;

    char buffer[BUFFERSIZE]; // By default, it is 64 size.

    // String and wrapping setup.
    ssize_t numBytesRead;
    struct wrappedString ws;
    ws.string = calloc(wfa->colSize, sizeof(wfa->colSize));
    checkIfMemoryAllocationFailed(ws.string); // This function checks for malloc failures.
    ws.size = 0;

    char *currentWord = NULL;
    size_t wordSize = 0;
    int consecutiveNewLines = 0;

    while ((numBytesRead = read(wfa->fd, buffer, BUFFERSIZE)) > 0) { // We start reading the file here/
        for (int x = 0; x < numBytesRead; x++) {
            if (buffer[x] == '\n') {
                consecutiveNewLines++;

                if (consecutiveNewLines == 2) { // If we see two new lines in a row, that means we are at a paragraph and need to print out the paragraph.
                    if (ws.size > 0) { // This means we need to flush the current wrapped string before we can continue.
                        checkWriteSuccess(write(wfa->wfd, ws.string , ws.size));
                        checkWriteSuccess(write(wfa->wfd, "\n", 1));
                        ws.string = memset(ws.string, 0, wfa->colSize);
                        ws.size = 0;
                    }

                    checkWriteSuccess(write(wfa->wfd, "\n", 1)); // Write newline for next paragraph.
                }
            }

            if (!isspace(buffer[x])) { // This means we are reading a word.
                wordSize++;

                currentWord = realloc(currentWord, wordSize + 1); // For the null terminator... And later a space. Hence, we add 1.

                checkIfMemoryAllocationFailed(currentWord);

                currentWord[wordSize - 1] = buffer[x];
                currentWord[wordSize] = '\0';

                consecutiveNewLines = 0;
            } else if (buffer[x] == ' ' || buffer[x] == '\n') { // This means we are done reading a word and can start the wrapping process.
                if (wordSize + 1 > wfa->colSize) { // If the word is bigger than colSize...
                    status = 1;

                    if (ws.size > 0) { // We may need to flush the current wrapped string before we can deal with the current word.
                        checkWriteSuccess(write(wfa->wfd, ws.string , ws.size));
                        checkWriteSuccess(write(wfa->wfd, "\n", 1));
                        ws.string = memset(ws.string, 0, wfa->colSize);
                        ws.size = 0;
                    }

                    checkWriteSuccess(write(wfa->wfd, currentWord , wordSize)); // Print...
                    checkWriteSuccess(write(wfa->wfd, "\n", 1));
                    currentWord = memset(currentWord, 0, wordSize); // and reset
                    wordSize = 0; // Continue from here.
                }


                if (wordSize > 0) {
                    if (ws.size + wordSize + 1 <= wfa->colSize) { // Check to see if the current word will fit inside the wrapped string. If so, concat it into wrapped string.
                        if (ws.size > 0) {
                            ws.string[ws.size] = ' '; // Add a space for the word
                        }
                        ws.string = strcat(ws.string, currentWord);

                        if (ws.size == 0) {
                            ws.size += wordSize; // If wrapped string is empty, the size becomes wordSize.
                        } else {
                            ws.size += wordSize + 1; // Else, wrapped string needs to store the size of the space we added if it's not currently empty.
                        }

                    } else {
                        checkWriteSuccess(write(wfa->wfd, ws.string, ws.size)); // Else write string and new line character.
                        checkWriteSuccess(write(wfa->wfd, "\n", 1));

                        ws.string = memset(ws.string, 0, wfa->colSize); // Reset everything.
                        ws.string = memcpy(ws.string, currentWord, wordSize); // Store the next word we just loaded in...
                        ws.size = wordSize; // Store word size.
                    }

                    currentWord = memset(currentWord, 0, wordSize); // Reset current word once we get here regardless of what happened above.
                    wordSize = 0;
                }
            }
        }
    }

    if (ws.size == 0) {
        checkWriteSuccess(write(wfa->wfd, ws.string, ws.size)); // Write the final wrapped string and flush it out. Then write two new lines, one to end the final wrapped string, and one for the new paragraph.
        checkWriteSuccess(write(wfa->wfd, "\n", 1));
    } else {
        checkWriteSuccess(write(wfa->wfd, ws.string, ws.size)); // Write the final wrapped string and flush it out. Then write two new lines, one to end the final wrapped string, and one for the new paragraph.
        checkWriteSuccess(write(wfa->wfd, "\n\n", 2));
    }

    free(ws.string); // Free memory
    free(currentWord);
    close(wfa->fd);
    close(wfa->wfd);

    wfa->status = status;

    return wfa; // Was our process successful once we get here? Status indicates that.
}

void *wrapDirectory(void *args) {
    struct wrapDirectoryArgs *wda = args;
    struct dirent *de;
    struct stat sb;
    de = readdir(wda->dir);
    int status = 0; //this is put here so we know to return 1 or not if one of the files contains a word size larger than colsize

    while (de!=NULL) { //while have not read last entry

        //skips files that start with . and start with wrap.
        while(de!=NULL && (strncmp (de->d_name, ".", 1) == 0 || strncmp(de->d_name, "wrap.", 5)==0)){
            de=readdir(wda->dir);
        }


        //if directory is null bc last entry starts with . or wrap. break out of loop
        if (de==NULL){
            break;
        }

        //get path to files in dir and file info
        char *rpath = readPathName(wda->dirName, de->d_name);

        stat(rpath, &sb);

        if (S_ISDIR(sb.st_mode) && wda->recursive == true) {
            struct wrapDirectoryArgs *wda2 = malloc(sizeof(struct wrapDirectoryArgs));
            wda2->dir = opendir(rpath);
            wda2->dirName = malloc(strlen(rpath) + 1);
            wda2->dirName = strcpy(wda2->dirName, rpath);
            wda2->directoryQueue = wda->directoryQueue;
            wda2->fileQueue = wda->fileQueue;
            wda2->fileThreading = wda->fileThreading;
            wda2->recursive = wda->recursive;
            wda2->colSize = wda->colSize;

            enqueue(wda->directoryQueue, wda2, sizeof(struct wrapDirectoryArgs));

            free(wda2);
        }

        //checks if file entry is regular file and only lists and read reg files
        if(S_ISREG(sb.st_mode)) {

            //get path for new wrap. file in directory and open them
            char *wpath = writePathName(wda->dirName, de->d_name);
            int wfd = open(wpath, O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, S_IRWXU);
            free(wpath);

            struct wrapFileArgs wfa;
            wfa.fd = open(rpath, O_RDONLY);
            wfa.colSize = wda->colSize;
            wfa.wfd = wfd;

            if (wda->fileThreading) {
                enqueue(wda->fileQueue, &wfa, sizeof(struct wrapFileArgs));
            } else {
                wrapFile(&wfa);

                int tempStatus = wfa.status;

                if (tempStatus == 1) {
                    status = 1;
                }
            }
        }
        de = readdir(wda->dir);
        free(rpath);

    }
    closedir(wda->dir); //close dir

    wda->returnVal = status;

    return wda;
}

void printDirEntry(DIR *dir){ //function that i just use to check the contents of my directory
    struct dirent* de;
    de = readdir(dir);
    while (de != NULL) {
        puts(de->d_name);
        de = readdir(dir);
    }
    puts("\n");
    closedir(dir);
}

/*
 * Starts file threads.
 */

void *startFileThreads(void *queue) {
    struct Queue *fileQueue = queue;

    struct wrapFileArgs *wfa = dequeue(fileQueue);
    int *status = malloc(sizeof(int));

    while (wfa != NULL) {
        wrapFile(wfa);

        if (wfa->status == 1) {
            *status = 1;
        }

        free(wfa);

        wfa = dequeue(fileQueue);
    }

    return status;
}

/*
 * Starts directory threads.
 */
void *startDirectoryThreads(void *queue) {
    struct Queue *directoryQueue = queue;
    struct wrapDirectoryArgs *wda = dequeue(directoryQueue);

    while (wda != NULL) {
        wrapDirectory(wda);

        free(wda->dirName);
        free(wda);

        wda = dequeue(directoryQueue);
    }

    return NULL;
}

/*
 * This function processes the -r argument and any file or directory threading numbers. If there is file or directory threading arguments
 * then the appropriate threads are started and created.
 */
int recursiveThreading(char **args) {
    int fileThreadsNum = 0, directoryThreadsNum = 0;
    struct Queue *directoryQueue = initQueue();
    checkIfMemoryAllocationFailed(directoryQueue);

    struct Queue *fileQueue = NULL;
    bool fileThreading = false, directoryThreading = false;
    pthread_t *fileThreads = NULL, *directoryThreads = NULL;

    if (strlen(args[1]) > 2) {
        sscanf(args[1], "-r%d,%d", &fileThreadsNum, &directoryThreadsNum);

        if (fileThreadsNum < 0 || directoryThreadsNum < 0) {
            perror("Cannot have negative fileThreads! :");
            exit(2);
        }

        if (fileThreadsNum > 0) {
            fileThreading = true;
            fileQueue = initQueue();
            checkIfMemoryAllocationFailed(fileQueue);
            fileThreads = malloc(sizeof(pthread_t) * fileThreadsNum);
            checkIfMemoryAllocationFailed(fileThreads);

            for (int x = 0; x < fileThreadsNum; x++) {
                pthread_create(&fileThreads[x], NULL, startFileThreads, fileQueue);
            }

            if (directoryThreadsNum > 0) {
                mainThreadTermination(directoryQueue, directoryThreadsNum);
                directoryThreading = true;
                directoryThreads = malloc(sizeof(pthread_t) * directoryThreadsNum);


                for (int x = 0; x < directoryThreadsNum; x++) {
                    pthread_create(&directoryThreads[x], NULL, startDirectoryThreads, directoryQueue);
                }
            }
        }
    }


    int returnVal = 0;

    struct wrapDirectoryArgs *initialWda = malloc(sizeof(struct wrapDirectoryArgs));

    initialWda->dir = opendir(args[3]);
    initialWda->dirName = malloc(strlen(args[3]) + 1);
    initialWda->dirName = strcpy(initialWda->dirName, args[3]);
    initialWda->colSize = atoi(args[2]);
    initialWda->recursive = true; // If this function is being called, then we are recursive.
    initialWda->fileThreading = fileThreading;
    initialWda->directoryQueue = directoryQueue;
    initialWda->fileQueue = fileQueue;
    initialWda->numDirectoryThreads = directoryThreadsNum;

    enqueue(directoryQueue, initialWda, sizeof(struct wrapDirectoryArgs)); // Enqueue directory path
    free(initialWda);

    if (!directoryThreading) {
        while (!isEmpty(directoryQueue)) {
            struct wrapDirectoryArgs *wda = dequeue(directoryQueue); // Dequeue any path that may have been added.
            if (wda->dirName != NULL) {
                wrapDirectory(wda);

                if (wda->returnVal == 1) {
                    returnVal = 1;
                }

                free(wda->dirName);
                free(wda);
            }
        }
    }


    if (fileThreading) {
        if (directoryThreading) {
            for (int x = 0; x < directoryThreadsNum; x++) {
                pthread_join(directoryThreads[x], NULL);
            }

            free(directoryThreads);
        }

        jobComplete(fileQueue);

        void *tempReturnVal;

        for (int x = 0; x < fileThreadsNum; x++) {
            pthread_join(fileThreads[x], &tempReturnVal);

            if ((*(int *) tempReturnVal) == 1) {
                returnVal = 1;
            }

            free(tempReturnVal);
        }

        free(fileQueue);
        free(fileThreads);
    }

    free(directoryQueue);

    return returnVal;
}

/*
 * Processes multiple arguments as specified by the extra credit.
 */
int extraCredit(int argc, char **argv){
    struct stat sb;
    int colSize;
    struct wrapFileArgs wfa;
    struct wrapDirectoryArgs wda;
    char *dirName;
    char *fileName;
    char *wpath;
    char *pathName;

    int finalStatus = 0;

    if(strncmp(argv[1], "-r", 2) == 0) {
        colSize = atoi(argv[2]);

        for (int i = 3; i < argc; i++) {
            int statSuccess = stat(argv[i], &sb);

            if (statSuccess < 0) {
                perror("Argument should be an existing regular file or a directory! Please check your input!");
                exit(2);
            }

            if (S_ISREG(sb.st_mode)) { //second arg is a reg file
                printf("Reg: %d\n", statSuccess);
                wfa.colSize = colSize;
                wfa.fd = open(argv[i], O_RDONLY);
                wfa.status = 0;

                int count = 0;
                pathName = argv[i];

                for(int j = strlen(pathName)-1; pathName[j] != '/'; j--){
                    count++;
                }

                dirName = malloc(strlen(argv[i]) - count);
                fileName = malloc(count + 2);

                dirName = strncpy(dirName, pathName, strlen(pathName)-count - 1);
                dirName[strlen(pathName)-count - 1] = '\0';
                fileName = strncpy(fileName, &pathName[strlen(pathName)-count], count+2);

                wpath = writePathName(dirName, fileName);

                wfa.wfd = open(wpath, O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, S_IRWXU); //need to change this to

                wrapFile(&wfa);

                if (wfa.status == 1) {
                    finalStatus = 1;
                }

                free(dirName);
                free(fileName);
                free(wpath);

            } else if (S_ISDIR(sb.st_mode)) { //second arg is a directory
                char **argumentArray = malloc(sizeof(char *) * argc);
                argumentArray[0] = malloc(strlen(argv[0]) + 1);
                argumentArray[0] = strcpy(argumentArray[0], argv[0]);
                argumentArray[1] = malloc(strlen(argv[1]) + 1);
                argumentArray[1] = strcpy(argumentArray[1], argv[1]);
                argumentArray[2] = malloc(strlen(argv[2]) + 1);
                argumentArray[2] = strcpy(argumentArray[2], argv[2]);
                argumentArray[3] = malloc(strlen(argv[i]) + 1);
                argumentArray[3] = strcpy(argumentArray[3], argv[i]);

                int status = recursiveThreading(argumentArray);

                if (status == 1) {
                    finalStatus = 1;
                }

                free(argumentArray[0]);
                free(argumentArray[1]);
                free(argumentArray[2]);
                free(argumentArray[3]);
            }
        }
    } else if(atoi(argv[1]) > 1) {
        colSize = atoi(argv[1]);

        for(int i = 2; i < argc; i++) {
            int statSuccess = stat(argv[i], &sb);

            if (statSuccess < 0) {
                perror("Argument should be an existing regular file or a directory! Please check your input!");
                exit(2);
            }

            if (S_ISREG(sb.st_mode)) { //second arg is a reg file

                wfa.colSize = colSize;
                wfa.fd = open(argv[i], O_RDONLY);
                wfa.status = 0;

                int count = 0;
                pathName = argv[i];

                for(int j = strlen(pathName)-1; pathName[j] != '/'; j--){
                    count++;
                }

                dirName = malloc(strlen(argv[i]) - count);
                fileName = malloc(count + 2);

                dirName = strncpy(dirName, pathName, strlen(pathName)-count - 1);
                dirName[strlen(pathName)-count - 1] = '\0';
                fileName = strncpy(fileName, &pathName[strlen(pathName)-count], count+2); // Why count + 2?

                wpath = writePathName(dirName, fileName);

                wfa.wfd = open(wpath, O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, S_IRWXU); //need to change this to

                wrapFile(&wfa);

                if (wfa.status == 1) {
                    finalStatus = 1;
                }

                free(dirName);
                free(fileName);
                free(wpath);

            } else if (S_ISDIR(sb.st_mode)) { //second arg is a directory

                wda.colSize = colSize;
                wda.dir = opendir(argv[i]);
                wda.dirName = malloc(strlen(argv[i]) + 1);
                wda.dirName = strcpy(wda.dirName, argv[i]);
                wda.returnVal = 0;
                wda.recursive = false;
                wda.fileThreading = false;
                wda.directoryQueue = NULL;
                wda.fileQueue = NULL;

                wrapDirectory(&wda);

                if (wda.returnVal == 1) {
                    finalStatus = 1;
                }

                free(wda.dirName);

            }
        }
    }

    return finalStatus;
}

int main(int argc, char **argv) {
    if (argc > 4 || (argc == 4 && strncmp(argv[1], "-r", 2) != 0)) { // Check the first argument to see what's going on...
        int status = extraCredit(argc, argv);
        return status;
    } else {
        char mode = checkArgs(argc, argv); // Processes arguments if there are not multiple sets of arguments
        if (mode == 'f') {
            int wfd = open("/dev/stdout", O_WRONLY | O_APPEND | O_TRUNC); // We use the full path names just to be specific and more clear. Hard coding in file descriptor 0 or 1 felt weird to us.

            struct wrapFileArgs wfa;
            wfa.fd = open(argv[2], O_RDONLY);
            wfa.colSize = atoi(argv[1]);
            wfa.wfd = wfd;

            wrapFile(&wfa);

            return wfa.status;
        } else if (mode == 'd') {
            //printDirEntry(opendir(argv[2]));

            if (argc == 4) { // If we get three arguments and checkArgs didn't fail, then it's recursive and potentially multi-threaded.
                return recursiveThreading(argv);
            } else if (argc == 3) {
                struct wrapDirectoryArgs wda;
                wda.dir = opendir(argv[2]);
                wda.dirName = argv[2];
                wda.colSize = atoi(argv[1]);
                wda.recursive = wda.fileThreading = false;
                wda.directoryQueue = wda.fileQueue = NULL;

                wrapDirectory(&wda);

                return wda.returnVal;
            }
        } else {
            int wfd = open("/dev/stdout", O_WRONLY | O_APPEND | O_TRUNC);

            struct wrapFileArgs wfa;
            wfa.fd = open("/dev/stdin", O_RDONLY);
            wfa.colSize = atoi(argv[1]);
            wfa.wfd = wfd;

            wrapFile(&wfa);

            return wfa.status;
        }
    }
}
