// Unix system calls: fork, exec, dup2, write, chdir, access.
#include <unistd.h>
// Types used by POSIX process APIs.
#include <sys/types.h>
// waitpid() for waiting on child processes.
#include <sys/wait.h>
// open() flags like O_CREAT and O_TRUNC.
#include <fcntl.h>

// errno values like EINTR.
#include <cerrno>
// strlen() for the fixed error message.
#include <cstring>
// ifstream for batch-file input mode.
#include <fstream>
// cin/cout for interactive input and prompt.
#include <iostream>
// istringstream for splitting by spaces.
#include <sstream>
// std::string for text handling.
#include <string>
// std::vector for dynamic arrays.
#include <vector>

// Lets us write string, vector, cout without std::.
using namespace std;

// Stores one parsed command from a line.
struct ParsedCommand {
    vector<string> args;          // Command name + command arguments.
    bool hasRedirection = false;  // True when command uses > file.
    string outFile;               // Output file used by redirection.
};

// Prints the exact project-required error text.
static void print_error() {
    char error_message[30] = "An error has occurred\n";               // Required message.
    write(STDERR_FILENO, error_message, strlen(error_message));        // Write to stderr.
}

// Removes leading and trailing spaces/tabs/newlines.
static string trim(const string& s) {
    size_t start = 0;                                                  // Index of first non-space.
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) {
        start++;                                                       // Move start right.
    }
    size_t end = s.size();                                             // One past last char.
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;                                                         // Move end left.
    }
    return s.substr(start, end - start);                              // Return trimmed part.
}

// Splits a line into raw command chunks using '&'.
static vector<string> splitByAmpersandRaw(const string& line) {
    vector<string> parts;                                              // Final command chunks.
    string cur;                                                        // Current chunk being built.
    for (char c : line) {                                              // Read each character.
        if (c == '&') {                                                // '&' ends one command.
            parts.push_back(cur);                                      // Save current chunk.
            cur.clear();                                               // Start a new chunk.
        } else {
            cur.push_back(c);                                          // Add regular char.
        }
    }
    parts.push_back(cur);                                              // Save the last chunk.
    return parts;                                                      // Return all chunks.
}

// Splits text by whitespace into tokens.
static vector<string> splitWhitespace(const string& s) {
    vector<string> tokens;                                             // Output token list.
    istringstream iss(s);                                              // Stream over the string.
    string tok;                                                        // Holds one token.
    while (iss >> tok) {                                               // Read next token.
        tokens.push_back(tok);                                         // Save token.
    }
    return tokens;                                                     // Return token list.
}

// Parses one command and validates redirection rules.
static bool parseCommand(const string& raw, ParsedCommand& out) {
    out = ParsedCommand();                                             // Reset output struct.
    string cmd = trim(raw);                                            // Remove outer spaces.
    if (cmd.empty()) {                                                 // Empty chunk is invalid.
        return false;
    }

    int redirCount = 0;                                                // Number of '>' symbols.
    size_t redirPos = string::npos;                                    // Position of first '>'.
    for (size_t i = 0; i < cmd.size(); i++) {                          // Scan command text.
        if (cmd[i] == '>') {                                           // Found redirection.
            redirCount++;                                              // Count it.
            if (redirPos == string::npos) {                            // Save first only.
                redirPos = i;
            }
        }
    }

    if (redirCount > 1) {                                              // More than one '>' is invalid.
        return false;
    }

    if (redirCount == 1) {                                             // Handle single redirection.
        string left = trim(cmd.substr(0, redirPos));                   // Command part.
        string right = trim(cmd.substr(redirPos + 1));                 // File-name part.

        if (left.empty() || right.empty()) {                           // Missing side is invalid.
            return false;
        }

        vector<string> rightTokens = splitWhitespace(right);           // Split right side.
        if (rightTokens.size() != 1) {                                 // Must be exactly one file.
            return false;
        }

        out.args = splitWhitespace(left);                              // Parse command args.
        if (out.args.empty()) {                                        // Need command name.
            return false;
        }

        out.hasRedirection = true;                                     // Mark redirection present.
        out.outFile = rightTokens[0];                                  // Store output file.
        return true;                                                   // Parsing succeeded.
    }

    out.args = splitWhitespace(cmd);                                   // No redirection case.
    return !out.args.empty();                                          // True only if a command exists.
}

// Finds full executable path by searching current shell paths.
static string findExecutable(const string& cmd, const vector<string>& paths) {
    for (const string& dir : paths) {                                  // Try each path dir.
        string full = dir;                                             // Start with directory.
        if (!full.empty() && full.back() != '/') {                     // Add slash if needed.
            full += "/";
        }
        full += cmd;                                                   // Append command name.
        if (access(full.c_str(), X_OK) == 0) {                         // Is executable?
            return full;                                               // Return first match.
        }
    }
    return "";                                                        // Not found in any path.
}

// Executes built-ins in parent process; returns true if handled.
static bool runBuiltin(const ParsedCommand& pcmd, vector<string>& paths) {
    const string& cmd = pcmd.args[0];                                  // Built-in name candidate.

    if (cmd == "exit") {                                              // Built-in: exit.
        if (pcmd.args.size() != 1) {                                   // exit takes no args.
            print_error();
            return true;                                               // Handled (as error).
        }
        exit(0);                                                       // Cleanly terminate shell.
    }

    if (cmd == "cd") {                                                // Built-in: cd.
        if (pcmd.args.size() != 2) {                                   // cd needs one arg.
            print_error();
            return true;                                               // Handled (as error).
        }
        if (chdir(pcmd.args[1].c_str()) != 0) {                        // Try changing directory.
            print_error();                                             // chdir failed.
        }
        return true;                                                   // cd is handled here.
    }

    if (cmd == "path") {                                              // Built-in: path.
        paths.clear();                                                 // Replace old path list.
        for (size_t i = 1; i < pcmd.args.size(); i++) {                // Add each new path arg.
            paths.push_back(pcmd.args[i]);
        }
        return true;                                                   // path is handled.
    }

    return false;                                                      // Not a built-in command.
}

// Forks and executes one external command.
static void executeExternal(const ParsedCommand& pcmd, const vector<string>& paths, vector<pid_t>& children) {
    if (paths.empty()) {                                               // No paths means no external cmds.
        print_error();
        return;
    }

    string execPath = findExecutable(pcmd.args[0], paths);             // Resolve command to full path.
    if (execPath.empty()) {                                            // Not found in path dirs.
        print_error();
        return;
    }

    pid_t pid = fork();                                                // Create child process.
    if (pid < 0) {                                                     // fork failed.
        print_error();
        return;
    }

    if (pid == 0) {                                                    // Child process branch.
        if (pcmd.hasRedirection) {                                     // Apply > file if requested.
            int fd = open(pcmd.outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666); // Open output file.
            if (fd < 0) {                                              // open failed.
                print_error();
                _exit(1);                                              // Exit child quickly.
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {                         // stdout -> file.
                print_error();
                close(fd);
                _exit(1);
            }
            if (dup2(fd, STDERR_FILENO) < 0) {                         // stderr -> same file.
                print_error();
                close(fd);
                _exit(1);
            }
            close(fd);                                                 // fd no longer needed.
        }

        vector<char*> argv;                                            // execv-style argv array.
        argv.reserve(pcmd.args.size() + 1);                            // Reserve space for null end.
        for (const string& s : pcmd.args) {                            // Convert string args to char*.
            argv.push_back(const_cast<char*>(s.c_str()));
        }
        argv.push_back(nullptr);                                       // argv must end with null.

        execv(execPath.c_str(), argv.data());                          // Replace child process image.

        print_error();                                                 // Reaches here only if execv fails.
        _exit(1);                                                      // Exit failed child.
    }

    children.push_back(pid);                                           // Parent stores child pid.
}

// Entry point of the shell program.
int main(int argc, char* argv[]) {
    vector<string> paths;                                              // Current shell search path.
    paths.push_back("/bin");                                          // Default path required by spec.

    istream* input = &cin;                                             // Input source defaults to stdin.
    ifstream batchFile;                                                // File stream for batch mode.
    bool interactive = true;                                           // True when reading from terminal.

    if (argc > 2) {                                                    // Too many startup args.
        print_error();
        exit(1);                                                       // Required fatal startup error.
    }

    if (argc == 2) {                                                   // Batch mode requested.
        batchFile.open(argv[1]);                                       // Open batch file.
        if (!batchFile.is_open()) {                                    // File open failed.
            print_error();
            exit(1);                                                   // Required fatal startup error.
        }
        input = &batchFile;                                            // Read lines from file.
        interactive = false;                                           // Do not print prompt in batch.
    }

    string line;                                                       // Stores each input line.
    while (true) {                                                     // Shell main loop.
        if (interactive) {                                             // Prompt only in interactive mode.
            cout << "wish> ";                                          // Required prompt text.
            cout.flush();                                              // Force prompt to appear now.
        }

        if (!getline(*input, line)) {                                  // Read next line; false at EOF.
            exit(0);                                                   // Graceful exit on EOF.
        }

        string lineTrimmed = trim(line);                               // Check for blank line.
        if (lineTrimmed.empty()) {                                     // Ignore empty input.
            continue;
        }

        vector<string> rawCommands = splitByAmpersandRaw(line);        // Split parallel commands.

        bool sawNonEmpty = false;                                      // Tracks if at least one cmd exists.
        bool syntaxError = false;                                      // Tracks bad '&' usage.
        for (const string& rc : rawCommands) {                         // Validate each split chunk.
            if (trim(rc).empty()) {                                    // Empty chunk means syntax error.
                syntaxError = true;
            } else {
                sawNonEmpty = true;                                    // Found real command text.
            }
        }

        if (!sawNonEmpty) {                                            // If nothing useful, skip line.
            continue;
        }
        if (syntaxError) {                                             // If malformed &, print error.
            print_error();
            continue;
        }

        vector<pid_t> children;                                        // Child processes launched this line.

        for (const string& raw : rawCommands) {                        // Execute each command chunk.
            ParsedCommand pcmd;                                        // Parsed form of one command.
            if (!parseCommand(raw, pcmd)) {                            // Parse/validate syntax.
                print_error();
                continue;
            }

            if (pcmd.hasRedirection) {                                 // Built-ins should not use > here.
                const string& name = pcmd.args[0];                     // Command name.
                if (name == "exit" || name == "cd" || name == "path") {
                    print_error();
                    continue;
                }
            }

            if (runBuiltin(pcmd, paths)) {                             // Handle built-ins in parent.
                continue;
            }

            executeExternal(pcmd, paths, children);                    // Launch non-built-in command.
        }

        for (pid_t pid : children) {                                   // Wait for all launched children.
            int status = 0;                                            // Receives child exit status.
            while (waitpid(pid, &status, 0) < 0) {                     // Retry wait on interrupt.
                if (errno != EINTR) {                                  // Stop retry for other errors.
                    break;
                }
            }
        }
    }

    return 0;                                                          // Unreached, but valid C++ main end.
}