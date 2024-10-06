#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

using namespace std;

FILE *inputfile;
bool newProcess = true;
const int BUFFER_SIZE = 4096;
char buffer[BUFFER_SIZE];
vector<int> input;

int main(int argc, char *argv[]){
  inputfile = fopen("lab2_assign/input3","r");
  //inputfile = fopen(argv[1],"r");
  while (1){   
    while (newProcess){ 
        fgets(buffer, BUFFER_SIZE, inputfile);
        if (feof(inputfile)) {
            return 0; // end of file
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        char *tok = strtok(buffer, " \t");
        input.push_back(atoi(tok));  // assume all integers as input
        newProcess = false;
    }
    


    char *tok = strtok(nullptr, " \t");  
    if (tok == NULL){
        newProcess=true;
    } 
    else{
        input.push_back(atoi(tok)); 
    }
    // input contains all the information for the process, for now we care about arrival time (AT) which is first element
    int AT = input[0];
    
        
  }

  return 0;
}