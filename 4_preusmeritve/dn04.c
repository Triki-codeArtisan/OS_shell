#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>


#define MAX_TOKENS 30
#define MAX_TOKEN_LEN 100
char tokens[MAX_TOKENS][MAX_TOKEN_LEN]; // kar stringi da bo cim bolj izi, sej (za moderni cas) ni blazno velike porabe prostora
int tokenCount = 0;
char vhod[300];
char* args[MAX_TOKENS];
char ukaz_brez_redir[MAX_TOKENS - 3][MAX_TOKEN_LEN];
char redirects[3][MAX_TOKEN_LEN];
int stUkaza;

// te je po vsakem ukazu treba ponastavit !!
int inRedirect = 0;
int outRedirect = 0;
int bgRun = 0;
int stRedir = 0;
int stArg = 0; // argumenti so vse kar ni ukaz ali preusmeritev

char promptName[9] = {"mysh"};
int lastCmdExitStatus = 0;

char procfs_path[1024] = "/proc";

////////////////////////////////////////////////////////////////////////
int tokenize() {
    // tokene bom hranil kot stringe za ease of use
    int ivhoda = 0;
    int prev_space = 1;
    int end1 = 0;
    while(!end1) {
        if(vhod[ivhoda] == '#' && prev_space){
            end1 = 1;

        } else if(vhod[ivhoda] == '"' && prev_space) {
            prev_space = 0;
            ivhoda++;
            int i = 0;
            int prvi = 0;
            while(1){
                if(vhod[ivhoda] == '"' ) {
                    prev_space = 0;
                    if(!prvi) {
                        break;
                    }
                    tokens[tokenCount][i] = '\0'; 
                    tokenCount++;
                    break;
                }
                prvi = 1;
                tokens[tokenCount][i] = vhod[ivhoda];
                i++;
                ivhoda++;
            }

            
        } else if (vhod[ivhoda] != ' ' && prev_space) {
            int i = 0;
            int prvi = 0;
            while(1){
                if(vhod[ivhoda] == ' ' || vhod[ivhoda] == '\n' || vhod[ivhoda] == '\0') {
                    if(!prvi) {
                        break;
                    }
                    tokens[tokenCount][i] = '\0'; 
                    tokenCount++;
                    break;
                }
                prvi = 1;
                tokens[tokenCount][i] = vhod[ivhoda];
                i++;
                ivhoda++;
            }

        } else if (vhod[ivhoda] == ' ') {
            prev_space = 1;
        } 
        
        ivhoda++;
        

        if(vhod[ivhoda] == '\0') {
            end1 = 1;
        }

    } // end of raw tokenization

    if(vhod[strlen(vhod)-1] == '\n'){
        vhod[strlen(vhod)-1] = '\0';
    } // cleaning vhod for easy print

    ////////////////////////////////////////////
    // need to indetify redirects and set stArg
    int i_last3tokens = 0;
    if(tokenCount > 3) {
        i_last3tokens = tokenCount - 3;
    }
    
    for(int iii = i_last3tokens; iii < tokenCount; iii++){

        if (tokens[iii][0] == '<') { 
            inRedirect = 1; 
            stRedir++;;
            strcpy(redirects[0], tokens[iii] +1 ); // brez <

        }
        else if (tokens[iii][0] == '>') { 
            outRedirect = 1;
            stRedir++;;
            strcpy(redirects[1], tokens[iii] +1 ); // brez >

        }
        else if (tokens[iii][0] == '&') { 
            bgRun = 1; 
            stRedir++;;
            redirects[2][0] = '&';
            redirects[2][1] = '\0';
        }
    }
    stArg = tokenCount - 1 - stRedir;

    // prikladno imet
    strcpy(ukaz_brez_redir[0], tokens[0]);
    for(int ii = 1; ii <= stArg; ii++) {
        strcpy(ukaz_brez_redir[ii], tokens[ii]);
    }
    
    return 0;
}

void printTokens() {
    for (int i = 0; i < tokenCount; i++) {
        printf("%s ", tokens[i]);
    }
    printf("\n");
}

void tokensToArgs() {
    // args[1] je pointer na prvi znak v tokens[i]
    for (int i = 0; i < tokenCount - stRedir; i++) {
        args[i] = tokens[i];  
    }
    args[tokenCount] = NULL;  // Null-terminate ker to rabi execv
}

///////////////////////////////////////////////////////////////////////
// iskanje in klicanje ukazov

//to je seznam podprtih ukazov
typedef struct {
    const char* name;
    int (*func)();
} BuiltinCmd;
                    // kar vrne find_builtin()
int myhelp();       // 0
int mydebug();      // 1
int myprompt();     // 2
int mystatus();     // 3 
int myexit();       // 4
int myprint();      // 5
int myecho();       // 6
int mylen();        // 7
int mysum();        // 8
int mycalc();       // 9
int mybasename();   // 10
int mydirname();    // 11
int mydirch();      // 12
int mydirwd();      // 13
int mydirmk();      // 14
int mydirrm();      // 15
int mydirls();      // 16
int myrename();     // 17
int myunlink();     // 18
int myremove();     // 19
int mylinkhard();   // 20
int mylinksoft();   // 21
int mylinkread();   // 22
int mylinklist();   // 23
int mycpcat();      // 24
int mypid();        // 25
int myppid();       // 26
int myuid();        // 27
int myeuid();       // 28
int mygid();        // 29
int myegid();       // 30
int mysysinfo();    // 31
int myproc();       // 32
int mypids();       // 33
int mypinfo();      // 34
int mysleep();      // 35
int mywaitone();    // 36
int mywaitall();    // 37


BuiltinCmd builtins[] = {
    { "help",       myhelp },     // 0
    { "debug",      mydebug },    // 1
    { "prompt",     myprompt },   // 2
    { "status",     mystatus },   // 3
    { "exit",       myexit},      // 4
    { "print",      myprint},     // 5 
    { "echo",       myecho},      // 6
    { "len",        mylen},       // 7
    { "sum",        mysum},       // 8
    { "calc",       mycalc},      // 9
    { "basename",   mybasename},  // 10
    { "dirname",    mydirname},   // 11
    { "dirch",      mydirch},     // 12 ***
    { "dirwd",      mydirwd},     // 13
    { "dirmk",      mydirmk},     // 14
    { "dirrm",      mydirrm},     // 15
    { "dirls",      mydirls},     // 16
    { "rename",     myrename},    // 17
    { "unlink",     myunlink},    // 18
    { "remove",     myremove},    // 19
    { "linkhard",   mylinkhard},  // 20
    { "linksoft",   mylinksoft},  // 21
    { "linkread",   mylinkread},  // 22
    { "linklist",   mylinklist},  // 23
    { "cpcat",      mycpcat},     // 24
    { "pid",        mypid},       // 25
    { "ppid",       myppid},      // 26
    { "uid",        myuid},       // 27
    { "euid",       myeuid},      // 28
    { "gid",        mygid},       // 29
    { "egid",       myegid},      // 30
    { "sysinfo",    mysysinfo},   // 31
    { "proc",       myproc},      // 32
    { "pids",       mypids},      // 33
    { "pinfo",      mypinfo},     // 34
    { "sleep",      mysleep},     // 35
    { "waitone",    mywaitone},   // 36
    { "waitall",    mywaitall}    // 37
};


//mora vrnit cifro ukaza ce je builtin, sicer vrne 1 
int find_builtin() {   
    char* ukaz = tokens[0];
    int stBuiltins = sizeof(builtins) / sizeof(BuiltinCmd); // st podprtih ukazov
    for (int i = 0; i < stBuiltins; i++) {
        if (strcmp(ukaz, builtins[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

int execute_builtin(int index) {
    if (index >= 0 && index < (sizeof(builtins) / sizeof(BuiltinCmd))) {
        return builtins[index].func();
    }
    return -1; // or error
}


//////////////////////////////////////////////////////////////////////////
// COMMAND FUNCTIONS
/////////////////  repl  ////////////////////
int myhelp() {
    printf("Available commands:\n");
    printf("help         - Show this help\n");
    printf("debug [lvl]  - Get/set debug level (0 or 1)\n");
    printf("prompt [str] - Show or change the shell prompt (max 8 chars)\n");
    printf("status       - Show the exit status of the last command\n");
    printf("exit [code]  - Exit the shell with optional exit status\n");
    printf("print ...    - Print arguments without newline\n");
    printf("echo ...     - Print arguments with newline\n");
    printf("len ...      - Print total length of all arguments\n");
    printf("sum ...      - Sum all numeric arguments\n");
    printf("calc a op b  - Perform a simple arithmetic operation (+ - * / %% )\n");
    printf("basename path- Extract filename from path\n");
    printf("dirname path - Extract directory from path\n");
    printf("dirch [path] - Change current directory (default /)\n");
    printf("dirwd [mode] - Show current directory ('base' or 'full')\n");
    printf("dirmk dir    - Create a directory\n");
    printf("dirrm dir    - Remove an empty directory\n");
    printf("dirls [path] - List contents of a directory\n");
    printf("rename old new - Rename a file or directory\n");
    printf("unlink file  - Unlink a file\n");
    printf("remove file  - Remove a file (alias for unlink)\n");
    printf("linkhard src dest - Create a hard link\n");
    printf("linksoft src dest - Create a symbolic link\n");
    printf("linkread path - Read symlink target\n");
    printf("linklist path - List symlink status\n");
    printf("cpcat src dst - Copy contents of src to dst\n");

    fflush(stdout);
    return 0;
}

int debug_level = 0; // global var sa si zaponi zadnji level
int mydebug() { // 1
    // >debug level (0/1 arg)
    if (tokenCount - stRedir == 1) {printf("%d\n", debug_level); fflush(stdout);}
    int level = -1;
    if(tokenCount - stRedir >= 2) {
        level = atoi(tokens[1]);
    }
    if(level > -1) {debug_level = level;} //level == -1 ce arg ni podan
    
    return 0;
}
int printDebug() { //pomozna funkcija
    if(debug_level > 0) {
        // print input line 
        printf("Input line: '%s'\n", vhod); fflush(stdout);     

        // print tokens
        for(int i = 0; i < tokenCount; i++){
            printf("Token %d: '%s'\n", i, tokens[i]);fflush(stdout);   
        }
        
        // //print redirections
        if(inRedirect) { printf("Input redirect: '%s'\n", redirects[0]);}
        if(outRedirect){ printf("Output redirect: '%s'\n", redirects[1]);}
        if(bgRun)      { printf("Background: 1\n"); } fflush(stdout);   

        // builtin or external
        char* fg_bg = bgRun ? "background" : "foreground";
        if(stUkaza >= 0){
            printf("Executing builtin '%s' in %s\n", builtins[stUkaza].name, fg_bg);
        } else {
            printf("External command '");
            for(int i = 0; i < stArg +1; i++) {
                if(i == stArg) {printf("%s'\n", ukaz_brez_redir[i]); }
                else           {printf("%s ",  ukaz_brez_redir[i]); }
            }
        }
        fflush(stdout);   
    }
} 

int myprompt() { // 2
    // >prompt name (0/1 arg)
    if(tokenCount - stRedir == 1) {
        printf("%s\n", promptName);fflush(stdout);   
    } else if (tokenCount - stRedir >= 2) {
        if(strlen(tokens[1]) <= 8) {
            strcpy(promptName, tokens[1]);
        } else {
            return 1;
        }
    }
    return 0;
}

int mystatus() { // 3
    // izpise se izhodni status zadnjega izvedenega ukaza
    // >status (0 arg)
    printf("%d\n", lastCmdExitStatus); fflush(stdout);   
    return lastCmdExitStatus; // izjemoma se ta ne steje za vracanje statusa
}

int myexit(){ // 4
    // >exit status (0/1 arg)
    if(tokenCount - stRedir == 1) {
        exit(lastCmdExitStatus);
    } else if (tokenCount - stRedir == 2) {
        int s = atoi(tokens[1]);
        exit(s);
    }
}

int myprint(){ // 5
    for(int i = 1; i < tokenCount - stRedir; i++){
        if(i == tokenCount - 1 - stRedir) { printf("%s", tokens[i]); }   
        else {printf("%s ", tokens[i]);}
    }   fflush(stdout);   
    return 0;
}

int myecho(){ // 6
    if(tokenCount - stRedir == 1) {printf("\n");}
    for(int i = 1; i < tokenCount - stRedir; i++){
        if(i == tokenCount -1 - stRedir) { printf("%s\n", tokens[i]); }
        else {printf("%s ", tokens[i]);}
    }   fflush(stdout);   
    return 0;
}

int mylen() {  // 7
    int vsota = 0;
    for(int i = 1; i < tokenCount - stRedir; i++){
        vsota += strlen(tokens[i]);
    }
    printf("%d\n", vsota); fflush(stdout);   
    return 0;
} 

int mysum(){  // 8
    if(tokenCount == 0) {
        printf("Invalid command\n");
        return 1;
    }
    int vsota = 0;
    for(int i = 1; i < tokenCount - stRedir; i++){
        vsota += atoi(tokens[i]);
    }
    printf("%d\n", vsota);
    fflush(stdout);   
    return 0;
} 

int mycalc(){  // 9
    if(tokenCount - stRedir != 4) {
        printf("invalid command\n");
        return 1; // nedovoljeno stevilo ukazov
    }
    int rez = 0;
    int a = atoi(tokens[1]);
    int b = atoi(tokens[3]);
    if(strlen(tokens[2]) != 1) {"Invalid operator\n"; return 1;}
    char op = tokens[2][0];
    switch (op) {
        case '+': rez = a + b; break;
        case '-': rez = a - b; break;
        case '*': rez = a * b; break;
        case '/': 
            if(b == 0) {printf("Devide by 0 error\n"); return 2;}
            rez = b != 0 ? a / b : 0; break;
        case '%': 
            if(b == 0) {printf("Devide by 0 error\n"); return 2;}
            rez = a%b; break;
        default:
            printf("Invalid operator.\n");
            return 1;
    }
    printf("%d\n", rez);
    fflush(stdout);   
    return 0;
} 
/////////////////  imeniki & datoteke  ////////////////////
int mybasename(){  // 10
    if(tokenCount - stRedir != 2) {
        return 1;
    }
    char* path = tokens[1];
    char* last_slash = strrchr(path, '/'); // pointer na zadni /
    if (last_slash == NULL) {return 2; }
    char* file; file = last_slash + 1; //kazalec na prvi znak za /
    printf("%s\n", file);
    
} 

int mydirname(){  // 11
    if(tokenCount - stRedir != 2) {
        return 1;
    }
    char path[MAX_TOKEN_LEN];
    char* fullpath = tokens[1];
    char* last_slash = strrchr(fullpath, '/'); // pointer na zadni /
    if (last_slash != NULL) {
        // lustn trik ker sta oba kazalca na char
        size_t path_len = last_slash - fullpath;  // number of chars before the last '/'
        strncpy(path, fullpath, path_len);
        path[path_len] = '\0'; 

        printf("%s\n", path);
        fflush(stdout);   
    } else {
        return 2;
    }   
    return 0;
    
} 

int mydirch() {
    char cwd[1024];
    
    if(debug_level >= 2) {
        getcwd(cwd, sizeof(cwd));
        printf("before: %s\n", cwd); 
    }

    if (tokenCount - stRedir == 1) {
        if (chdir("/") != 0) {
            int err = errno;
            perror("dirch");
            fflush(stderr); 
            return err;
        
        }
        
    } else {
        char* path = tokens[1];
        if (chdir(path) != 0) {
            int err = errno;
            perror("dirch");
            fflush(stderr); 
            return err;
            
        }
    }

    if(debug_level >= 2) {
        getcwd(cwd, sizeof(cwd));
        printf("after: %s\n", cwd); 
    }

    return 0;
}

int dirch_private(char* path){
    if (chdir(path) != 0) {
        int err = errno;
        perror("dirch_private");
        fflush(stderr); 
        return err;
        
    }
}

int mydirwd() { // 13
    char* mode;
    if (tokenCount - stRedir == 1) { mode = "base";}
    else                 { mode = tokens[1]; }
    
    char cwd[1024];
    if(!strcmp(mode, "base")) { //strcmp vrne 0 ce sta enaka
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            if(!strcmp(cwd, "/")) { //ce je root moras izpisat ta /
                printf("/\n"); 
            } else {
                char* last_slash = strrchr(cwd, '/');
                printf("%s\n", last_slash +1);
            }
            fflush(stdout);   
            
        } else {
            int err = errno;
            perror("getcwd() error");
            fflush(stderr); 
            return err;
            
        }

    } else if(!strcmp(mode, "full")) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
            fflush(stdout);   
        } else {
            int err = errno;
            perror("getcwd() error");
            fflush(stderr); 
            return err;
            
        }
    }
    return 0;
        
} 

int mydirmk() { // 14
    if(tokenCount - stRedir != 2) {
        printf("invalid command");
        fflush(stdout); 
        return 1;
        
    } else {
        char* dirname = tokens[1];
        if (mkdir(dirname, 0755) == 0) {
            // kul
        } else {
            int err = errno;
            perror("dirmk");
            fflush(stderr); 
            return err;
            
        }
    }
    return 0;
}  

int mydirrm() { // 15
    if(tokenCount - stRedir!= 2) {
        printf("invalid command");
        fflush(stdout); 
        return 1;
        
    } else {
        char* dirname = tokens[1];
        //printf("Trying to remove: '%s'\n", dirname); fflush(stdout);
        if (rmdir(dirname) == 0) {
            // kul
        } else { 
            int err = errno;
            perror("dirrm");
            fflush(stderr); 
            return err;
            
        }
    }
    return 0;
}  

int mydirls() { // 16
    char path[1024] = {"./"};
    char cwd[1024];
    

    if(tokenCount - stRedir >= 2) {
        if(getcwd(cwd, sizeof(cwd)) == NULL){
            perror("getcwd"); 
            fflush(stderr); 
            return errno;
        
        }
        if(strcpy(path + 2, tokens[1]) == NULL) { perror("strcpy"); fflush(stderr);  return errno;   }
        int succ = dirch_private(path);
        if(succ != 0) {
            return succ;
        }
    }

    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        fflush(stderr); 
        return 1;
    }

    struct dirent *entry;
    char *names[1024]; // max 1024 files
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < 1024) {
        names[count++] = strdup(entry->d_name); // save entry name
    }

    // Print all names with 2 spaces between, except after last
    for (int i = 0; i < count; i++) {
        printf("%s", names[i]);
        fflush(stdout);
        if (i != count - 1) printf("  ");
        free(names[i]); // cleanup strdup
    }
    printf("\n");
    fflush(stdout);

    closedir(dir);

    // Go back to original directory
    if (tokenCount - stRedir >= 2) {
        int s = dirch_private(cwd); // move back
        if (s != 0) return s;
    }

    return 0;
    
}  

int myrename() {
    if (tokenCount - stRedir!= 3) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    if (rename(tokens[1] , tokens[2]) != 0) {
        int err = errno;
        perror("rename");
        fflush(stderr); 
        return err;
    }

    if(debug_level >= 2) {
        printf("rename succesful\n");
        fflush(stdout);
    }


    return 0;
}

int myunlink() {
    if (tokenCount - stRedir != 2) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    if(unlink(tokens[1]) == -1){
        int err = errno;
        perror("unlink");
        fflush(stderr);
        return err;
    }

    if(debug_level >= 2) {
        printf("unlink succesful\n");
        fflush(stdout);
    }

    return 0;
}

int myremove() {
    if (tokenCount  - stRedir != 2) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    if(unlink(tokens[1]) == -1){
        int err = errno;
        perror("remove");
        fflush(stderr);
        return err;
    }

    if(debug_level >= 2) {
        printf("remove succesful\n");
        fflush(stdout);
    }

    return 0;
}

int mylinkhard() {
    if (tokenCount - stRedir != 3) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    if (link(tokens[1], tokens[2]) == -1) {
        int err = errno;
        perror("linkhard");
        fflush(stderr);
        return err;
    }

    if(debug_level >= 2) {
        printf("linkhard succesful\n");
        fflush(stdout);
    }

    return 0;
}

int mylinksoft() {
    if (tokenCount - stRedir != 3) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    if (symlink(tokens[1], tokens[2]) == -1) {
        int err = errno;
        perror("linksoft");
        fflush(stderr);
        return err;
    }

    if(debug_level >= 2) {
        printf("linksoft succesful\n");
        fflush(stdout);
    }

    return 0;
}

int mylinkread() {
    if (tokenCount - stRedir != 2) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    char path[1024];

    int len;
    if ((len = readlink(tokens[1], path, sizeof(path) -1)) == -1) {
        int err = errno;
        perror("readlink");
        fflush(stderr);
        return err;
    }
    path[len] = '\0';

    if(debug_level >= 2) {
        printf("readlink succesful\n");
        fflush(stdout);
    }

    printf("%s\n", path);

    return 0;
}

int mylinklist() {
    if(tokenCount - stRedir != 2) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    DIR *dir = opendir(".");  
    if (dir == NULL) {
        int err = errno;
        perror("opendir");
        return err;
    }

    struct stat target_stat;
    if (stat(tokens[1], &target_stat) == -1) {  // Pridobi informacije o ciljni datoteki
        int err = errno;
        perror("stat");
        closedir(dir);
        return err;
    }

    ino_t target_inode = target_stat.st_ino;  // Inode ciljne datoteke
    int first = 1;  // Spremenljivka za formatiranje izpisa (za ločitev z dvema presledkoma)

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Preskoči . in ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat statbuf;
        if (stat(entry->d_name, &statbuf) == -1) {  // Pridobi informacije o datoteki v imeniku
            perror("stat");
            continue;
        }

        // Preveri, če se inode ujema z inode ciljne datoteke
        if (statbuf.st_ino == target_inode) {
            if (!first) {
                printf("  ");  // Izpisi dva presledka med povezavami
            }
            printf("%s", entry->d_name);  // Izpisi ime datoteke
            first = 0;  // Po prvem izpisu omogoči ločevanje z dvema presledkoma
        }
    }

    printf("\n");  // Nova vrstica po izpisu vseh trdnih povezav
    closedir(dir);  // Zapri imenik
    return 0;
}

int mycpcat() {

    if(tokenCount - stRedir > 3) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    }

    int sourceFD, destFD;
    char buffer[1024];
    ssize_t bytesRead, bytesWritten;

    if(tokenCount - stRedir == 1 || strcmp(tokens[1], "-") == 0) {
        sourceFD = 0;
    } else {
        sourceFD = open(tokens[1], O_RDONLY);
    }

    if (sourceFD == -1) {
        int err = errno;
        perror("cpcat");
        fflush(stderr);
        return err;
    }

    if(tokenCount - stRedir == 2 || tokenCount - stRedir == 1) {
        destFD = 1;

    } else if (tokenCount - stRedir == 3) {
        destFD = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (destFD == -1) {
            int err = errno;
            perror("open");
            fflush(stderr);
            close(sourceFD); // Close the source file before exiting
            return err;
        }
    }


    while ((bytesRead = read(sourceFD, buffer, sizeof(buffer))) > 0) {
        // if(tokenCount - stRedir == 2 || tokenCount - stRedir == 1) {
            bytesWritten = write(destFD, buffer, bytesRead); // to stdout
            if (bytesWritten == -1) {
                int err = errno;
                perror("write");
                fflush(stderr);
                close(sourceFD);
                close(destFD);
                return err;
            }
        // }

        // if (tokenCount - stRedir == 3) {
        //     bytesWritten = write(destFD, buffer, bytesRead); // to destination file
        //     if (bytesWritten == -1) {
        //         int err = errno;
        //         perror("write");
        //         fflush(stderr);
        //         close(sourceFD);
        //         close(destFD);
        //         return err;
        //     }
        // }
    }

    if (bytesRead == -1) {
        int err = errno;
        perror("read");
        fflush(stderr);
        return err;
    }

    // Close both files
    if(sourceFD != 0) {close(sourceFD);}
    if(destFD != 1)   {close(destFD);}
    return 0;
}
/////////////////  procesi  ////////////////////
int mypid() { // 25
    printf("%d\n", getpid());
    fflush(stdout);
    return 0;
}    

int myppid() { // 26
    printf("%d\n", getppid());
    fflush(stdout);
    return 0;
}     

int myuid() { // 27
    printf("%d\n", getuid());
    fflush(stdout);
    return 0;
}     

int myeuid() { // 28
    printf("%d\n", geteuid());
    fflush(stdout);
    return 0;
}    

int mygid() { // 29
    printf("%d\n", getgid());
    fflush(stdout);
    return 0;
}    

int myegid() { // 30
    printf("%d\n", getegid());
    fflush(stdout);
    return 0;
}    

int mysysinfo(){ // 31
    struct utsname buf; // init strukture
    if(uname(&buf) == 0) {
        // dostop do komponent strukrue
        printf("Sysname: %s\n", buf.sysname); 
        printf("Nodename: %s\n", buf.nodename);
        printf("Release: %s\n", buf.release);
        printf("Version: %s\n", buf.version);
        printf("Machine: %s\n", buf.machine);
        fflush(stdout);

    } else {
        int err = errno;
        perror("uname");
        fflush(stderr);
        return err;
    }

    return 0;
}    

int myproc() { // 32

    if(tokenCount - stRedir >= 3) {
        printf("wrong number of arguments\n");
        fflush(stdout);
        return 1;
    } else if(tokenCount - stRedir == 1 ) {
        printf("%s\n", procfs_path);
        fflush(stdout);
        return 0;
    } else if (tokenCount - stRedir == 2) {
        if(access(tokens[1], F_OK|R_OK) != 0) {
            if (debug_level >= 2) {
                printf("invalid path - access");
                fflush(stdout);
            }
            return 1;
        }

        if(strcpy(procfs_path, tokens[1]) == NULL){
            int err = errno;
            perror("strcpy");
            fflush(stderr);
            return err;
        }
    }
    return 0; 
} 

int  isInt(const char* str) { //pomozna funkcija
    if (str == NULL || *str == '\0') return 0;

    // If it's just "0", accept it
    if (str[0] == '0' && str[1] == '\0') return 1;

    // Reject if it starts with '0' and is longer than 1 digit
    if (str[0] == '0') return 0;

    // Check all characters are digits
    for (int i = 0; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) return 0;
    }

    return 1;
}
int compare_ints(const void* a, const void* b) { //pomozna funkcija
    return (*(int*)a - *(int*)b);
}
int mypids() { // 33
    DIR* dir = opendir(procfs_path);
    if (dir == NULL) {
        perror("opendir");
        return errno;
    }

    int* pids = malloc(128 * sizeof(int)); // initial size is 128
    if (!pids) {
        perror("malloc");
        closedir(dir);
        return 1;
    }
    size_t count = 0;
    size_t capacity = 128;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        if (isInt(entry->d_name)) {
            int pid = atoi(entry->d_name);
            if (count >= capacity) {
                capacity *= 2;
                int* temp = realloc(pids, capacity * sizeof(int));
                if (!temp) {
                    perror("realloc");
                    free(pids);
                    closedir(dir);
                    return 1;
                }
                pids = temp;
            }
            pids[count++] = pid;
        }
            
    }

    closedir(dir);

    qsort(pids, count, sizeof(int), compare_ints);
    for (size_t i = 0; i < count; i++) {
        printf("%d\n", pids[i]);
        fflush(stdout);
    }

    free(pids);

    return 0;
} 

typedef struct {
    int pid;
    int ppid;
    char state;
    char name[1024];
} ProcessInfo;

int compare(const void *a, const void *b) { //pomozna funkcija
    ProcessInfo *p1 = (ProcessInfo *)a;
    ProcessInfo *p2 = (ProcessInfo *)b;
    return p1->pid - p2->pid;
}
int getProcessInfo(int pid, ProcessInfo *info) { //pomozna funkcija
    char path[1024];
    snprintf(path, PATH_MAX, "%s/%d/stat", procfs_path ,pid);
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening stat file");
        return -1;  
    }

    char buffer[1024];
    ssize_t bytesRead = read(fd, buffer, 1023);
    if (bytesRead == -1) {
        perror("Error reading stat file");
        close(fd);
        return -1;
    }
    buffer[bytesRead] = '\0';  // Null-terminate the buffer

    // Parse the buffer
    int ppid;
    char state;
    char name[1024];
    
    int numFields = sscanf(buffer, "%d %s %c %d", &info->pid, name, &state, &ppid);
    if (numFields != 4) {
        close(fd);
        return -1;  // Error parsing the file
    }

    // Clean the name (remove parentheses)
    name[strlen(name) - 1] = '\0';  // Remove closing parenthesis
    memmove(name, name + 1, strlen(name));  // Remove opening parenthesis
    strcpy(info->name, name);

    // Store other process information
    info->ppid = ppid;
    info->state = state;

    // Close the file descriptor after reading
    close(fd);

    return 0;
}
int mypinfo() { // 34
    DIR *dir = opendir(procfs_path);
    if (dir == NULL) {
        perror("opendir");
        return errno;
    }

    ProcessInfo processes[PATH_MAX];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        char *endptr;
        int pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;  // Not a valid PID
        }

        if (getProcessInfo(pid, &processes[count]) == 0) {
            count++;
        }
    }
    closedir(dir);
    qsort(processes, count, sizeof(ProcessInfo), compare);

    printf("%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");
    for (int i = 0; i < count; i++) {
        printf("%5d %5d %6c %s\n", processes[i].pid, processes[i].ppid, processes[i].state, processes[i].name);
    }


    return 0;
} 

int mysleep() { // xx
    int time = 5;
    if(tokenCount - stRedir >= 2 && isInt(tokens[1])) {
        time = atoi(tokens[1]);
    }
    sleep(time);
    return 0;
} 

int mywaitone() { // xx
    int s;
    int pid = -1;

    if (tokenCount - stRedir == 2 && isInt(tokens[1])) {
        pid = atoi(tokens[1]);
        
    } else if (tokenCount - stRedir >= 3) {
        printf("invalid number of args");
        return 1;
    }

    if(waitpid(pid, &s, 0) == -1) { // ni otrok
        return 0;

    } else if (WIFEXITED(s)){
        return WEXITSTATUS(s);
    }
} 

int mywaitall() {
    while (1) {
        int pid;
        int status;
        pid = waitpid(-1, &status, 0); // -1 waits for any child

        if (pid == -1) {
            if (errno == ECHILD) {
                break;  // No more children
            } else {
                perror("waitpid");
                break;  // Exit on error
            }
        } 
    }

    return 0;
}

// int my() { // xx
    
// } 

//////////////////////////////////////////////////////////////////

// SIGCHILD SUPPORT
void handle_sigchld(int sig) {
    // Reap all terminated children
    int pid, status, serrno;
    serrno = errno;
    while (1)
        {
        pid = waitpid (WAIT_ANY, &status, WNOHANG);
        if (pid < 0)
            {
            //perror ("waitpid");
            // no more children, all reaped
            break;
            }
        if (pid == 0)
            // no child ready to be reaped
            break;
        
        }
    errno = serrno;
}

/////////////////////////////////////////////////////////////////////////////
int main() {
    //for SIGCHILD - zombie processes
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  // avoid interrupting I/O, ignore stopped children
    sigaction(SIGCHLD, &sa, NULL);

    int end = 0;
    while(!end) {
        if(isatty(STDIN_FILENO)){
            printf("%s> ", promptName);
        }
        if(fgets(vhod, sizeof(vhod), stdin) == NULL) {
            end = 1;
            continue;
        }
        if(strlen(vhod) == 1 && vhod[0] == '\n') { // prazen ukaz
            continue; 
        }
        
        int succ = tokenize();
        stUkaza = find_builtin();
        printDebug(); // se izvede le ce je debug_level > 0
        
        if(stUkaza != -1) { 
            //builtin command
            if (!bgRun) {
                // important to save stdin and stout fd 
                int save_stdin = dup(0);   // original stdin
                int save_stdout = dup(1); // original stout

                // redirects
                fflush(stdout);
                if (inRedirect) {
                    int irfd = -1;
                    if( (irfd = open(redirects[0], O_RDONLY)) == -1) {
                        int err = errno;
                        perror("open");
                        fflush(stderr);
                        exit(err);  
                    }
                    dup2(irfd, 0);
                    close(irfd);
                }
                if(outRedirect) {
                    int irfd = -1;
                    if( (irfd = open(redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
                        int err = errno;
                        perror("open");
                        fflush(stderr);
                        exit(err);  
                    }
                    dup2(irfd, 1);
                    close(irfd);
                }

                lastCmdExitStatus = execute_builtin(stUkaza);

                fflush(stdout);
                dup2(save_stdin, 0);
                dup2(save_stdout, 1);

            } else {
                // background run
                fflush(stdin);
                pid_t pid = fork();
                if (pid == 0) {
                    // child runs the builtin in background

                    //redirects
                    fflush(stdout);
                    if (inRedirect) {
                        int irfd = -1;
                        if( (irfd = open(redirects[0], O_RDONLY)) == -1) {
                            int err = errno;
                            perror("open");
                            fflush(stderr);
                            exit(err);  
                        }
                        dup2(irfd, 0);
                        close(irfd);
                    }
                    if(outRedirect) {
                        int irfd = -1;
                        if( (irfd = open(redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
                            int err = errno;
                            perror("open");
                            fflush(stderr);
                            exit(err);  
                        }
                        dup2(irfd, 1);
                        close(irfd);
                    }
                    int s = execute_builtin(stUkaza);
                    exit(s);
                } else if (pid > 0) {
                    // parent does not wait for child in bgRun

                } else {
                    perror("fork");
                    fflush(stderr);
                }
            }
            

        } else {
            // external command
            fflush(stdin);
            pid_t pid = fork();
            if(pid > 0) {  // stars
                if (!bgRun) {  // pocakaj ce v ospredju
                    int status = -1;
                    waitpid(pid, &status, 0);
                    if(WIFEXITED(status)) {
                        lastCmdExitStatus = WEXITSTATUS(status);
                    }
                } else {  // Background - ne cakas
                    
                }

            } else if (pid == 0) {  // Child process
                //redirects
                fflush(stdout);
                if (inRedirect) {
                    int irfd = -1;
                    if( (irfd = open(redirects[0], O_RDONLY)) == -1) {
                        int err = errno;
                        perror("open");
                        fflush(stderr);
                        exit(err);  
                    }
                    dup2(irfd, 0);
                    close(irfd);
                }
                if(outRedirect) {
                    int irfd = -1;
                    if( (irfd = open(redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
                        int err = errno;
                        perror("open");
                        fflush(stderr);
                        exit(err);  
                    }
                    dup2(irfd, 1);
                    close(irfd);
                }
                

                tokensToArgs();
                int e = execvp(args[0], args);
                if(e == -1) {
                    perror("exec");
                    fflush(stderr);
                    exit(127);  // Child exits if exec fails
                }
                exit(0);

            } else {
                int err = errno;
                perror("fork");
                fflush(stderr);
                return 1;
            }
            fflush(stdout);
        }

        // reset for new command
        tokenCount = 0;
        inRedirect = 0;
        outRedirect = 0;
        bgRun = 0;
        stArg = 0;
        stRedir = 0;
        stUkaza = 0; 
    }

    return lastCmdExitStatus;
}