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

struct parsed {
    vector<string> args;
    bool redirect = false; //if command has ">" redirection
    string outFile;
};

void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

string wspaceTrim(const string &s) {
    size_t start = 0; //first character
    size_t end = s.size(); //1 + last character

    // Find the first non-whitespace character
    while (start < end && isspace(s[start])) {
        start++;
    }

    // Find the last non-whitespace character
    while (end > start && isspace(s[end - 1])) {
        end--;
    }

    return s.substr(start, end - start); //RETURN PORTION OF STRING W/O START and END WHITESPACE
}

bool builtinCommand(const parsed& pcommand, vector<string>& path) {
    const string& cmdName = pcommand.args[0]; //command name is first argument
    if (cmdName == "exit") {
        if (pcommand.args.size() != 1) { //exit should have no arguments
            print_error();
            return true; //return true since command was handled, even though it was an error
        }
        exit(0); //exit the shell
    }

    if (cmdName == "cd") {
        if (pcommand.args.size() != 2) { //cd should have only 1 argument
            print_error();
            return true;
        }
        if (chdir(pcommand.args[1].c_str()) != 0) { //change directory
            print_error(); //if it's not 0, then chdir failed
        }
        return true;
    }

    if (cmdName == "path") {
        path.clear(); //clear the /bin path, bin commands using executables shouldn't WORK if no path
        for (size_t i = 1; i < pcommand.args.size(); i++) { //add each new path argument to path vector
            path.push_back(pcommand.args[i]);
        }
        return true;
    }

    return false;

}

//this function finds the full executable path to the parameter command by searching shell paths.
string findExecutable(const string& cmdName, const vector<string>& path) {
    for (const string& directory : path) { //try each path directory
        //check if file exists in directory
        string fullPath = directory; //start with directory to put in this temp string;
        if (!fullPath.empty() && fullPath.back() != '/') { //if fullPath already doesn't have a slash, add it
            fullPath += '/';
        }
        fullPath += cmdName; //append command name to directory to get full path
        //Check if it is executable with access()
        if (access(fullPath.c_str(), X_OK) == 0) { //if access returns 0, then file exists and is executable
            return fullPath; //return the full path to the executable
        }
    }
    return ""; //for if we finish searching all paths and we cannot find anything
}

void PathCommand(const parsed& pcommand, const vector<string> &path, vector<pid_t>& child_pid) {
    if (path.empty()) { //if path is empty (like with when you type the path command with no args), we can't execute bin commands
        print_error();
        return;
    }

    string execPath = findExecutable(pcommand.args[0], path); //find the executable path for the command
    if (execPath.empty()) { //if we can't find an executable for the command, print error and return
        print_error();
        return;
    }

    pid_t pid = fork(); //fork a child process to execute the command (IMPORTANT!)
    if (pid < 0) { //if fork fails, print error and return
        print_error();
        return;
    }

    if (pid == 0) {  //child process executes command
        if (pcommand.redirect) {
            //file descriptor for output file, write permissions
            int fd = open(pcommand.outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) { //if file fails to open, print error
                print_error();
                _exit(1); //exit child process safely
            }
            if (dup2(fd, STDOUT_FILENO) < 0) { //redirect stdout to output file
                print_error();
                close(fd);
                _exit(1);
            }
            if (dup2(fd, STDERR_FILENO) < 0) { //redirect stderr to output file
                print_error();
                close(fd);
                _exit(1);
            }
            close(fd); //close file descriptor
        }

        //SETUP SO EXECV works and COMMANDS GET EXECUTED
        vector <char*> argVec; //create argument vector for execv (execute file with argument vector)
        argVec.reserve(pcommand.args.size() + 1); //reserve space for command arguments
        for (const string& arg : pcommand.args) { //iterate through string command arguments
            argVec.push_back(const_cast<char*>(arg.c_str())); //convert to char and push into argVec
        }
        argVec.push_back(nullptr); //argVec must end with null pointer

        execv(execPath.c_str(), argVec.data()); //execute command, replacing child process
        print_error(); //if execv fails, goes here and prints error
        _exit(1);
    }

    child_pid.push_back(pid); //parent process stores child PID for waiting later
}

vector<string> splitChunk(const string &s) {
    vector<string> pieces; //vector to hold split pieces of string
    istringstream iss(s); //create input string stream from string s
    string pieceforVec; //temp variable to push into vector
    while (iss >> pieceforVec) {
        pieces.push_back(pieceforVec);
    }
    return pieces; //return vector of split pieces of string
}

bool commandParse(const string& chunk, parsed& pcommand) {
    pcommand = parsed(); //reset pcommand to default values
    string cmd = wspaceTrim(chunk); //trim leading and trailing whitespace from chunk
    int redirCount = 0; //number of ">" in command
    if (cmd.empty()) { 
        return false; //cannot have empty chunk, syntax error
    }

    size_t redirectIndex = string::npos; //index of ">" in command, default to npos (not found)
    for (size_t i = 0; i < cmd.size(); i++) { //count number of ">" in command
        if (cmd[i] == '>') {
            redirCount++;
            if (redirectIndex == string::npos) { //save index of first ">" only
                redirectIndex = i;
            }
        }
    }

    if (redirCount > 1) {
        return false; //cannot have more than one ">" in command, syntax error
    }

    if (redirCount == 1) { //redirection
        string leftPart = wspaceTrim(cmd.substr(0, redirectIndex)); //portion of command before ">"
        string rightPart = wspaceTrim(cmd.substr(redirectIndex + 1)); //portion of command after ">"
        if (leftPart.empty() || rightPart.empty()) {
            return false; //both sides of ">" must have text, syntax error
        }

        //redirection always has a command on the left and output file on the right

        //should only be one chunk on the right side of >, if more than one chunk, syntax error
        vector<string> rightSplitPieces = splitChunk(rightPart); //if whitespace in right, split into pieces
        if (rightSplitPieces.size() != 1) {
            return false;
        }

        pcommand.args = splitChunk(leftPart); //split left part into command and args
        if (pcommand.args.empty()) {
            return false; //need at least a command on the left side, syntax error
        }

        pcommand.redirect = true;
        pcommand.outFile = rightSplitPieces[0];
        return true; //parsed redirection command
    }

    //FOR NON-REDIRECTION COMMANDS, JUST SPLIT INTO ARGUMENTS
    pcommand.args = splitChunk(cmd); //split entire command into command and args
    if (pcommand.args.empty()) {
        return false; //need at least a command, syntax error
    }
    return true; //parsed non-redirection command
}

vector<string> ParallelCommandSplit(const string& line) {
    vector<string> commands; //start with empty vector
    string currentSegment; //start with empty string
    for (char c : line) { //for every character in line, detect "&" in string so chunks between them get pushed
        if (c == '&') {
            commands.push_back(currentSegment);
            currentSegment.clear();
        } else {
            currentSegment.push_back(c); //push character in current segment
        }
    }
    commands.push_back(currentSegment); //make sure to push last segment after EOL is reached
    return commands; //return vector of ALL commands in parallel command line
}

int main(int argc, char *argv[]) {
    vector<int> fdVec; //file descriptor vector
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

    string line; //string to hold each line of input
    while (true) { //SHELL IS IN WHILE LOOP WHILE RUNNING
        if (interactMode == true) {
            cout << "wish> "; //prompt for user input
            cout.flush(); //force "wish> " to appear before anything else or user input
        }

        if (!getline(*input, line)) { //hitting EOF means we exit shell in either mode
            exit(0);
        }

        //robust to white space, trim all leading and trailing white space
        string trimmedLine = wspaceTrim(line);
        if (trimmedLine.empty()) { //if blank line
            //cout << "empty! \n";
            continue; //go back to top of while loop in next iteration for next line of input
        }

        //PARALLEL COMMAND SPLIT AND SYNTAX CHECK
        vector<string> commandChunks = ParallelCommandSplit(trimmedLine); //split line into parallel command chunks by "&"

        // allow a trailing '&' by ignoring the final empty chunk
        if (!commandChunks.empty() && wspaceTrim(commandChunks.back()).empty()) {
            commandChunks.pop_back();
        }
        
        bool chunksPresent = false; //checks if at least one command exists after ParallelCommandSplit
        bool syntaxCheck = true; //checks for syntax error
        for (const string& chunk : commandChunks) {
            if (wspaceTrim(chunk).empty()) {
                syntaxCheck = false; //if chunk is empty, then we have a syntax error (bad & usage)
            } else {
                chunksPresent = true;
            }
        }

        if (!chunksPresent) { //if chunksPresent is false, then we have a syntax error due to bad "&" usage
            continue;
        }
        if (!syntaxCheck) {
            print_error();
            continue;
        }

        //PARSE INPUT LINE, REDIRECTION, and PATH
        vector<pid_t> childprocesses; //holds child PIDs

        for (const string& chunk : commandChunks) {
            parsed pcommand; //parsed command
            bool pcCheck = commandParse(chunk, pcommand);
            if (!pcCheck) { //if commandParse returns false, we have a syntax error in the command
                print_error();
                continue;
            }

            if (pcommand.redirect) { //built-n commands do not have REDIRECTION signs in syntax
                const string& cmdName = pcommand.args[0];
                if (cmdName == "exit" || cmdName == "cd" || cmdName == "path") {
                    print_error();
                    continue;
                }
            }

            bool builtinCheck = builtinCommand(pcommand, pathTerm); //checks if command is built-in
            if (builtinCheck) { //execute built in command logic with this condition
                continue;
            }

            PathCommand(pcommand, pathTerm, childprocesses); //execute non-built-in command logic with this function
        }

        for (size_t i = 0; i < childprocesses.size(); i++) { //wait for all child processes to finish
            int status = 0;
            while (waitpid(childprocesses[i], &status, 0) < 0) {
                if (errno != EINTR) { //if failure not interrupted by signal, stop retrying
                    break;
                }
            }
        }
    }
    /*
    if (input != nullptr && interactMode) {
        cout << "wish> \n"; //prompt for user input
    }
    */
   return 0;
}