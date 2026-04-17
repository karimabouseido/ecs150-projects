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

//writes all the characters in correspondence with count bytes
int writeFunc(int FD, const void *buf, size_t count) { 
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

//reads zip file and keeps track of all the characters and their run-lengths

int readFunc(int FD, void *buf, size_t count) { //to make sure all bytes are read from input
    char *current = (char *) buf;
    size_t totalRead = 0;

    while (totalRead < count) {
        ssize_t bytesRead = read(FD, current + totalRead, count - totalRead);
        if (bytesRead < 0) {
            return -1;
        } else if (bytesRead == 0) { //if end of file is reached
            return 0;
        }
        totalRead += bytesRead;
    }
    
    return 1;
}

int main(int argc, char *argv[]) {
    const char guide[] = "wunzip: file1 [file2 ...]\n";
    const char errorMessage[] = "wunzip: cannot open file\n";

    if (argc < 2) { //if no other terms are provided (if just ./wzip)
        write(STDOUT_FILENO, guide, sizeof(guide) - 1);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        int fileDescriptor = open(argv[i], O_RDONLY); //open file for reading
        if (fileDescriptor < 0) { //if file fails to open, prints error message!
            writeFunc(STDOUT_FILENO, errorMessage, sizeof(errorMessage) - 1);
            return 1;
        }

        while (true) {
            int runLength = 0;
            char runChar = 0;

            //read 4-byte integer count for run-length
            int readINTCheck = readFunc(fileDescriptor, &runLength, sizeof(int));
            if (readINTCheck < 0) {
                close(fileDescriptor);
                return 1;
            } else if (readINTCheck == 0) { //if end of file is reached
                break;
            }

            //read 1-byte character for run char
            int readCHARCheck = readFunc(fileDescriptor, &runChar, sizeof(char));
            if (readCHARCheck <= 0) {
                close(fileDescriptor);
                return 1; //bad output
            }

            //write row of characters "runLength" times
            for (int j = 0; j < runLength; j++) {
                if (writeFunc(STDOUT_FILENO, &runChar, sizeof(char)) < 0) {
                    close(fileDescriptor);
                    return 1;
                }
            }
        }

        close(fileDescriptor);
    }

    return 0;
}