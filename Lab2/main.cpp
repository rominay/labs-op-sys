#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>
#include <queue>

using namespace std;

FILE *inputfile;
FILE *randfile;
bool newProcess = true;
const int BUFFER_SIZE = 4096;
char buffer[BUFFER_SIZE];
int ofs = 0;
vector<int> randvals;
int quantum;
int totalCPU = 0;
int totalTT = 0;
int totalCW = 0;
int totalIO = 0;


typedef enum {STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_PREEMPT, STATE_DONE} process_state_t;

class Process{
private:
  int AT; // arrival time
  int TC; // total CPU time
  int CB; // CPU Burst 
  int IO; // IO Burst
  int FT; //finishing time
  int TT; // turnaround 
  int IT; // I/O time 
  int PRIO; // static priority
  int CW; // CPU waiting time
public:
  // constructor
  Process(int AT, int TC, int CB, int IO){
    this->AT = AT;
    this->TC = TC;
    this->CB = CB;
    this->IO = IO;
    FT = 0;
		TT = 0;
		IT = 0;
    PRIO = 0; // TO DO
		CW = 0;
  }
  int get_AT() const {return AT;}
  int get_TC() const {return TC;}
  int get_CB() const {return CB;}
  int get_IO() const {return IO;}
  int get_FT() const { return FT; }
  int get_TT() const { return TT; }
  int get_IT() const { return IT; }
  int get_PRIO() const { return PRIO; }
  int get_CW() const { return CW; }
  // add setters
};

list <Process*> processes;

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
    virtual ~BaseScheduler() {} // virtual destructor 
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual string get_type() = 0;
    //virtual bool test_preempt(Process* activated_process);
};
BaseScheduler *scheduler;
// FCFS Scheduler 
class FSFSScheduler : public BaseScheduler {
private:
    queue<Process*> runQueue; 

public:
    string get_type() override {return "FCFS";}
    Process* get_next_process() override {
        if (runQueue.empty()) {
            cout << "No processes in the queue." << endl;
            return NULL; 
        }
        Process* nextProcess = runQueue.front();  
        runQueue.pop();                          
        return nextProcess;
    }

    void add_process(Process* p) override {
        runQueue.push(p); 
    }
};

int myrandom(int burst) {
	if (ofs == randvals.size()){ofs = 0;}
  else{ofs++;}
	return 1 + (randvals[ofs] % burst);
}

DesLayer deslayer;
Process* current_running_process;

void simulation(){
  Event* event;
  int CURRENT_TIME;
  bool CALL_SCHEDULER;
  while ((event= deslayer.get_event())){ // we call the deslayer to give us an event 
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    process_state_t transition = event->get_transition();
    //int timeInPrevState =  CURRENT_TIME - proc->state_ts; TO DO
    delete event; event = nullptr;

    switch(transition){
      case STATE_READY: // TRANS_TO_READY
        // must come from BLOCKED or CREATED
        // add to run queue, no event created
        scheduler->add_process(proc);
        CALL_SCHEDULER = true;
        break;
      case STATE_PREEMPT: // similar to TRANS_TO_READY
        // must come from RUNNING (preemption)
        // add to runqueue (no event is generated)
        CALL_SCHEDULER = true;
        break;
      case STATE_RUNNING:
        {
        // create event for either preemption or blocking
        //current_running_process = proc;
        //if((proc->get_CPU_time() + burst) >= proc->get_TC()){
					//proc->set_CPU_time(proc->get_CPU_time()+burst);
					//proc->set_CB_reamain_time(proc->get_CB_reamain_time()-1);
        process_state_t transition = STATE_DONE;
        int burst= myrandom(proc->get_CB());
        //proc->set_CP
				deslayer.put_event(CURRENT_TIME+burst, proc, transition);
				//}
        }
        break;
      case STATE_BLOCKED:
        //create an event for when process becomes READY again
        CALL_SCHEDULER = true;
        break;
      case STATE_DONE:
        current_running_process = NULL;
    }

    if (CALL_SCHEDULER) {
      if (deslayer.get_next_event_time() == CURRENT_TIME){
        event = deslayer.get_event();
        continue; 
      }
      CALL_SCHEDULER = false;
      if (current_running_process == nullptr){
        current_running_process = scheduler->get_next_process();
        if (current_running_process == nullptr){
          event = deslayer.get_event();
          continue;
        }
        // create event to make this process runnable for same time
        process_state_t transition = STATE_RUNNING;
				deslayer.put_event(CURRENT_TIME, current_running_process, transition);
				current_running_process = NULL;
      }
    }

  }
};


 

int main(int argc, char *argv[]){
  /*
  * Open file with random numbers
  */
  ifstream randfile("lab2_assign/rfile");
	string rs;
	while(randfile>>rs){
		randvals.push_back(atoi(rs.c_str()));
	}
  /*
  * Open input file
  */
  inputfile = fopen("input0","r");
  //inputfile = fopen(argv[1],"r");

  int AT;
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
    Process* process = new Process(AT, TC, CB, IO); 
    //process -> static_prio = myrandom( maxprio );
    processes.push_back(process);
    process_state_t transition = STATE_READY;
    deslayer.put_event(AT, process, transition);
    newProcess=true;   
  }
  scheduler = new FSFSScheduler();
  //quantum = 100000;
  simulation();
  /*
  * Output
  */
  cout<<scheduler->get_type()<<endl;

  int index=0;
	for(auto proc : processes){
		totalCPU += proc->get_TC();
		totalTT += proc->get_TT();
		totalCW += proc->get_CW();
		printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
			index,proc->get_AT(),proc->get_TC(),proc->get_CB(),proc->get_IO(),
			proc->get_PRIO(),proc->get_FT(), proc->get_TT(),proc->get_IT(),proc->get_CW());
    index++;
	}
  return 0;
}