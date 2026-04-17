#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

int main(int argc, char *argv[]) {
    vector<int> fdVec;
    vector<string> pathTerm; 
    pathTerm.push_back("/bin"); //all the unix commands are in /bin!

    istream* input = &cin; //contains stdin (user input!); can change to file input if batch mode w/ pointer
    ifstream batchFile; //input file stream for batch mode
    bool interactMode = true; //true if in interactive mode, false if in batch mode

    //ARGUMENT NUM CHECK before opening shell
    if (argc > 2) {
        print_error();
        exit(1);
    }

    if (argc == 2) { //this assumes batch file and activates batch mode
        batchFile.open(argv[1]); //open second argument
        if (!batchFile.is_open()) { //if file fails to open
            print_error();
            exit(1);
        }   
        input = &batchFile; //set input to batch file stream
        interactMode = false;
    }

    while (true) {


    }
    /*
    if (input != nullptr && interactMode) {
        cout << "wish> \n"; //prompt for user input
    }
    */
   return 0;
}