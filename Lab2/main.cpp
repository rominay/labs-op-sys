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
int AT;


class Event{
private:
  int timestamp; 
  string oldstate;
  string newstate;
  int pid;
  //Process process;
public:
  // constructor
  Event(int timestamp, int pid){
    this->timestamp = timestamp;
    this->pid = pid;
  }
  // getters
  int get_timestamp() const {return timestamp;}
  int get_pid() const {return pid;}
  string get_oldstate() const { return oldstate; }
  string get_newstate() const { return newstate; }  
  // setters 
  void set_timestamp(int newTimestamp){timestamp=newTimestamp;}
  void set_oldstate(string newOldstate){oldstate=newOldstate;}
  void set_newstate(string newNewstate){newstate=newNewstate;}
};

class DesLayer{
private:
  vector<Event> eventQ;
public:
  // getters
  Event get_event(){
    if (eventQ.empty()) {} //TODO
    else {
      Event event = eventQ[0];
      eventQ.erase(eventQ.begin());
      return event;
    }
  }
  vector<Event> get_eventQ() const {return eventQ;}
  // setters
  void put_event(Event newEvent){
    if (eventQ.empty()) {eventQ.push_back(newEvent);} // we take the first element as sorted 
    else{
    }
  }

};

vector<Event> eventQ;

int main(int argc, char *argv[]){
  inputfile = fopen("input-1-eventQ","r");
  int pid = 0;
  //inputfile = fopen(argv[1],"r");
  //DesLayer deslayer; 
  while (1){   
    while (newProcess){ 
        fgets(buffer, BUFFER_SIZE, inputfile);
        if (feof(inputfile)) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        char *tok = strtok(buffer, " \t");
        AT = atoi(tok);
        newProcess = false;
    }
    if (feof(inputfile)) {
        break;
    }
    char *tok = strtok(nullptr, " \t");  
    int TC = atoi(tok);

    tok = strtok(nullptr, " \t"); 
    int CB = atoi(tok);

    tok = strtok(nullptr, " \t"); 
    int IO = atoi(tok);
    Event event = Event(AT, pid);
    eventQ.push_back(event);
    newProcess=true; 
    pid++;  
        
  }

  // we do insert sort 
  int n = eventQ.size();
  // Traverse through the array
  for (int i = 1; i < n; i++) {
      Event newEvent = eventQ[i]; // Current element to be inserted
      int key = newEvent.get_timestamp();
      int j = i - 1;

      // Shift elements of arr[0..i-1], that are greater than key,
      // to one position ahead of their current position
      while (j >= 0 && eventQ[j].get_timestamp() > key) {
          eventQ[j + 1] = eventQ[j]; // Move the element one position to the right
          j--;
      }
      eventQ[j + 1] = newEvent; // Place the key in its correct position
  }
  for (Event event : eventQ) {
    int timestamp = event.get_timestamp();
    int pid = event.get_pid();
    cout << "time: " << timestamp << " pid: " << pid << endl;
  }
  return 0;
}