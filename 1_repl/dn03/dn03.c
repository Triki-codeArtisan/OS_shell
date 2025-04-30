#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#define MAX_TOKENS 30
char tokens[MAX_TOKENS][100]; // kar stringi da bo cim bolj pregledna koda
int tokenCount = 0;
char vhod[250];
char ukaz_brez_redir[MAX_TOKENS - 3][100];
char redirects[3][100];
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


BuiltinCmd builtins[] = {
    { "help",       myhelp },    // 0
    { "debug",      mydebug },   // 1
    { "prompt",     myprompt },  // 2
    { "status",     mystatus },  // 3
    { "exit",       myexit}      // 4
    
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

int debug_level = 0; // global var sa si zaponi zadnji level
int mydebug() {
    // >debug level (0/1 arg)
    if (tokenCount == 1) {printf("%d\n", debug_level); }
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
        printf("Input line: '%s'\n", vhod);        

        // print tokens
        for(int i = 0; i < tokenCount; i++){
            printf("Token %d: '%s'\n", i, tokens[i]);
        }
        
        // //print redirections
        if(inRedirect) { printf("Input redirect: '%s'\n", redirects[0]);}
        if(outRedirect){ printf("Output redirect: '%s'\n", redirects[1]);}
        if(bgRun)      { printf("Background: 1\n"); } 

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
    }
}

int myprompt() {
    // >prompt name (0/1 arg)
    if(tokenCount == 1) {
        printf("%s\n", promptName);
    } else if (tokenCount >= 2) {
        if(strlen(tokens[1]) <= 8) {
            strcpy(promptName, tokens[1]);
        } else {
            return 1;
        }
    }
    return 0;
}

int mystatus() {
    // izpise se izhodni status zadnjega izvedenega ukaza
    // >status (0 arg)
    printf("%d\n", lastCmdExitStatus);
    return lastCmdExitStatus; // izjemoma se ta ne steje za vracanje statusa
}

int myexit(){
    // >exit status (0/1 arg)
    if(tokenCount == 1) {
        exit(lastCmdExitStatus);
    } else if (tokenCount >= 2) {
        int s = atoi(tokens[1]);
        exit(s);
    }
}

int myhelp() {
    printf("Available commands:\n");
    printf("help - Show this help\n");
    printf("debug [level] - Set the debug level (0 or 1)\n");
    printf("prompt [name] - Change the prompt\n");
    printf("status - Show the exit status of the last command\n");
    printf("exit [status] - Exit the shell with an optional exit status\n");
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
        
        int succ = tokenize();
        
        stUkaza = find_builtin();
        printDebug();// se izvede le ce je debug_level > 0
        
        if(stUkaza != -1) {
            lastCmdExitStatus = execute_builtin(stUkaza);

        } else {
            // izpis se premakne v printdebug
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

    return 0;
}