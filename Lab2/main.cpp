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

class Process{
private:
  int TC;
  int CB;
  int IO;
public:
  // constructor
  Process(int TC, int CB, int IO){
    this->TC = TC;
    this->CB = CB;
    this->IO = IO;
  }
};

class Event{
private:
  int timestamp; 
  string oldstate;
  string newstate;
  int pid;
  Process* process;
public:
  // constructor
  Event(int timestamp, int pid, Process* process){
    this->timestamp = timestamp;
    this->pid = pid;
    this->process = process;
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
  void set_eventQ(vector<Event> newEventQ){eventQ = newEventQ;}

};



vector<Event> order_eventQ(vector<Event> eventQ){
  // we do insert sort 
  int n = eventQ.size();
  for (int i = 1; i < n; i++) {
      Event newEvent = eventQ[i]; 
      int key = newEvent.get_timestamp();
      int j = i - 1;

      while (j >= 0 && eventQ[j].get_timestamp() > key) {
          eventQ[j + 1] = eventQ[j]; 
          j--;
      }
      eventQ[j + 1] = newEvent; 
  }
  for (Event event : eventQ) {
    int timestamp = event.get_timestamp();
    int pid = event.get_pid();
    cout << "time: " << timestamp << " pid: " << pid << endl;
  }
  return eventQ;
}

void simulation(DesLayer deslayer){
  Event* event;
  while ((event = deslayer.get_event())){
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    int transition = event->get_transition();
    int timeInPrevState =  CURRENT_TIME - proc->state_ts; 
    delete event; event = nullptr;

    switch(transition){
      case TRANS_TO_READY:
        // must come from BLOCKED or CREATED
        // add to run queue, no event created
        CALL_SCHEDULER = true;
        break;
      case TRANS_TO_PREEMPT: // similar to TRANS_TO_READY
        // must come from RUNNING (preemption)
        // add to runqueue (no event is generated)
        CALL_SCHEDULER = true;
        break;
      case TRANS_TO_RUN:
        // create event for either preemption or blocking
        break;
      case TRANS_TO_BLOCK:
        //create an event for when process becomes READY again
        CALL_SCHEDULER = true;
        break;
    }

    if (CALL_SCHEDULER) {
      if (get_next_event_time() == CURRENT_TIME)
        continue; //process next event from Event queue
      CALL_SCHEDULER = false;
      if (CURRENT_RUNNING_PROCESS == nullptr){
        CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
        if (CURRENT_RUNNING_PROCESS == nullptr){
          continue;
        }
        // create event to make this process runnable for same time.`
      }
    }

  }
};

int main(int argc, char *argv[]){
  //inputfile = fopen("input-1-eventQ","r");
  int pid = 0;
  inputfile = fopen(argv[1],"r");
  DesLayer deslayer; 
  vector<Event> eventQ;
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
    Process* process = new Process(TC, CB, IO);  
    Event event = Event(AT, pid, process);
    eventQ.push_back(event);
    newProcess=true; 
    pid++;    
  }
  vector<Event> orderedEventQ = order_eventQ(eventQ); // we get the ordered list of events 
  deslayer.set_eventQ(orderedEventQ);
  simulation(deslayer);
  return 0;
}