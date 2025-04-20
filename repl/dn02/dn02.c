#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#define MAX_TOKENS 30
int tokens[MAX_TOKENS][2]; // le kazalci na tokene
int tokenCount = 0;

int main() {
    // tokenize

    char vhod[250];
    int end = 0;
    while(!end) {
        if(isatty(STDIN_FILENO)){
            printf("trikish> ");
        }
        if(fgets(vhod, sizeof(vhod), stdin) == NULL) {
            end = 1;
            continue;
        }

        // izpis vhodne vrstice
        printf("Input line: '");
        int i = 0;
        while(1) {
            if (vhod[i] == '\n') {
                break; // to je treba ignorirat, sej pride sele na konc
            }
            printf("%c", vhod[i]);
            i++;
        }
        printf("'\n");

        // tokene bom hranil kot indeks zacetka in konca (vkljucno)
        i = 0;
        int prev_space = 1;
        int end1 = 0;
        while(!end1) {
        
            if(vhod[i] == '"'){
                i++;
                if(vhod[i] == '"') {
                    i++;
                    continue; // "" ignoriraj
                }

                tokens[tokenCount][0] = i;
                int br = 0;
                while (!br) {
                    if(vhod[i] == '"') {
                        tokens[tokenCount][1] = i-1;
                        tokenCount++;
                        br = 1;
                    }
                    i++;
                }
            } else if( (vhod[i] == ' ' || vhod[i] == '\n')&& !prev_space) {
                prev_space = 1;
                tokens[tokenCount][1] = i-1;
                tokenCount++;
            
            } else if(vhod[i] == '#' && prev_space) {
                end1 = 1;

            
            } else if(vhod[i] != ' ' && prev_space) {
                prev_space = 0;
                tokens[tokenCount][0] = i;

            }
            i++;

            if(vhod[i] == '\0') {
                end1 = 1;
            }
        }

        // print tokens
        if (tokenCount) {
            for(int j = 0; j < tokenCount; j++) {
                int a = tokens[j][0];
                int b = tokens[j][1];
                // printf("%d %d", a, b);
                printf("Token %d: '", j);
                for(int k = a; k <= b; k++) {
                    printf("%c", vhod[k]);
                }
                printf("'\n");
            }
        }
        //print redirections
        int inRedirect = 0;
        int outRedirect = 0;
        int bgRun = 0;

        int i_last3 = 0;
        if(tokenCount > 3) {
            i_last3 = tokenCount - 3;
        }
        
        for(int iii = i_last3; iii < tokenCount; iii++){
            // printf("%d\n", iii);
            int a = tokens[iii][0];
            int b = tokens[iii][1];
            // printf("%c\n", vhod[a]);

            if (vhod[a] == '<') { 
                inRedirect = 1; 
                printf("Input redirect: '");
                for(int k = a+1; k <= b; k++) {
                    printf("%c", vhod[k]);
                }
                printf("'\n");
            }
            else if (vhod[a] == '>') { 
                outRedirect = 1;
                printf("Output redirect: '");
                for(int k = a+1; k <= b; k++) {
                    printf("%c", vhod[k]);
                }
                printf("'\n"); 
            }
            else if (vhod[a] == '&') { 
                bgRun = 1; 
                printf("Background: 1");
            }
        }
        
        //reset tokens
        for(int j = 0; j < tokenCount; j++) {
            tokens[j][0] = 0;
            tokens[j][1] = 0;
        }
        tokenCount = 0;
    }
    ///////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////

    

    return 0;
}
