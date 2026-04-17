#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

/*
write function needs ssize_t write(int fd, const void *buf, size_t count);
read function needs ssize_t read(int fd, void *buf, size_t count);
close function needs int close(int fd);
open function needs int open(const char *pathname, int flags);
*/

int writeFunc(int FD, const void *buf, size_t count) { //to make sure all bytes are written to output
    const char *current = (const char *) buf;
    size_t totalWritten = 0;

    while (totalWritten < count) {
        ssize_t bytesWritten = write(FD, current + totalWritten, count - totalWritten);
        if (bytesWritten < 0) {
            return -1;
        }
        totalWritten += bytesWritten;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    const char guide[] = "wzip: file1 [file2 ...]\n";
    const char errorMessage[] = "wzip: cannot open file\n";

    if (argc < 2) { //if no other terms are provided (if just ./wzip)
        write(STDOUT_FILENO, guide, sizeof(guide) - 1);
        return 1;
    }

    bool run = false;
    int runLength = 0;
    char runChar = 0;

    char buffer[4096];

    for (int i = 1; i < argc; i++) {
        int fileDescriptor = open(argv[i], O_RDONLY); //open file for reading
        if (fileDescriptor < 0) { //if file fails to open, prints error message!
            writeFunc(STDOUT_FILENO, errorMessage, sizeof(errorMessage) - 1);
            return 1;
        }
    
        while (true) {
            ssize_t bytesRead = read(fileDescriptor, buffer, 4096);
            if (bytesRead < 0) { //error
                close(fileDescriptor);
                return 1;
            } else if (bytesRead == 0) { //if end of file is reached
                break;
            }

            for (ssize_t j = 0; j < bytesRead; j++) { //used to count run-length of charachter
                char currentChar = buffer[j]; 
                if (!run) {
                    runChar = currentChar;
                    runLength = 1;
                    run = true;
                } else if (currentChar == runChar) { //if one of same type is found in a row, add it
                    runLength++;
                } else {
                    //failure check if writes into int and char of correct byte size and format
                    if (writeFunc(STDOUT_FILENO, &runLength, sizeof(int)) < 0) {
                        close(fileDescriptor);
                        return 1;
                    }
                    if (writeFunc(STDOUT_FILENO, &runChar, sizeof(char)) < 0) {
                        close(fileDescriptor);
                        return 1;
                    }

                    runChar = currentChar;
                    runLength = 1; //assumes new character, we reset the run
                }
            }
        }
    }

    if (run) { //if there is a pending run that has not been written to output, write it now
        if (writeFunc(STDOUT_FILENO, &runLength, sizeof(int)) < 0) {
            return 1;
        }
        if (writeFunc(STDOUT_FILENO, &runChar, sizeof(char)) < 0) {
            return 1;
        }
    }

    return 0;
}