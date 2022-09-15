#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    //create copies of std in and out using dup()
    int std_in = dup(0);
    int std_out = dup(1);
    vector <pid_t> bg_pid;
    char buffer[1024];
    string curr_dir = getcwd(buffer, sizeof(buffer)); //saves dir for cd -
    string prev_dir = curr_dir;

    for (;;) {
        //implement iteration over vector of background pid (declare outside of loop)
        //waitpid() - using flag for non-blocking
        for (size_t i = 0; i < bg_pid.size(); i++){
            int status = 0;
            if(waitpid(bg_pid[i], &status, WNOHANG) == bg_pid[i]){
                bg_pid.erase(bg_pid.begin() + i);
                i--;
            }
        }

        dup2(std_in,0);
        dup2(std_out,1);
        // need date/time, username, and absolute path to current dir
        //implement username with getenv("USER")
        //implament date/time with time()
        //implement curdir with getcwd()
        time_t time_curr;
        time (&time_curr);
        string timeS(ctime(&time_curr));

        cout << YELLOW << timeS.substr(0, timeS.length() -1) << " " << getenv("USER") << ":" << getcwd(buffer, sizeof(buffer)) << "$" <<NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        //chdir()
        //if dir (cd <dir>) is "-" then go to previous working directory 
        //variable storing previouse working directory (declare outside loop)
        //cd - along another cd - will bounce back and fourth
        if (input.find("cd") == 0){ //0 for start location of cd in input
            curr_dir = getcwd(buffer,sizeof(buffer));
            if (input.find("-") == 3){//3 for start location of - in input
                chdir(prev_dir.c_str());
                prev_dir = curr_dir;
            }else{
                string dir_name = input.substr(3);
                // cout << dir_name << endl;
                chdir(dir_name.c_str());
                prev_dir = curr_dir;
            }
            continue;
        }
        //cd works

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }


        // print out every command token-by-token on individual lines
        // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }
        //for piping
        //for cmd : commands
        //      call pipe() to make pipe
        //      fork() - in child, redirect stdout; in par, redirect stdin
        //      ^ already written
        //add checks for first and last command
        
        for (auto cmd : tknr.commands){
            int fds[2];
            if(pipe(fds) < 0){
                perror("pipe");
                exit(2);
            }

            // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }
            
            //add check for background process - add pid to vector if background and dont waitpid() in parent
            if (pid == 0) {  // if child, exec to run command
                //cerr << "are we here" << endl;
                //currently singly argument
                //implement multiple arguments - iterate over args of current command to make 
                //char* array
                char** args = new char* [cmd->args.size()+1];
                for (size_t i = 0; i < cmd->args.size(); i++){
                    args[i] = (char*) cmd->args.at(i).c_str();
                }
                args[cmd->args.size()] = nullptr;
                //works i think
                
                // if current command is redirected then open file and dup2 std in/out being redirected
                // implement it safely for both at same time
                if(cmd->hasOutput()){//write
                    int fd = open(cmd->out_file.c_str(), O_WRONLY|O_CREAT,S_IWUSR|S_IRUSR);
                    dup2(fd, 1);
                    close(fd);
                } //works

                if(cmd->hasInput()){//read
                    int fd = open(cmd->in_file.c_str(), O_RDONLY,S_IWUSR|S_IRUSR);
                    dup2(fd, 0);
                    close(fd);
                } //works

                if(cmd != tknr.commands.back()){
                   dup2(fds[1], 1);//stdout(1) into write
                   close(fds[0]);
                }

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
                delete[] args;


            }
            else {  // if parent, wait for child to finish
                if (cmd->isBackground()){
                    bg_pid.push_back(pid);
                }else{
                    dup2(fds[0], 0);
                    close(fds[1]);
                    int status = 0;
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }
            }
            //restore stdin and stdout
            close(fds[0]);
            close(fds[1]);
            if(pid == 0){
                int status = 0;
                waitpid(bg_pid.back(), &status, 0);
            }
        }
    }
}