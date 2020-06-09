#include "main.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pty.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stack>
#include <vector>
#include <algorithm>

typedef std::pair<int,int> pii;

extern int yylex();
extern int yylineno;
extern char* yytext;
extern void lex_scan_string(const char*);
extern void lex_clear_buffer();

bool FLAG_RUNNING = true;
bool FLAG_DEBUG   = true;

const int FILE_ELF = 0; // ELF file for executing
const int FILE_TXT = 1; // Text file for I/O
const int IOR_NONE = 0;
const int IOR_OUT  = 1;
const int IOR_APP  = 2;
const int IOR_IN   = 3;

void sig_handler(int signo){
    std::cout << "Recived singal " << signo << '\n';
}

std::vector<std::string> get_PATH(){
    std::vector<std::string> ret;
    std::string full = std::getenv("PATH");
    std::string str = "";
    for(auto ch:full){
        if(ch == ':'){
            ret.push_back(str);
            str = "";
            continue;
        }
        str += ch;
    }
    if(str.length() > 1){
        ret.push_back(str);
    }
    return ret;
}

int find_program(std::string pname, std::string& out){
    auto paths = get_PATH();
    bool found = false;
    for(auto path:paths){
        DIR* cur_dir;
        if((cur_dir = opendir(path.c_str())) == NULL){
            continue;
        }
        struct dirent* fdir;
        while((fdir = readdir(cur_dir)) != NULL){
            if(fdir -> d_type != DT_REG){continue;}
            std::string fname = fdir -> d_name;
            if(fname == pname){
                out = path + "/" + fname;
                found = true; break;
            }
        }
        if(found){break;}
    }
    return found ? 0 : 1;
}

pii fork_and_pipe(int& pid_out){
    pid_out = fork();
    int _pipes[2];
    if((pipe(_pipes)) == -1){
        std::cout << "An error occurred during creating pipe\n";
        std::cout << (strerror(errno)) << '\n';
        return std::make_pair(-1, -1);
    }
    return std::make_pair(_pipes[0], _pipes[1]);
}

int execute(std::string path, std::vector<std::string> args, int p_out, int p_in){
    int main_pipe[2];
    if(pipe(main_pipe) == -1){
        std::cout << "An error occurred during creating pipe\n";
        std::cout << (strerror(errno)) << '\n';
        return 1;
    }

    pid_t pid;
    pid = fork();

    if(pid == 0){
        std::vector<char*> arr;
        std::transform(
            std::cbegin(args), std::cend(args), std::back_inserter(arr),
            [](auto& str){ return const_cast<char*>(str.c_str()); }
        );
        arr.push_back(NULL);
        const std::vector<char*> const_arr(arr);
        char* const* child_argv = const_arr.data();

        close(STDOUT_FILENO);
        dup2(main_pipe[1], STDOUT_FILENO);
        close(STDIN_FILENO);

        execv(path.c_str(), child_argv);

        printf("Child process failed to execute command, aborting\n");
        exit(1);
    }
    else if(pid < 0){
        return -1;
    }
    else{
        close(main_pipe[1]);
        char buffer[0xffff] = {0};
        int nbytes = 0;
        while((nbytes = read(main_pipe[0], buffer,
                             sizeof(buffer)-sizeof(char)))
             ){
            buffer[nbytes] = '\0';
            std::cout << buffer << '\n';
        }
        wait(NULL);
    }
    return 0;
}

pii determine_flags(int ntoken){
    int file_flag = -1, io_flag = -1;
    switch(ntoken){
    case RED_STDOUT:
    case RED_STDIN:
    case APP_STDOUT:
        new_proc = true;
        file_flag = FILE_TXT;
    }
    switch(ntoken){
    case RED_STDOUT:
        io_flag = IOR_OUT; break;
    case RED_STDIN:
        io_flag = IOR_IN; break;
    case APP_STDOUT:
        io_flag = IOR_APP; break;
    case SYM_PIPE:
        new_proc  = true;
        file_flag = FILE_ELF;
        io_flag   = IOR_OUT;
    }
    return std::make_pair(file_flag, io_flag);
}

void process_input(std::string input){
    int ntoken = 0;
    lex_scan_string(input.c_str());
    if(FLAG_DEBUG){std::cout << "Parsing tokens:\n";}
    std::vector<std::string> files;
    std::vector<std::vector<std::string>> arguments;
    std::vector<int> file_flags;
    std::vector<int> io_flags;

    bool new_proc = true;
    int f_len = 0;
    int cur_flag = FILE_ELF;
    do{
        ntoken = yylex();
        std::string operand = yytext;
        if(FLAG_DEBUG){
            std::cout << ntoken << ' ' << operand << '\n';
        }
        if(ntoken == IDENTIFIER && operand == "exit"){FLAG_RUNNING = false; break;}
        if(FLAG_DEBUG){
            printf("New proc: %d\n", new_proc);
            printf("Operand: %s\n", operand.c_str());
        }
        if(ntoken == IDENTIFIER){
            if(new_proc){
                std::vector<std::string> arglist;
                files.push_back(operand);
                arguments.push_back(arglist);
                f_len++;
                file_flags.push_back(cur_flag);
            }
            else{
                arguments[f_len-1].push_back(operand);
            }
            new_proc = false;
        }
        else{
            auto _flags = determine_flags(ntoken);
            if(_flags.first != -1){
                new_proc = true;
                cur_flag = _flags.first;
                io_flags.push_back(_flags.second);
            }
        }
    }while(ntoken);
    lex_clear_buffer();
    io_flags.push_back(IOR_NONE);

    for(int i=0;i<f_len;++i){
        std::string pname = files[i];
        auto args = arguments[i];
        if(FLAG_DEBUG){
            std::cout << "Running "<< pname << " with args:";
            for(auto arg:arguments[i]){
                std::cout << arg << ' ';
            }
            std::cout << "\n-------\n";
        }
        if(file_flags[i] == FILE_ELF){
            std::string ppath;
            int no = find_program(pname, ppath);
            if(no == -1){
                std::cout << strerror(errno) << '\n';
                return ;
            }
            args.insert(args.begin(), ppath);
            fork_execute(ppath, args);
        }
    }
}

std::string get_user_input(){
    std::string ret = "";
    std::string inp;
    bool ok = false;
    std::cout << "$";
    while(!ok){
        std::cout << "> ";
        ok = true;
        getline(std::cin, inp);
        int len = inp.length();
        if(inp[len-1] == '\\'){
            ok = false;
            ret += inp.substr(0, len-1);
        }
        else{
            ret += inp;
        }
    }
    return ret;
}

int main(int argc, char* argv[]){
    while(FLAG_RUNNING){
        if(signal(SIGINT, sig_handler) == SIG_ERR){std::cout << "An error occurred while capturing SIGINT\n";}
        std::string input = get_user_input();
        process_input(input);
    }
    return 0;
}
