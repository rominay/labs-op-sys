#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>
#include <queue>
#include <unistd.h> // For getopt
#include <regex>
#include <vector>

using namespace std;

bool verbose=false;
int ofs = 0;
vector<int> randvals;
/* input args */
int quantum;
int maxprio;
/* global variables for final calculation */
int totalCPU = 0;
int totalTT = 0;
int totalCW = 0;
int totalIO = 0;
int last_event_FT=0;
int totalIOTime = 0; 

typedef enum {STATE_READY, STATE_RUNNING, STATE_BLOCKED} process_state_t; 

const char* stateToString(process_state_t state) {
    switch (state) {
        case STATE_READY:    return "READY";
        case STATE_RUNNING:  return "RUNNG";
        case STATE_BLOCKED:  return "BLOCK";
        default:             return "UNKNOWN";  //invalid state
    }
};

int myrandom(int burst) {
	if (ofs == static_cast<int>(randvals.size())){ofs = 0;}
  else{ofs++;}
	return 1 + (randvals[ofs] % burst);
};

class Process{
public:
  int AT; // arrival time
  int TC; // total CPU time
  int CB; // CPU Burst 
  int IO; // IO Burst

  int FT; //finishing time
  int TT; // turnaround 
  int IT; // I/O time 

  int static_priority; // static priority
  int dynamic_priority;
  int CW; // CPU waiting time
  int CPU_time; 
  int remaining_CPU_burst;
  int state_ts; // this is the time at which it was put to ready 
  int time_prev_state; // the amount of time in previous state
  int pid; // identifier
  process_state_t old_state;
  // constructor
  Process(int AT, int TC, int CB, int IO, int pid){
    this->AT = AT;
    this->TC = TC;
    this->CB = CB;
    this->IO = IO;
    this->pid=pid;
    FT = 0;
		TT = 0;
		IT = 0;
    static_priority = myrandom(maxprio);
    dynamic_priority = static_priority-1;
		CW = 0;
    CPU_time=0;
    remaining_CPU_burst=0;
    state_ts=-1; // we initialize with -1 to indicate that it has not experienced any transition
    time_prev_state=-1; // we initialize with -1 to indicate that it has not experienced any transition
  }
  int get_AT() const {return AT;}
  int get_TC() const {return TC;}
  int get_CB() const {return CB;}
  int get_IO() const {return IO;}
  int get_FT() const { return FT; }
  int get_TT() const { return FT - AT; }
  int get_IT() const { return IT; }
  int get_static_priority() const { return static_priority; }
  int get_CW() const { return CW; }
  int get_state_ts() const {return state_ts;}
  int get_time_prev_state() const {return time_prev_state;}
  int get_remaining_time() const {return TC - CPU_time;}
  // add setters
  void set_FT(int new_FT) { FT=new_FT; }
  void set_time_prev_state(int new_time_prev_state) {time_prev_state=new_time_prev_state;}
};


list <Process*> processes;

class Event{
private:
  int timestamp; 
  string oldstate;
  string newstate;
  Process* process;
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

list<Event*> a_list;

class DesLayer{
private:
public:
  list<Event*> eventQ;
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

DesLayer deslayer;
Process* current_running_process;


// Base class for scheduler
class BaseScheduler {
public:
    virtual ~BaseScheduler() {} // virtual destructor 
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual string get_type() = 0;
    void virtual handle_preemption(Process* readyProcess, 
                                   Process* runningProcess, int currentTime){};
};

BaseScheduler *scheduler;

// First Come First Serve Scheduler 
class FCFSScheduler : public BaseScheduler {
private:
    queue<Process*> runQueue; 

public:
    string get_type() override {return "FCFS";}
    Process* get_next_process() override {
        if (runQueue.empty()) {
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


class LCFSScheduler : public BaseScheduler{
	deque<Process *> runQueue;

public:
  string get_type() override {return "LCFS";}
	Process *get_next_process() override {
		if (runQueue.empty()){
			return NULL;
		}
		Process* nextProcess=runQueue.back();
		runQueue.pop_back();
		return nextProcess;
	}
  void add_process(Process *p) override {
		runQueue.push_back(p);
	}
};

class SRTFScheduler : public BaseScheduler{
  list <Process *> runQueue;
public:
    string get_type() override { 
        return "SRTF"; 
    }

    Process* get_next_process() override {
        if (runQueue.empty()) {
            return NULL;
        }

        // get process with shortest remaining time
        auto shortestProcess = runQueue.begin();
        for (auto it = runQueue.begin(); it != runQueue.end(); ++it) {
            if ((*it)->get_remaining_time() < (*shortestProcess)->get_remaining_time()) {
                shortestProcess = it;
            }
        }
        Process* nextProcess = *shortestProcess;
        // remove it from the list
        runQueue.erase(shortestProcess);
        return nextProcess;
    }

    void add_process(Process* p) override {
        runQueue.push_back(p);
    }
};
class RoundRobinScheduler : public BaseScheduler{
	queue<Process *> runQueue;
public:
  string get_type() override {return "RR";}
	Process *get_next_process() override {
		if (runQueue.empty()){
			return NULL;
		}
		Process* nextProcess=runQueue.front();
		runQueue.pop();
		return nextProcess;
	}
  void add_process(Process *p) override {
		runQueue.push(p);
	}
};

// util function
bool isEmpty(queue<Process*> a_queue[]){
  for (int i=0; i<maxprio; i++){
    if (a_queue[i].size() != 0) {
      return false;
    }
  }
  return true;
};

class PRIOScheduler : public BaseScheduler{
public:
	queue<Process*>* activeQ; 
  queue<Process*>* expiredQ;
  PRIOScheduler(int maxprio){
    activeQ = new queue<Process*>[maxprio];
    expiredQ = new queue<Process*>[maxprio];

  }
  ~PRIOScheduler() {
    delete activeQ;
    delete expiredQ;
  }
  
	Process* get_next_process() override {
      for (size_t i=0; i<2;i++){
        Process* nextProcess;
        for (int i = maxprio - 1; i >= 0; --i) { // Prioritize higher dynamic priorities
          if (!activeQ[i].empty()) {
              nextProcess = activeQ[i].front();
              activeQ[i].pop();
              return nextProcess;
          }
        }

        if (isEmpty(activeQ)) {
            // Swap active and expired queues if active is empty
            queue<Process*>* tempQ = activeQ;
            activeQ = expiredQ;
            expiredQ = tempQ;
        }
      }
      return nullptr;
    }

  void add_process(Process *p) override {
    if (p->dynamic_priority == -1){
      p->dynamic_priority = p->static_priority-1; // it is reset
      expiredQ[p->dynamic_priority].push(p);
      return;
      
    } 
    activeQ[p->dynamic_priority].push(p);
	}
  string get_type() override {return "PRIO";}
};

class EPRIOScheduler : public PRIOScheduler {
public:
    queue<Process*>* activeQ;
    queue<Process*>* expiredQ;

    EPRIOScheduler(int maxprio) : PRIOScheduler(maxprio) {}

    string get_type() override {
        return "PREPRIO";
    }

    void handle_preemption(Process* readyProcess, Process* runningProcess, int currentTime) {
        if (runningProcess != nullptr) { 
          bool condition_1 = readyProcess->dynamic_priority > runningProcess->dynamic_priority;
          bool condition_2 = !has_pending_event(runningProcess, currentTime);
          bool make_preemption = condition_1 && condition_2;
          if (verbose){
            string preemp; 
            if (make_preemption == true) {preemp="YES";}
            else {preemp="NO";}
            cout << "--> PrioPreempt Cond1=" << condition_1 << "Cond2=" << condition_2 << "-->" << preemp  << endl;
          }
          if (make_preemption) {
             
            remove_future_events(runningProcess);

            // make preemption
            process_state_t transition = STATE_READY; 
            deslayer.put_event(currentTime, runningProcess, transition);
          }
        }
    }

private:
    bool has_pending_event(Process* proc, int currentTime) {
        for (auto ev : deslayer.eventQ) {
            if (ev->get_process() == proc && ev->get_timestamp() == currentTime) {
                return true;
            }
        }
        return false;
    }

    void remove_future_events(Process* proc) {
        auto it = deslayer.eventQ.begin();
        while (it != deslayer.eventQ.end()) {
            if ((*it)->get_process() == proc) {
                it = deslayer.eventQ.erase(it); 
            } else {
                ++it;
            }
        }
    }
};

void simulation(){
  Event* event;
  int CURRENT_TIME = 0;
  bool CALL_SCHEDULER=false;
  int activeIOCount = 0;  
  int lastIOTransitionTime = 0;

  while ((event= deslayer.get_event())){ // we call the deslayer to give us an event 
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    process_state_t transition = event->get_transition();
    int timeInPrevState;
    if (proc->state_ts==-1) {timeInPrevState=0;}
    else{
      timeInPrevState =  CURRENT_TIME - proc->state_ts;
    }
    proc->state_ts=CURRENT_TIME;
    proc->set_time_prev_state(timeInPrevState);
    delete event; event = nullptr;

    switch(transition){
      case STATE_READY: 
        {
        int current_priority = proc->dynamic_priority;
        if (proc->old_state==STATE_BLOCKED){ // it not the first time put on ready
          // post-accounting:
          proc->IT += timeInPrevState;
          activeIOCount--; // it stopped being doing IO
          if (activeIOCount == 0) {  // transition 1 -> 0 
            totalIOTime += CURRENT_TIME - lastIOTransitionTime;
          }
          if (scheduler->get_type() == "PRIO" || scheduler->get_type() == "PREPRIO"){
            proc->dynamic_priority = proc->static_priority-1; // after coming from IO it is reset like this
          }
        }

        if (proc->old_state==STATE_RUNNING) {
          // post-accounting:
          proc->remaining_CPU_burst-=timeInPrevState;
          proc->CPU_time += timeInPrevState;
          current_running_process=nullptr;
          if (scheduler->get_type() == "PRIO" || scheduler->get_type() == "PREPRIO"){
            proc->dynamic_priority-=1; // we came from preemption
          }
        }
        

        if (verbose) {
          if (proc->old_state==STATE_RUNNING){ 
            cout << CURRENT_TIME<<" "<< proc->pid <<" "<< timeInPrevState<< ": " << "RUNNG"<<" -> "<< "READY";
            cout <<" cb="<< proc->remaining_CPU_burst <<" rem="<<proc->TC-proc->CPU_time<< " prio="<< current_priority << endl;
          }
          else {
            cout << CURRENT_TIME<<" "<< proc->pid <<" "<< timeInPrevState<< ": " << stateToString(proc->old_state)<<" -> "<< "READY" << endl;
          }
        }
        CALL_SCHEDULER = true;
        proc->old_state=STATE_READY;
        scheduler->add_process(proc);
        scheduler-> handle_preemption(proc, current_running_process, CURRENT_TIME);
        break;
        }
      case STATE_RUNNING:
        {
        current_running_process = proc;
        proc->CW+=proc->time_prev_state; // we know we come from READY
        int time_to_run;
        int time_remaining_to_run = proc->remaining_CPU_burst;
        if (time_remaining_to_run > 0){ // it means we did not exhaust previous CPU burst
          time_to_run = time_remaining_to_run;

        }
        else{ // we get a new CPU bust 
          int CPU_burst= myrandom(proc->get_CB());
          int remaining_CPU_time = proc->get_TC() - proc->CPU_time;
          if (remaining_CPU_time < CPU_burst){//if the remaining time is less than the CPU_burst , we run for the remaining time
            time_to_run = remaining_CPU_time;
          }
          else {// we run for the CPU_burst
            time_to_run = CPU_burst;
          }
          proc->remaining_CPU_burst = time_to_run;
        }

        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< stateToString(proc->old_state) <<" -> "<<"RUNNG";
					cout <<" cb="<< time_to_run <<" rem="<<proc->TC-proc->CPU_time<< " prio="<< proc->dynamic_priority <<endl;
				}
        if (time_to_run > quantum){ // we will not finish 
          process_state_t transition = STATE_READY;
          deslayer.put_event(CURRENT_TIME+quantum, proc, transition);
        }
        else{ // it goes to I/O
          process_state_t transition;
          transition = STATE_BLOCKED;
          deslayer.put_event(CURRENT_TIME+time_to_run, proc, transition);
        }
        proc->old_state=STATE_RUNNING;
        break;
        }
        
      case STATE_BLOCKED:
        {
        // post-accounting:
        proc->CPU_time += timeInPrevState;
        proc->remaining_CPU_burst=0;
        // check if we are done
        if (proc->CPU_time >= proc->get_TC()){ 
          proc->set_FT(CURRENT_TIME);
          if (verbose) {
            cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< "DONE" << endl;
          }
          last_event_FT = CURRENT_TIME;
          current_running_process=nullptr;
          CALL_SCHEDULER=true;
          break;
        }

        activeIOCount++; // it is doing IO 
        if (activeIOCount == 1) {  // transition 0 -> 1 
          lastIOTransitionTime = CURRENT_TIME;
        }
        //create an event for when process becomes READY again
        int IO_burst= myrandom(proc->get_IO());
        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< stateToString(proc->old_state) <<" -> "<<"BLOCK";
					cout <<" ib="<< IO_burst <<" rem="<<proc->TC-proc->CPU_time <<endl;
				}
        process_state_t transition = STATE_READY;
        deslayer.put_event(CURRENT_TIME+IO_burst, proc, transition);
        CALL_SCHEDULER = true;
        current_running_process = nullptr;
        proc->old_state=STATE_BLOCKED;
        break;
        }
    }

    if (CALL_SCHEDULER) {
      if (deslayer.get_next_event_time() == CURRENT_TIME){
        continue; 
      }
      CALL_SCHEDULER = false;
      if (current_running_process == nullptr){
        current_running_process = scheduler->get_next_process();
        if (current_running_process == nullptr){
          continue;
        }
        // create event to make this process runnable for same time
        process_state_t transition = STATE_RUNNING;
				deslayer.put_event(CURRENT_TIME, current_running_process, transition);
      }
    }
  }
};

// help message
void display_help() {
    cout << "Usage: <program> [-v] [-s<schedspec>] inputfile randfile\n"
              << "Options:\n"
              << "  -v        Enable verbose output\n"
              << "  -s <schedspec> Specify the scheduling specification (requires argument)\n"
              << "  inputfile The input file to be processed\n"
              << "  randfile  The random file to be used\n";
    exit(0);
}

int main(int argc, char *argv[]){
  /*
  * Pass parameters
  */
  int opt;
  string schedspec; // scheduling specification

  // the arguments
  while ((opt = getopt(argc, argv, "vs:")) != -1) {
      switch (opt) {
          case 'v':  // verbose
              verbose = true;
              break;
          case 's':  // scheduler algorithm
              schedspec = optarg;
              break;
          case '?':  // unknown or missing
              std::cerr << "Unknown or missing argument!" << std::endl;
              display_help(); 
              break;
      }
  }
  /*
  * Open file with random numbers
  */
  string rand_file = argv[optind + 1];
  ifstream randfile(rand_file);
	string rs;
	while(randfile>>rs){
		randvals.push_back(atoi(rs.c_str()));
	}
  /*
  * Read the args
  */
  while(1){
    if (schedspec == "F") {
      scheduler = new FCFSScheduler();
      quantum = 10000;
      maxprio = 4; 
      break;
    }
    if (schedspec == "L") {
      scheduler = new LCFSScheduler();
      quantum = 10000;
      maxprio = 4; 
      break;
    }
    if (schedspec == "S") {
      scheduler = new SRTFScheduler();
      quantum = 10000;
      maxprio = 4; 
      break;
    }

    // Case 2: R<num> (e.g., R100)
    regex r_regex("^R([0-9]+)$");
    smatch r_match;
    if (regex_match(schedspec, r_match, r_regex)) {
        maxprio = 4; 
        string str_quantum = r_match[1];
        quantum = stoi(str_quantum);
        if (quantum<0) { // check if valid quantum
          cout << "no valid value of quantum=" <<quantum;
          return 0;} 
        scheduler = new RoundRobinScheduler();
        break;
    }

    // Case 3: P<num>[:<maxprio>] (e.g., P10:20 or P10)
    regex p_regex("^P([0-9]+)(?::([0-9]+))?$");
    smatch p_match;
    if (regex_match(schedspec, p_match, p_regex)) {
        string str_quantum = p_match[1];
        quantum = stoi(str_quantum); // TO DO: check if quantum is required
        if (p_match[2].length() > 0) {
          string str_maxprios = p_match[2];
          maxprio = stoi(str_maxprios);
        } else{
          maxprio=4;
        }
        //check if valid input
        if (quantum<0) { // check if valid quantum
          cout << "no valid value of quantum=" <<quantum;
          return 0;
        }; 
        if (maxprio<0) { 
          cout << "no valid value of maxprio=" <<maxprio;
          return 0;
        };
        scheduler = new PRIOScheduler(maxprio);
        break;
    }

    // Case 4: E<num>[:<maxprios>] (e.g., E5:10 or E5)
    regex e_regex("^E([0-9]+)(?::([0-9]+))?$");
    smatch e_match;
    if (regex_match(schedspec, e_match, e_regex)) {
        string str_quantum = e_match[1];
        quantum = stoi(str_quantum); 
        if (e_match[2].length() > 0) {
          string str_maxprios = e_match[2];
          maxprio = stoi(str_maxprios);
        } else{
          maxprio=4;
        }
        //check if valid input
        if (quantum<0) { 
          cout << "no valid value of quantum=" <<quantum;
          return 0;
        };
        if (maxprio<0) { 
          cout << "no valid value of maxprio=" <<maxprio;
          return 0;
        }; 
        scheduler = new EPRIOScheduler(maxprio);
        break;
    }

    cout << "Invalid scheduling specification format: " << schedspec << endl;
    return 0;
  } 
  
  /*
  * Open input file
  */
  char* input_file = argv[optind];
  ifstream file(input_file);
  int pid=0;
  string line;
  while (getline(file, line)) {  
    stringstream ss(line);
    int AT, TC, CB, IO; 
    ss >> AT >> TC >> CB >> IO;
    Process* process = new Process(AT, TC, CB, IO, pid); 
    processes.push_back(process);
    deslayer.put_event(AT, process, STATE_READY);
    pid++;  
  }
  /*
  * Perform the simulation
  */ 
  simulation();
  /*
  * Output
  */ 
  string type = scheduler->get_type();
  cout<<type;
  if (type == "RR" || type == "PRIO" || type == "PREPRIO") { 
    cout << " " << quantum;
  }
  cout << endl;
  /*
  * For each process 
  */
  int index=0;
	for(auto proc : processes){
		totalCPU += proc->get_TC();
		totalTT += proc->get_TT();
		totalCW += proc->get_CW();
		printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
			index,proc->get_AT(),proc->get_TC(),proc->get_CB(),proc->get_IO(),proc->get_static_priority(),
      // the calculated stats
      proc->get_FT(), proc->get_TT(),proc->get_IT(),proc->get_CW());
    index++;
	}
  /*
  * Summary information
  */
  double utilization_CPU = 100.0*(totalCPU/(double)last_event_FT);
	double utilization_IO = 100.0*((totalIOTime)/(double)last_event_FT);

	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
		last_event_FT, utilization_CPU, utilization_IO, 
		(totalTT/(double)processes.size()), (totalCW/(double)processes.size()), 
		(processes.size()*(100/(double)last_event_FT))); 	

  return 0;
}