#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_TOKENS 30
#define MAX_TOKEN_LEN 100
char tokens[MAX_TOKENS][MAX_TOKEN_LEN]; // kar stringi da bo cim bolj pregledna koda
int tokenCount = 0;
char vhod[300];
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
            redirects[2][0] = '\0';
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
    { "dirls",      mydirls}      // 16

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
// command functions
int myhelp() {  // 0
    printf("Available commands:\n");
    printf("help - Show this help\n");
    printf("debug [level] - Set the debug level (0 or 1)\n");
    printf("prompt [name] - Change the prompt\n");
    printf("status - Show the exit status of the last command\n");
    printf("exit [status] - Exit the shell with an optional exit status\n");
    return 0;
}

int debug_level = 0; // global var sa si zaponi zadnji level
int mydebug() { // 1
    // >debug level (0/1 arg)
    if (tokenCount == 1) {printf("%d\n", debug_level); fflush(stdout);}
    int level = -1;
    if(tokenCount >= 2) {
        level = atoi(tokens[1]);
    }
    if(level > -1) {debug_level = level;} //level == -1 ce arg ni podan
    
    return 0;
}
int printDebug() {
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
    if(tokenCount == 1) {
        printf("%s\n", promptName);fflush(stdout);   
    } else if (tokenCount >= 2) {
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
    if(tokenCount == 1) {
        exit(lastCmdExitStatus);
    } else if (tokenCount >= 2) {
        int s = atoi(tokens[1]);
        exit(s);
    }
}

int myprint(){ // 5
    for(int i = 1; i < tokenCount; i++){
        if(i == tokenCount -1) { printf("%s", tokens[i]); }   
        else {printf("%s ", tokens[i]);}
    }   fflush(stdout);   
    return 0;
}

int myecho(){ // 6
    if(tokenCount == 1) {printf("\n");}
    for(int i = 1; i < tokenCount; i++){
        if(i == tokenCount -1) { printf("%s\n", tokens[i]); }
        else {printf("%s ", tokens[i]);}
    }   fflush(stdout);   
    return 0;
}

int mylen() {  // 7
    int vsota = 0;
    for(int i = 1; i < tokenCount; i++){
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
    for(int i = 1; i < tokenCount; i++){
        vsota += atoi(tokens[i]);
    }
    printf("%d\n", vsota);
    fflush(stdout);   
    return 0;
} 

int mycalc(){  // 9
    if(tokenCount != 4) {
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
int mybasename(){  // 10
    if(tokenCount != 2) {
        return 1;
    }
    char* path = tokens[1];
    char* last_slash = strrchr(path, '/'); // pointer na zadni /
    if (last_slash == NULL) {return 2; }
    char* file; file = last_slash + 1; //kazalec na prvi znak za /
    printf("%s\n", file);
    
} 

int mydirname(){  // 11
    if(tokenCount != 2) {
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

    if (tokenCount == 1) {
        if (chdir("/") != 0) {
            int err = errno;
            perror("dirch");
            fflush(stdout); 
            return err;
        
        }
        
    } else {
        char* path = tokens[1];
        if (chdir(path) != 0) {
            int err = errno;
            perror("dirch");
            fflush(stdout); 
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
        fflush(stdout); 
        return err;
        
    }
}

int mydirwd() { // 13
    char* mode;
    if (tokenCount == 1) { mode = "base";}
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
            fflush(stdout); 
            return err;
             
        }

    } else if(!strcmp(mode, "full")) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
            fflush(stdout);   
        } else {
            int err = errno;
            perror("getcwd() error");
            fflush(stdout); 
            return err;
              
        }
    }
    return 0;
        
} 

int mydirmk() { // 14
    if(tokenCount != 2) {
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
            fflush(stdout); 
            return err;
              
        }
    }
    return 0;
}  

int mydirrm() { // 15
    if(tokenCount != 2) {
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
            fflush(stdout); 
            return err;
              
        }
    }
    return 0;
}  

int mydirls() { // 16
    char path[1024] = {"./"};
    char cwd[1024];
    

    if(tokenCount >= 2) {
        if(getcwd(cwd, sizeof(cwd)) == NULL){
            perror("getcwd"); 
            fflush(stdout); 
            return errno;
          
        }
        if(strcpy(path + 2, tokens[1]) == NULL) { perror("strcpy"); fflush(stdout);  return errno;   }
        int succ = dirch_private(path);
        if(succ != 0) {
            return succ;
        }
    }

    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        fflush(stdout); 
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
        if (i != count - 1) printf("  ");
        free(names[i]); // cleanup strdup
    }
    printf("\n");

    closedir(dir);

    // Go back to original directory
    if (tokenCount >= 2) {
        int s = dirch_private(cwd); // move back
        if (s != 0) return s;
    }

    return 0;
    
}  




/////////////////////////////////////////////////////////////////////////////
int main() {
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
        printDebug();// se izvede le ce je debug_level > 0
        
        if(stUkaza != -1) {
            lastCmdExitStatus = execute_builtin(stUkaza);

        } else {
            // izpis izvedu zunanjo funkcijo
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