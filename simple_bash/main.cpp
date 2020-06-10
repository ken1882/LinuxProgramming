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
const int IOR_NONE = 0; // No I/O redirect
const int IOR_OUT  = 1; // Redirect stdout in overwrite mode
const int IOR_APP  = 2; // Redirect stdout in append mode
const int IOR_IN   = 3; // Redirect stdin
const int IOR_PIPE = 4; // Bi-directional redirect two process

struct ProcInfo{
    std::string pname;
    std::string ppath;
    std::vector<std::string> args;
    int file_flag;
    int io_flag;

    ProcInfo(std::string pn, std::vector<std::string> _args,
             int fflag, int iflag)
    {
        pname = pn;
        args  = _args;
        file_flag = fflag;
        io_flag   = iflag;
    }
};

void sig_handler(int signo){
    std::cout << "Recived singal " << signo << '\n';
}

std::string get_cwd(){
    char buffer[0xfff];
    std::string ret = "";
    if(getcwd(buffer, sizeof(buffer)) != NULL){
        ret = buffer;
    }
    else{
        std::cout << "An error occurred during getting pwd!\n";
        std::cout << strerror(errno) << '\n';
    }
    return ret;
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
    ret.push_back(get_cwd());
    return ret;
}

bool find_program(std::string pname, std::string& out){
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
    return found;
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

// Should only be called by child process
void execute_program(std::string path, std::vector<std::string> args,
             int fd_in, int fd_out){

    if(FLAG_DEBUG){
        printf("Running %s with args:\n", path.c_str());
        for(auto arg:args){
            std::cout << arg << ' ';
        }
        std::cout << "\n-------\n";
    }
    std::vector<char*> arr;
    std::transform(
        std::cbegin(args), std::cend(args), std::back_inserter(arr),
        [](auto& str){ return const_cast<char*>(str.c_str()); }
    );
    arr.push_back(NULL);
    const std::vector<char*> const_arr(arr);
    char* const* child_argv = const_arr.data();

    close(STDOUT_FILENO);
    if(fd_in >= 0){
        close(STDIN_FILENO);
        dup2(fd_in, STDIN_FILENO);
    }
    if(fd_out >= 0){dup2(fd_out, STDOUT_FILENO);}

    execv(path.c_str(), child_argv);
    printf("Child process failed to execute process %s\n", path.c_str());
    exit(1);
}

void read_final_output(int fd_in, int fd_out){
    close(fd_out);
    char buffer[0xffff] = {0};
    int nbytes = 0;
    while((nbytes = read(fd_in, buffer,
                         sizeof(buffer)-sizeof(char)))
         ){
        buffer[nbytes] = '\0';
        std::cout << buffer << '\n';
    }
    wait(NULL);
}

pii determine_flags(int ntoken){
    int file_flag = -1, io_flag = -1;
    switch(ntoken){
    case RED_STDOUT:
    case RED_STDIN:
    case APP_STDOUT:
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
        file_flag = FILE_ELF;
        io_flag   = IOR_PIPE;
    }
    return std::make_pair(file_flag, io_flag);
}

int generate_proc_info(std::string pname, std::vector<std::string>& args){
    std::string ppath;
    bool found = find_program(pname, ppath);
    if(!found){
        printf("Command not found '%s'\n", pname.c_str());
        return -1;
    }
    args.insert(args.begin(), ppath);
    return 0;
}

int transform_proc_info(ProcInfo& proc){
    auto pname = proc.pname;
    auto args  = proc.args;
    if(proc.file_flag == FILE_ELF){
        int err = generate_proc_info(pname, args);
        if(err == -1){return -1;}
        proc.ppath = args[0];
        proc.args  = args;

    }
    else if(proc.file_flag == FILE_TXT){

    }
    return 0;
}

void execute_command(std::vector<ProcInfo> proces){
    int plen = proces.size();
    std::vector<int> children_pids;
    std::vector<pii> children_pipes;
    pid_t _pid = -1;
    std::string ppath;
    std::vector<std::string> args;
    int main_pipe[2];
    if(pipe(main_pipe)){
        std::cout << "An error occurred during creating pipes\n";
        std::cout << strerror(errno);
        return ;
    }
    int fd_in = -1, fd_out = -1;
    int idx = 0;
    while(idx < plen){
        transform_proc_info(proces[idx]);
        if(proces[idx].io_flag == IOR_NONE){
            fd_out = main_pipe[1];
        }

        children_pipes.push_back(fork_and_pipe(_pid));
        if(_pid == 0){
            ppath = proces[idx].ppath;
            args  = proces[idx].args;
            break;
        }
        idx++;
    }

    if(_pid == 0){
        close(main_pipe[0]);
        execute_program(ppath, args, fd_in, fd_out);
    }
    else{
        read_final_output(main_pipe[0], main_pipe[1]);
    }
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
    std::vector<ProcInfo> procs;
    for(int i=0;i<f_len;++i){
        procs.push_back(ProcInfo(files[i], arguments[i],
                                 file_flags[i], io_flags[i]));
    }
    execute_command(procs);
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
