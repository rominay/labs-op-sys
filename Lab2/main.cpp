#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>

using namespace std;

FILE *inputfile;
bool newProcess = true;
const int BUFFER_SIZE = 4096;
char buffer[BUFFER_SIZE];
int AT;

typedef enum {STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_PREEMPT, STATE_DONE} process_state_t;

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
  process_state_t transition; 
public:
  // constructor
  Event(int timestamp, int pid, Process* process, process_state_t transition){
    this->timestamp = timestamp;
    this->pid = pid;
    this->process = process;
    this->transition = transition;
  }
  // getters
  int get_timestamp() const {return timestamp;}
  int get_pid() const {return pid;}
  string get_oldstate() const { return oldstate; }
  string get_newstate() const { return newstate; } 
  Process* get_process() const {return process;} 
  process_state_t get_transition() const {return transition;}
  // setters 
  void set_timestamp(int newTimestamp){timestamp=newTimestamp;}
  void set_oldstate(string newOldstate){oldstate=newOldstate;}
  void set_newstate(string newNewstate){newstate=newNewstate;}
  void set_transition(process_state_t newTransition){transition=newTransition;}
};

class DesLayer{
private:
  list<Event> eventQ;
public:
  // getters
  Event get_event(){
    if (eventQ.empty()) {
      Process* dummyProc = new Process(0,0,0);
      process_state_t transition = STATE_READY;
      Event dummyEvent = Event(-1, 0, dummyProc, transition);
      return dummyEvent; //TO DO return an empty event with timestamp -1 
    }
    else {
      Event event = eventQ.front();
      eventQ.erase(eventQ.begin());
      return event;
    }
  }
  //vector<Event> get_eventQ() const {return eventQ;}
  // setters
  void put_event(Event newEvent){
    if (eventQ.empty()) {eventQ.push_back(newEvent);} // we take the first element as sorted 
    else{ // we know the current eventQ is ordered from smaller to largest according to policy
      int eventQsize = eventQ.size();
      int key = newEvent.get_timestamp();
      auto it = eventQ.begin();
      for (const Event& event : eventQ){
        if (event.get_timestamp()>key){
          eventQ.insert(it, newEvent);
          break;
        }
        advance(it, 1);
      }
      if (eventQ.size()==eventQsize){ // means we did not add it
        eventQ.push_back(newEvent);
      }
    }
  }

  //void set_eventQ(list<Event> newEventQ){eventQ = newEventQ;}
  
  int get_next_event_time(){
    if (eventQ.empty()){return -1;}
    else{return eventQ.front().get_timestamp();}
  }

  void print_eventQ(){
    for (Event event : eventQ) {
    int timestamp = event.get_timestamp();
    int pid = event.get_pid();
    cout << "time: " << timestamp << " pid: " << pid << endl;
  }
  }
};


class Scheduler{
public:
  Scheduler();
  void get_next_process();
};

void simulation(DesLayer deslayer, Scheduler* scheduler){
  Event event = deslayer.get_event();
  int CURRENT_TIME;
  bool CALL_SCHEDULER;
  while (event.get_timestamp() > -1){
    Process* proc = event.get_process();
    CURRENT_TIME = event.get_timestamp();
    int transition = event.get_transition();
    //int timeInPrevState =  CURRENT_TIME - proc->state_ts; 
    //delete event; event = nullptr;
    Process* current_running_process;

    switch(transition){
      case STATE_READY: // TRANS_TO_READY
        // must come from BLOCKED or CREATED
        // add to run queue, no event created
        CALL_SCHEDULER = true;
        break;
      case STATE_PREEMPT: // similar to TRANS_TO_READY
        // must come from RUNNING (preemption)
        // add to runqueue (no event is generated)
        CALL_SCHEDULER = true;
        break;
      case STATE_RUNNING:
        // create event for either preemption or blocking
        current_running_process = proc;
        break;
      case STATE_BLOCKED:
        //create an event for when process becomes READY again
        CALL_SCHEDULER = true;
        break;
    }

    if (CALL_SCHEDULER) {
      if (deslayer.get_next_event_time() == CURRENT_TIME)
        continue; //process next event from Event queue
      CALL_SCHEDULER = false;
      if (current_running_process == nullptr){
        //current_running_process = scheduler->get_next_process();
        if (current_running_process == nullptr){
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
  //vector<Event> eventQ;
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
    process_state_t transition = STATE_READY;
    Event event = Event(AT, pid, process, transition);
    deslayer.put_event(event);
    newProcess=true; 
    pid++;    
  }
  deslayer.print_eventQ();
  //vector<Event> orderedEventQ = order_eventQ(eventQ); // we get the ordered list of events 
  //deslayer.set_eventQ(orderedEventQ);
  //Scheduler* scheduler; // TO DO
  //simulation(deslayer, scheduler);
  return 0;
}