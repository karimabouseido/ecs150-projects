#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <string>

using namespace std;
/*
write function needs ssize_t write(int fd, const void *buf, size_t count);
read function needs ssize_t read(int fd, void *buf, size_t count);
close function needs int close(int fd);
open function needs int open(const char *pathname, int flags);
*/
int readFDLine(int FD, string &line, string &pending) {
    line.clear();
    while (true) {
        size_t newLine = pending.find('\n');
        if (newLine != string::npos) { //if new line is found in pending
            line = pending.substr(0, newLine + 1); //line is everything up to and including the new line character
            pending.erase(0, newLine + 1);
            return 1;
        }

        char buffer[4096];
        ssize_t bytesRead = read(FD, buffer, 4096);
        if (bytesRead < 0) {
            return -1; //error
        } else if (bytesRead == 0) { //if end of file is reached
            if (!pending.empty()) { //if there is still pending data, return it as a line
                line = pending;
                pending.clear();
                return 1;
            }
            return 0; //end of file reached and no pending data
        }

        pending.append(buffer, bytesRead); //append new data to pending
    }
    
}

int process(int FD, const string &inString) {
    string line;
    string pending; //pending data not processed yet

    while (true) {
        int lineCheck = readFDLine(FD, line, pending);
        if (lineCheck < 0) { //error
            return 1;
        } else if (lineCheck == 0) { //end of file reached
            break;
        }

        if(line.find(inString) != string::npos) { //if characters are found in line
            ssize_t writtenCount = 0;
            ssize_t totalCount = line.size();

            while (writtenCount < totalCount) { //keep writing until all bytes are written
                ssize_t bytesWritten = write(STDOUT_FILENO, line.c_str() + writtenCount, totalCount - writtenCount);
                if (bytesWritten < 0) { //error
                    return 1;
                }
                writtenCount += bytesWritten;
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char guide[] = "wgrep: searchterm [file ...]\n";
    const char errorMessage[] = "wgrep: cannot open file\n";

    if (argc < 2) { //if no search term is provided (if just ./wgrep)
        write(STDOUT_FILENO, guide, sizeof(guide) - 1);
        return 1;
    } else if (argc == 2) { //read from standard input if search term is only argument
        string search = argv[1];
        return process(STDIN_FILENO, search); //STDIN_FILENO can read standard input
    }

    string search = argv[1]; //search term is first argument (argc = 2)

    for (int i = 2; i < argc; i++) {
        int fileDescriptor = open(argv[i], O_RDONLY); //open file for reading
        if (fileDescriptor < 0) { //if file fails to open, prints error message!
            write(STDOUT_FILENO, errorMessage, sizeof(errorMessage) - 1);
            return 1;
        }
        int processWork = process(fileDescriptor, search);
        close(fileDescriptor);

        if (processWork != 0) {
            return 1;
        }
    }

    return 0;
}