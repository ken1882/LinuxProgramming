#include <iostream>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stack>
#include <vector>
#include <filesystem>
#include <algorithm>

typedef std::filesystem::directory_entry dir_entry;

namespace fs = std::filesystem;
const int MAX_TRAVERSE_DEPTH = 10;

int reg_total = 0, dir_total = 0, blk_total = 0;

enum class Color{
    BLACK = 30, RED = 31, GREEN = 32, YELLOW = 33,
    BLUE = 34, MAGENTA = 35, CYAN = 36, WHITE = 37,
    RESET = 0
};

void change_color(Color clr){
    printf("\u001b[%dm", (int)clr);
}

void change_color4mode(int fmode){
    if(S_ISLNK(fmode)){change_color(Color::GREEN);}
    else if(S_ISDIR(fmode)){++dir_total; change_color(Color::CYAN);}
    else if(S_ISCHR(fmode)){change_color(Color::MAGENTA);}
    else if(S_ISBLK(fmode)){change_color(Color::RED);}
    else if(S_ISFIFO(fmode)){change_color(Color::BLUE);}
    else if(S_ISSOCK(fmode)){change_color(Color::YELLOW);}
}

void change_color4mode(char* path){
    if(fs::is_symlink(path)){change_color(Color::GREEN); return;}
    struct stat fstat;
    if(stat(path, &fstat) == -1){
        std::cout << "Unable to get file stat for " << path << '\n';
        std::cout << strerror(errno) << '\n';
        return ;
    }
    change_color4mode(fstat.st_mode);
}

bool sort_by_name(dir_entry e1, dir_entry e2){
    return e1.path() < e2.path();
}

std::string path2filename(char* path){
    std::stack<char> _stack;
    int len = strlen(path);
    for(int i=len-1;i>=0;--i){
        if(path[i] == '/' || path[i] == '\\'){break;}
        _stack.push(path[i]);
    }
    std::string filename = "";
    while(!_stack.empty()){
        filename += _stack.top();
        _stack.pop();
    }
    return filename;
}

void print_padding(int depth, std::vector<bool> padding){
    for(int i=0;i<depth;++i){
        std::cout << (padding[i] ? '|' : ' ') << "   ";
    }
}

int print_filename(char* path){
    struct stat fstat;
    if(stat(path, &fstat) == -1){return -1;}
    int fmode = fstat.st_mode;
    std::cout << "+---";

    if(S_ISDIR(fmode)){std::cout << "+ ";}
    else{std::cout << "- ";}

    if(S_ISREG(fmode)){++reg_total;}
    blk_total += fstat.st_blocks;

    change_color4mode(path);
    std::cout << path2filename(path);
    if(fs::is_symlink(path)){
        std::cout << " -> ";
        std::string dest = fs::read_symlink(path);
        change_color4mode(const_cast<char*>(dest.c_str()));
        std::cout << dest;
    }
    std::cout << '\n';
    change_color(Color::RESET);
    return fmode;
}

int list_directory(char* path, int depth, std::vector<bool> padding){
    struct stat parent_stat;
    DIR* cur_dir;

    if(stat(path, &parent_stat) == -1){
        std::cout << "Errnor while reading stat " << path << '\n';
        return -1;
    }
    auto fmode = parent_stat.st_mode;
    //std::cout << fmode << '\n';
    //std::cout << S_ISDIR(fmode) << ' ' << S_ISREG(fmode) << ' ' << S_ISLNK(fmode) << '\n';
    if(!(cur_dir = opendir(path))){
        std::cout << "Unable to open " << path << " : " << strerror(errno) << '\n';
        return -1;
    }
    else{
        if(depth == 0){
            change_color(Color::CYAN);
            std::cout << path << '\n';
            change_color(Color::RESET);
        }
        std::vector<dir_entry> flist;
        for(auto &entry : fs::directory_iterator(path)){
            flist.push_back(entry);
        }
        std::sort(flist.begin(), flist.end(), sort_by_name);
        int len = flist.size();
        for(int i=0;i<len;++i){
            auto& entry = flist[i];
            std::string str = entry.path();
            print_padding(depth, padding);
            int _sfmode = print_filename(const_cast<char*>(str.c_str()));
            if(_sfmode == -1){return -1;}
            else if(S_ISDIR(_sfmode) && !fs::is_symlink(str)){
                auto _padding = padding;
                _padding.push_back(i == len-1 ? false : true);
                list_directory(const_cast<char*>(str.c_str()), depth+1, _padding);
                print_padding(depth+1, _padding);
                std::cout << '\n';
            }
        }
        closedir(cur_dir);
    }
    change_color(Color::RESET);
    return 0;
}

int main(int argc, char** argv){
    for(int i=1;i<argc;++i){
        std::vector<bool> padding;
        change_color(Color::RESET);
        reg_total = 0; dir_total = 0; blk_total = 0;
        int ret = list_directory(argv[i], 0, padding);
        if(ret == -1){std::cout << "\nAn Error occured!\n";}
        std::cout << "*===============\n";
        std::cout << "Total regular files: " << reg_total << '\n';
        std::cout << "Total directories: " << dir_total << '\n';
        std::cout << "Blocks used: " << blk_total << '\n';
    }
    return 0;
}
