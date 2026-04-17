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

int main(int argc, char *argv[]) {
    //if NO FILES
    if (argc == 1) {
        return 0;
    }

    char buffer[4096];
    const char errorMessage[] = "wcat: cannot open file\n";

    //now loop and process each file
    for (int i = 1; i < argc; i++) {
        int fileDescriptor = open(argv[i], O_RDONLY); //open file for reading
        if (fileDescriptor < 0) { //if file fails to open, prints error message!
            write(STDOUT_FILENO, errorMessage, sizeof(errorMessage) - 1);
            return 1;
        }

        while (true) { //escape this while loop when return or break occurs
            ssize_t bytesRead = read(fileDescriptor, buffer, 4096);
            if (bytesRead < 0) { //error
                close(fileDescriptor);
                return 1;
            } else if (bytesRead == 0) { //if end of file is reached
                break;
            }
            ssize_t writtenCount = 0; //count of bytes written so far
            while (writtenCount < bytesRead) { //keep writing until all bytes are written
                ssize_t bytesWritten = write(STDOUT_FILENO, buffer + writtenCount, bytesRead - writtenCount);
                if (bytesWritten < 0) { //error
                    close(fileDescriptor);
                    return 1;
                }
                writtenCount += bytesWritten;
            }
        }
        close(fileDescriptor);
    }

    return 0;
}