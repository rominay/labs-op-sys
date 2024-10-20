#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>
#include <queue>
#include <unistd.h> // For getopt
#include <cstdlib>  // For exit()
#include <regex>
#include <vector>

using namespace std;

FILE *inputfile;
FILE *randfile;
bool newProcess = true;
bool verbose=false;
const int BUFFER_SIZE = 4096;
char buffer[BUFFER_SIZE];
int ofs = 0;
vector<int> randvals;
int quantum;
int totalCPU = 0;
int totalTT = 0;
int totalCW = 0;
int totalIO = 0;
int maxprio;
int last_event_FT;
int totalIOTime = 0; 
int CURRENT_TIME;


typedef enum {STATE_CREATED, STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_DONE} process_state_t; //STATE_PREEMPT

const char* stateToString(process_state_t state) {
    switch (state) {
        case STATE_CREATED:     return "CREATED";
        case STATE_READY:    return "READY";
        case STATE_RUNNING:  return "RUNNG";
        case STATE_BLOCKED:  return "BLOCK";
        //case STATE_PREEMPT:  return "STATE_PREEMPT";
        case STATE_DONE:     return "DONE";
        //default:             return "Unknown State";
    }
}

int myrandom(int burst) {
	if (ofs == randvals.size()){ofs = 0;}
  else{ofs++;}
	return 1 + (randvals[ofs] % burst);
}

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
  int current_CPU_burst;
  int remaining_CPU_burst;
  int state_ts; // this is the time at which it was put to ready 
  int time_prev_state; // the amount of time in previous state
  int pid; // identifier
  process_state_t old_state=STATE_CREATED;
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
    current_CPU_burst=0;
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
  int get_remaining_CPU_burst() const {return remaining_CPU_burst;}
  int get_state_ts() const {return state_ts;}
  int get_time_prev_state() const {return time_prev_state;}
  int get_remaining_time() const {return TC - CPU_time;}
  // add setters
  void set_remaining_CPU_burst(int new_remaining_CPU_burst){remaining_CPU_burst=new_remaining_CPU_burst;}
  void set_FT(int new_FT) { FT=new_FT; }
  void set_state_ts(int new_state_ts) {state_ts=new_state_ts;}
  void set_time_prev_state(int new_time_prev_state) {time_prev_state=new_time_prev_state;}
};


struct CompareRemainingTime {
    bool operator()(Process* p_top, Process* p_to_add ) {
        return p_top->get_remaining_time() > p_to_add->get_remaining_time();  //shorter remaining time has higher priority
    }
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


// Base class for scheduling
class BaseScheduler {
public:
    virtual ~BaseScheduler() {} // virtual destructor 
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual string get_type() = 0;
    void virtual handle_preemption(Process* proc, Process* CRP, int time) = 0;
};
BaseScheduler *scheduler;


// FCFS Scheduler 
class FCFSScheduler : public BaseScheduler {
private:
    queue<Process*> runQueue; 

public:
    void handle_preemption(Process* proc, Process* CRP, int time){};
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
  void handle_preemption(Process* proc, Process* CRP, int time){};
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
	vector <Process *> runQueue;
public:
    void handle_preemption(Process* proc, Process* CRP, int time){};
    string get_type() override {return "SRTF";}
    Process *get_next_process() override {
	    if (runQueue.empty()){
			return NULL;
		}
		int shortestremainTime = runQueue[0]->get_remaining_time();
        int shortestIndex = 0;
        for(int i=1;i<runQueue.size();i++){
        if ((runQueue[i]->get_remaining_time()) < shortestremainTime){
            shortestremainTime=runQueue[i]->get_remaining_time();
            shortestIndex = i;
        }
        }
        Process* nextProcess = runQueue[shortestIndex];
        runQueue.erase(runQueue.begin()+shortestIndex);;
        return nextProcess;
        }

    void add_process(Process *p) override {
        runQueue.push_back(p);
	}
};

class RoundRobinScheduler : public BaseScheduler{
	queue<Process *> runQueue;
public:
  void handle_preemption(Process* proc, Process* CRP, int time){};
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
  PRIOScheduler(){// TO DO: pass maxprio?
    activeQ = new queue<Process*>[maxprio];
    expiredQ = new queue<Process*>[maxprio];

  }
  ~PRIOScheduler() {
    delete activeQ;
  }
  

  void handle_preemption(Process* proc, Process* CRP, int time){};
	Process* get_next_process() override {
    if (isEmpty(activeQ) && isEmpty(expiredQ)){
      return nullptr;
    }
    if (isEmpty(activeQ)){
      // we swap the pointers
      queue<Process*>* tempQ = activeQ;
      activeQ = expiredQ;
      expiredQ = tempQ;
    }
    Process* nextProcess;
    for (int i=maxprio-1; i>=0; --i){ // we go over higher priority to less priority
      if (!activeQ[i].empty()) {
        nextProcess = activeQ[i].front();
        activeQ[i].pop();
        return nextProcess;
      }
    }
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

class EPRIOScheduler : public BaseScheduler {
public:
    queue<Process*>* activeQ;
    queue<Process*>* expiredQ;

    EPRIOScheduler() {
        activeQ = new queue<Process*>[maxprio];
        expiredQ = new queue<Process*>[maxprio];
    }

    ~EPRIOScheduler() {
        delete[] activeQ;
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

    void add_process(Process* p) override {
        //handle_preemption(p, current_running_process, CURRENT_TIME);
        if (p->dynamic_priority == -1) {
            p->dynamic_priority = p->static_priority - 1; 
            expiredQ[p->dynamic_priority].push(p);
            return;
        }
        activeQ[p->dynamic_priority].push(p);
    }

    string get_type() override {
        return "PREPRIO";
    }

    void handle_preemption(Process* readyProcess, Process* runningProcess, int currentTime) {
        // if readyProcess process has a higher dynamic prio than current_running_process
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
            //runningProcess->remaining_CPU_burst=CURRENT_TIME-runningProcess->state_ts;// TODO: change
            //runningProcess->CPU_time-=CURRENT_TIME-runningProcess->state_ts; // TODO: change. we did not complete the burst 
            deslayer.put_event(currentTime, runningProcess, transition);

            // make the process ready
            //transition = STATE_READY;//STATE_RUNNING;
            //deslayer.put_event(currentTime, readyProcess, transition);
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
  //int CURRENT_TIME;
  bool CALL_SCHEDULER;
  int activeIOCount = 0;  
  int lastIOTransitionTime = 0;
  //int CPU_burst=0;


  while ((event= deslayer.get_event())){ // we call the deslayer to give us an event 
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    //if (CURRENT_TIME == 123) {
    //  printf("here");
    //}
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
          proc->dynamic_priority = proc->static_priority-1; // after coming from IO it is reset like this
        }

        if (proc->old_state==STATE_RUNNING) {
          // post-accounting:
          //proc->set_remaining_CPU_burst(proc->current_CPU_burst-timeInPrevState);
          proc->remaining_CPU_burst-=timeInPrevState;
          proc->CPU_time += timeInPrevState;
          current_running_process=nullptr;
          proc->dynamic_priority-=1; // we came from preemption
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
        //int CPU_burst;
        // create event for either preemption or blocking
        proc->CW+=proc->time_prev_state; // we know we come from READY
        int time_to_run;
        int time_remaining_to_run = proc->get_remaining_CPU_burst();
        if (time_remaining_to_run > 0){ // it means we did not exhaust previous CPU burst
          time_to_run = time_remaining_to_run;

        }
        else{ // we get a new CPU bust 
          int CPU_burst= myrandom(proc->get_CB());
          //proc->current_CPU_burst=CPU_burst;
          // the CPU_time of a process is the total time it used CPU 
          int remaining_CPU_time = proc->get_TC() - proc->CPU_time;
          if (remaining_CPU_time < CPU_burst){//if the remaining time is less than the CPU_burst , we run for the remaining time
            time_to_run = remaining_CPU_time;
          }
          else {// we run for the CPU_burst
            proc->remaining_CPU_burst = CPU_burst;
            time_to_run = CPU_burst;
          }
        }

        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< stateToString(proc->old_state) <<" -> "<<"RUNNG";
					cout <<" cb="<< time_to_run <<" rem="<<proc->TC-proc->CPU_time<< " prio="<< proc->dynamic_priority <<endl;
				}
        if (time_to_run > quantum){ // we will not finish 
          //(*proc).CPU_time += quantum;
          //proc->set_remaining_CPU_burst(time_to_run-quantum);
          process_state_t transition = STATE_READY;
          deslayer.put_event(CURRENT_TIME+quantum, proc, transition);
        }
        else{ // it goes to I/O
          //(*proc).CPU_time += time_to_run;
          process_state_t transition;
          //proc->set_remaining_CPU_burst(0);
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
        // check if we are done
        if (proc->CPU_time >= proc->get_TC()){ 
          transition = STATE_DONE; 
          proc->set_FT(CURRENT_TIME);
          deslayer.put_event(CURRENT_TIME, proc, transition);
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
        //(*proc).IT += IO_burst;
        process_state_t transition = STATE_READY;
        deslayer.put_event(CURRENT_TIME+IO_burst, proc, transition);
        CALL_SCHEDULER = true;
        current_running_process = nullptr;
        proc->old_state=STATE_BLOCKED;
        break;
        }

      case STATE_DONE:
        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< "DONE" << endl;
				}
        last_event_FT = CURRENT_TIME;
        proc->old_state=STATE_DONE;
        current_running_process=nullptr;
        CALL_SCHEDULER=true;
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

// Function to display help message
void display_help() {
    std::cout << "Usage: <program> [-v] [-s<schedspec>] inputfile randfile\n"
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
  string schedspec; // Variable to store the scheduling specification

  // Loop through all arguments
  while ((opt = getopt(argc, argv, "vs:")) != -1) {
      switch (opt) {
          case 'v':  // Enable verbose
              verbose = true;
              break;
          case 's':  // Scheduling specification (argument expected)
              schedspec = optarg;
              break;
          case '?':  // Handle unknown options or missing arguments
              std::cerr << "Unknown option or missing argument!" << std::endl;
              display_help();  // Show help message on error
              break;
      }
  }

  char* input_file = argv[optind];
  string rand_file = argv[optind + 1];



  
  /*
  * Open file with random numbers
  */
  ifstream randfile("lab2_assign/rfile");
  //ifstream randfile(rand_file);
	string rs;
	while(randfile>>rs){
		randvals.push_back(atoi(rs.c_str()));
	}
  /*
  * Open input file
  */
  //inputfile = fopen("input0","r");
  inputfile = fopen("lab2_assign/input0","r");
  //inputfile = fopen(input_file,"r");
  verbose = true;
  schedspec="R2";
  if (schedspec == "F") {
    scheduler = new FCFSScheduler();
    quantum = 10000;
    maxprio = 4; 
  }
  if (schedspec == "L") {
    scheduler = new LCFSScheduler();
    quantum = 10000;
    maxprio = 4; 
  }
  if (schedspec == "S") {
    scheduler = new SRTFScheduler();
    quantum = 10000;
    maxprio = 4; 
  }


  // Case 2: R<num> (e.g., R100)
  std::regex r_regex("^R([0-9]+)$");
  std::smatch r_match;
  if (std::regex_match(schedspec, r_match, r_regex)) {
      scheduler = new RoundRobinScheduler();
      maxprio = 4; 
      string str_quantum = r_match[1];
      quantum = stoi(str_quantum);
  }

  // Case 3: P<num>[:<maxprio>] (e.g., P10:20 or P10)
  std::regex p_regex("^P([0-9]+)(?::([0-9]+))?$");
  std::smatch p_match;
  if (std::regex_match(schedspec, p_match, p_regex)) {
      string str_quantum = p_match[1];
      quantum = stoi(str_quantum); // TO DO: check if quantum is required
      if (p_match[2].length() > 0) {
        string str_maxprios = p_match[2];
        maxprio = stoi(str_maxprios);
      } else{
        maxprio=4;
      }
      scheduler = new PRIOScheduler();
  }

  // Case 4: E<num>[:<maxprios>] (e.g., E5:10 or E5)
  std::regex e_regex("^E([0-9]+)(?::([0-9]+))?$");
  std::smatch e_match;
  if (std::regex_match(schedspec, e_match, e_regex)) {
      string str_quantum = e_match[1];
      quantum = stoi(str_quantum); 
      if (e_match[2].length() > 0) {
        string str_maxprios = e_match[2];
        maxprio = stoi(str_maxprios);
      } else{
        maxprio=4;
      }
      scheduler = new EPRIOScheduler();
  }
  
  
  int AT;
  int pid=0;
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
    Process* process = new Process(AT, TC, CB, IO, pid); 
    processes.push_back(process);
    process_state_t transition = STATE_READY;
    deslayer.put_event(AT, process, transition);
    newProcess=true; 
    pid++;  
  }
  /*
  * Logic for the quantum
  */

  //verbose=true;

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
    //totalIO += proc->get_IT();
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