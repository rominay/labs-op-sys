#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>
#include <queue>

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
  int get_TC() const {return TC;}
};

class Event{
private:
  int timestamp; 
  string oldstate;
  string newstate;
  //int pid;
  Process* process;
  //int process;
  process_state_t transition; 
public:
  // constructor
  Event(int timestamp, Process* process, process_state_t transition){
    this->timestamp = timestamp;
    this->process = process;
    this->transition = transition;
  }
  // getters
  int get_timestamp() const {return timestamp;}
  //int get_pid() const {return pid;}
  string get_oldstate() const { return oldstate; }
  string get_newstate() const { return newstate; } 
  Process* get_process() const {return process;}
  //int get_process() const {return process;} 
  process_state_t get_transition() const {return transition;}
  // setters 
  void set_timestamp(int newTimestamp){timestamp=newTimestamp;}
  void set_oldstate(string newOldstate){oldstate=newOldstate;}
  void set_newstate(string newNewstate){newstate=newNewstate;}
  void set_transition(process_state_t newTransition){transition=newTransition;}
};

class DesLayer{
private:
  list<Event*> eventQ;
public:
  // getters
  Event* get_event(){
    if (eventQ.empty()) {return NULL;}
    else {
      Event* ev = eventQ.front(); 
      eventQ.pop_front();  // Remove the first event from the queue 
      return ev;
    }
  }
  // setters
  void put_event(int AT, Process* process, process_state_t transition){
    Event* newEvent = new Event(AT, process, transition);
    if (eventQ.empty()) {eventQ.push_back(newEvent);} // we take the first element as sorted 
    else{ // insert in the correct position
      auto it = eventQ.begin();
      while (it != eventQ.end() && (*it)->get_timestamp() <= newEvent->get_timestamp()) {
          ++it;  // Find the correct position
      }
      eventQ.insert(it, newEvent); 
    }
  }

  
  int get_next_event_time(){
    if (eventQ.empty()){return -1;}
    else{return eventQ.front()->get_timestamp();}
  }

  
};


// Base class for scheduling
class BaseScheduler {
public:
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual ~BaseScheduler() {} // virtual destructor 
};

// FIFO Scheduler 
class FIFOScheduler : public BaseScheduler {
private:
    queue<Process*> processQueue; 

public:
    Process* get_next_process() override {
        if (processQueue.empty()) {
            cout << "No processes in the queue." << endl;
            return nullptr; 
        }
        Process* nextProcess = processQueue.front();  
        processQueue.pop();                          
        return nextProcess;
    }

    void add_process(Process* p) override {
        processQueue.push(p); 
    }
};

DesLayer deslayer;

void simulation(){
  Event* event = deslayer.get_event();
  int CURRENT_TIME;
  bool CALL_SCHEDULER;
  while (event){
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    int transition = event->get_transition();
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
        //current_running_process = proc;
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
  inputfile = fopen("input-1-eventQ","r");
  //int pid = 0;
  //inputfile = fopen(argv[1],"r");
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
    deslayer.put_event(AT, process, transition);
    newProcess=true; 
    //pid++;    
  }
  // comment out below if you want to print the eventQ
  Event* event = deslayer.get_event();
  // Assuming get_event() is a function that gets an event from the queue or source.
  while (event) {
    int timestamp = event->get_timestamp();
    //int pid = event.get_process();
    cout << "time: " << timestamp << endl; //" pid: " << pid << endl;
    event = deslayer.get_event();
  }
  // another thing to try could be 
  int CB=0;
  int IO=0;
  process_state_t transition = STATE_READY;
  Process* process = new Process(0, CB, IO);
  deslayer.put_event(100, process ,transition);
  process = new Process(1, CB, IO);
  deslayer.put_event(100, process ,transition);
  process = new Process(2, CB, IO);
  deslayer.put_event(10, process ,transition);
  process = new Process(3, CB, IO);
  deslayer.put_event(1, process ,transition);
  process = new Process(4, CB, IO);
  deslayer.put_event(20, process ,transition);
  process = new Process(5, CB, IO);
  deslayer.put_event(20, process ,transition);

  event = deslayer.get_event();
  while (event) {
    int timestamp = event->get_timestamp();
    //int pid = event.get_process();
    cout << "time: " << timestamp << " id: " << event->get_process()->get_TC() << endl; //" pid: " << pid << endl;
    event = deslayer.get_event();
  }
  // another thing to try could be 
  cout << "----" << endl;
  process = new Process(1, CB, IO);
  deslayer.put_event(100, process ,transition);
  event = deslayer.get_event();
  while (event) {
    cout << "time: " << event->get_timestamp() << " id: " << event->get_process()->get_TC() << endl; //" pid: " << pid << endl;
    event = deslayer.get_event();
  }
  process = new Process(2, CB, IO);
  deslayer.put_event(5, process ,transition);
  process = new Process(3, CB, IO);
  deslayer.put_event(7, process ,transition);
  event = deslayer.get_event();
  while (event) {

    cout << "time: " << event->get_timestamp() << " id: " << event->get_process()->get_TC() << endl; //" pid: " << pid << endl;
    event = deslayer.get_event();
  }


  //FIFOScheduler *scheduler = new FIFOScheduler();
  //simulation();
  return 0;
}