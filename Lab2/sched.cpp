#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <list>
#include <queue>
#include <unistd.h> // For getopt
#include <cstdlib>  // For exit()
#include <regex>

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

typedef enum {STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_PREEMPT, STATE_DONE} process_state_t;

const char* stateToString(process_state_t state) {
    switch (state) {
        case STATE_READY:    return "READY";
        case STATE_RUNNING:  return "RUNNG";
        case STATE_BLOCKED:  return "BLOCK";
        case STATE_PREEMPT:  return "STATE_PREEMPT";
        case STATE_DONE:     return "DONE";
        default:             return "Unknown State";
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
  int remaining_CPU_burst;
  int state_ts; // this is the time at which it was put to ready 
  int time_prev_state; // the amount of time in previous state
  int pid; // identifier
  string old_state="CREATED";
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
  int get_remaining_CPU_burst() const {return remaining_CPU_burst;}
  int get_state_ts() const {return state_ts;}
  int get_time_prev_state() const {return time_prev_state;}
  int get_remaining_time() const {return TC - CPU_time;}
  // add setters
  //void set_oldstate(process_state_t new_old_state) {old_state=new_old_state;}
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

list<Event*> a_list;

class DesLayer{
private:
  //list<Event*> eventQ;
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
    //cout << "AddEvent(" << AT << ":" << process->pid << ":" << stateToString(transition)  << ")" << endl; // to comment
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
    //queue<Process*> runQueue;
    //int quantum;
    virtual ~BaseScheduler() {} // virtual destructor 
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual string get_type() = 0;
    //virtual int get_quantum() = 0;
    //virtual bool test_preempt(Process* activated_process);
};
BaseScheduler *scheduler;
// FCFS Scheduler 
class FCFSScheduler : public BaseScheduler {
private:
    queue<Process*> runQueue; 

public:
    //queue<Process*> runQueue;
    string get_type() override {return "FCFS";}
    Process* get_next_process() override {
        if (runQueue.empty()) {
            //cout << "No processes in the queue." << endl;
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
	vector <Process *> runQueue;
public:
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
  //int get_quantum() override {
  //  return quantum;
  //}
};

bool isEmpty(deque<Process*>* a_deque){
  for (size_t i=0; i<maxprio; i++){
    if (a_deque[i].size() != 0) {return false;}
  }
  return true;
};

class PRIOScheduler : public BaseScheduler{
	queue<Process*> activeQ0;
  queue<Process*> expiredQ0;

  queue<Process*> activeQ1;
  queue<Process*> expiredQ1;

  queue<Process*> activeQ2;
  queue<Process*> expiredQ2;

  queue<Process*> activeQ3;
  queue<Process*> expiredQ3;
public:
  string get_type() override {return "PRIO";}

	Process* get_next_process() override {
		if (activeQ0.empty() && activeQ1.empty() && activeQ2.empty() && activeQ3.empty() 
        && expiredQ0.empty() && expiredQ1.empty() && expiredQ2.empty() && expiredQ3.empty()){ //all are empty
      return nullptr;
    }
    if (activeQ0.empty() && activeQ1.empty() && activeQ2.empty() && activeQ3.empty()){ //all are empty
      activeQ0.swap(expiredQ0);
      activeQ1.swap(expiredQ1);
      activeQ2.swap(expiredQ2);
      activeQ3.swap(expiredQ3);
    }
    Process* nextProcess;
    if (!activeQ3.empty()){
      nextProcess = activeQ3.front();
      activeQ3.pop();
    } else if (!activeQ2.empty()){
      nextProcess = activeQ2.front();
      activeQ2.pop();
    } else if (!activeQ1.empty()){
      nextProcess = activeQ1.front();
      activeQ1.pop();
    } else if (!activeQ0.empty()){
      nextProcess = activeQ0.front();
      activeQ0.pop();
    }
    return nextProcess;
  }

  void add_process(Process *p) override {
    if (p->dynamic_priority == -1){
      p->dynamic_priority = p->static_priority-1; // it is reset
      if (p->dynamic_priority==0) {
        expiredQ0.push(p);
        return;
      }
      if (p->dynamic_priority==1) {
        expiredQ1.push(p);
        return;
      }
      if (p->dynamic_priority==2) {
        expiredQ2.push(p);
        return;
      }
      if (p->dynamic_priority==3) {
        expiredQ3.push(p);
        return;
      }
    } 
    if (p->dynamic_priority == 0) {activeQ0.push(p);}
    else if (p->dynamic_priority == 1) {activeQ1.push(p);}
    else if (p->dynamic_priority == 2) {activeQ2.push(p);}
    else if (p->dynamic_priority == 3) {activeQ3.push(p);}
	}
  
};


DesLayer deslayer;
Process* current_running_process;


//void print_scheduler(){
// show scheduler 
  //cout <<  "SCHED ";
  //queue <Process*> q = scheduler->runQueue; 
  //while (!q.empty()) {
  //  Process* front = q.front();
  //  cout << front->pid  << ":" << front->AT << " ";
  //  q.pop(); // Remove the element from the queue
  //}
  //cout << endl;
//};

void print_eventQ(){
  // show event Q
  cout << "EventQ "; 
  for (auto ev : deslayer.eventQ){
    Process* proc = ev->get_process();
    cout << ev->get_timestamp() << ":" << proc->pid << " ";
  }
  cout << endl;
};
void simulation(){
  Event* event;
  int CURRENT_TIME;
  bool CALL_SCHEDULER;
  int activeIOCount = 0;  
  int lastIOTransitionTime = 0;

  while ((event= deslayer.get_event())){ // we call the deslayer to give us an event 
    Process* proc = event->get_process();
    CURRENT_TIME = event->get_timestamp();
    //if (CURRENT_TIME == 44) {
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
      case STATE_READY: // TRANS_TO_READY
        
        if (proc->old_state=="BLOCK"){ // it not the first time put on ready
          activeIOCount--; // it stopped being doing IO
          if (activeIOCount == 0) {  // transition 1 -> 0 
            totalIOTime += CURRENT_TIME - lastIOTransitionTime;
          }
          proc->dynamic_priority = proc->static_priority-1; // after coming from IO it is reset like this
        }

        if (proc->old_state=="RUNNG") {
          current_running_process=nullptr;
          proc->dynamic_priority-=1; // we came from preemption
          //if (proc->dynamic_priority==-1){proc->dynamic_priority = proc->static_priority-1;}
        }
        //current_running_process=nullptr;
        CALL_SCHEDULER = true;
        scheduler->add_process(proc);
        if (verbose) {
          if (proc->old_state=="RUNNG"){ //|| proc->old_state=="PREEMP"
            cout << CURRENT_TIME<<" "<< proc->pid <<" "<< timeInPrevState<< ": " << "RUNNG"<<" -> "<< "READY";
            cout <<" cb="<< proc->remaining_CPU_burst <<" rem="<<proc->TC-proc->CPU_time<< " prio="<< proc->dynamic_priority << endl;
          }
          else {
            cout << CURRENT_TIME<<" "<< proc->pid <<" "<< timeInPrevState<< ": " << proc->old_state<<" -> "<< "READY" << endl;
          }
        }

        proc->old_state="READY";
        break;
      //case STATE_PREEMPT: // similar to TRANS_TO_READY
        // must come from RUNNING (preemption)
        // add to runqueue (no event is generated)
      //  {
      //  proc->dynamic_priority-=1;
      //  if (proc->dynamic_priority==-1) proc->dynamic_priority=proc->static_priority-1; // TO DO: check if this is correct
      //  scheduler->add_process(proc);
      //  CALL_SCHEDULER = true;
        
        //if (verbose) {
				//	cout << CURRENT_TIME<<" "<< proc->pid <<" "<< timeInPrevState<< ": " << proc->old_state<<" -> "<< "PREEMP" <<endl;
				//}
        
      //  proc->old_state="READY";
        
      //  break;
      //  }
      case STATE_RUNNING:
        {
        int CPU_burst;
        // create event for either preemption or blocking
        //current_running_process = proc;
        proc->CW+=proc->time_prev_state; // we know we come from READY
        int time_to_run;
        int time_remaining_to_run = proc->get_remaining_CPU_burst();
        if (time_remaining_to_run > 0){ // it means we did not exhaust previous CPU burst
          time_to_run = time_remaining_to_run;

        }
        else{ // we get a new CPU bust 
          CPU_burst= myrandom(proc->get_CB());
          // the CPU_time of a process is the total time it used CPU 
          int remaining_CPU_time = proc->get_TC() - (*proc).CPU_time;
          if (remaining_CPU_time < CPU_burst){//if the remaining time is less than the CPU_burst , we run for the remaining time
            time_to_run = remaining_CPU_time;
          }
          else {// we run for the CPU_burst
            time_to_run = CPU_burst;
          }
        }

        if (verbose) {
          string old_state = proc->old_state;
          //if (old_state=="PREEMPT") {old_state="READY";}
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< old_state <<" -> "<<"RUNNG";
					cout <<" cb="<< time_to_run <<" rem="<<proc->TC-proc->CPU_time<< " prio="<< proc->dynamic_priority <<endl;
				}
        //cout << quantum <<endl;
        if (time_to_run > quantum){ // we will not finish 
          (*proc).CPU_time += quantum;
          proc->set_remaining_CPU_burst(time_to_run-quantum);
          //current_running_process=nullptr;
          //if (proc == current_running_process) {
          //current_running_process = nullptr;
          //}
          //proc->dynamic_priority-=1; // we will go to preemption
          process_state_t transition = STATE_READY;
          deslayer.put_event(CURRENT_TIME+quantum, proc, transition);
        }
        else{ // it goes to I/O
          (*proc).CPU_time += time_to_run;
          process_state_t transition;
          proc->set_remaining_CPU_burst(0);
          if ((*proc).CPU_time < proc->get_TC()){ // we are still not done
            transition = STATE_BLOCKED;
          }
          else{ // we are done 
            transition = STATE_DONE; 
            proc->set_FT(CURRENT_TIME+time_to_run);
          }
          deslayer.put_event(CURRENT_TIME+time_to_run, proc, transition);
        }
        proc->old_state="RUNNG";
        break;
        }
        
      case STATE_BLOCKED:
        {
        activeIOCount++; // it is doing IO 
        if (activeIOCount == 1) {  // transition 0 -> 1 
          lastIOTransitionTime = CURRENT_TIME;
        }
        //create an event for when process becomes READY again
        int IO_burst= myrandom(proc->get_IO());
        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< proc->old_state <<" -> "<<"BLOCK";
					cout <<" ib="<< IO_burst <<" rem="<<proc->TC-proc->CPU_time <<endl;
				}
        (*proc).IT += IO_burst;
        process_state_t transition = STATE_READY;
        deslayer.put_event(CURRENT_TIME+IO_burst, proc, transition);
        CALL_SCHEDULER = true;
        //if (proc == current_running_process) {
        current_running_process = nullptr;
        //}
        proc->old_state="BLOCK";
        break;
        }

      case STATE_DONE:
        if (verbose) {
					cout<< CURRENT_TIME<<" "<<proc->pid<<" "<< timeInPrevState <<": "<< "DONE" << endl;
				}
        last_event_FT = CURRENT_TIME;
        //current_running_process = nullptr;
        //if (proc == current_running_process) {
        //  current_running_process = nullptr;
        //}
        proc->old_state="DONE";
        current_running_process=nullptr;
        CALL_SCHEDULER=true;
    }

    if (CALL_SCHEDULER) {
      
      if (deslayer.get_next_event_time() == CURRENT_TIME){
        //event = deslayer.get_event();
        continue; 
      }
      CALL_SCHEDULER = false;
      if (current_running_process == nullptr){
        current_running_process = scheduler->get_next_process();
        //print_scheduler();
        if (current_running_process == nullptr){
          //event = deslayer.get_event();
          continue;
        }
        // create event to make this process runnable for same time
        process_state_t transition = STATE_RUNNING;
				deslayer.put_event(CURRENT_TIME, current_running_process, transition);
				//current_running_process = nullptr;
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

// Function to parse the scheduling specification
bool parse_schedspec(const std::string& spec) {
    // Case 1: FLS (no arguments)
    if (spec == "F") {
      scheduler = new FCFSScheduler();
      quantum = 10000;
      maxprio = 4; 
      return true;
    }
    if (spec == "L") {
      scheduler = new LCFSScheduler();
      quantum = 10000;
      maxprio = 4; 
      return true;
    }
    if (spec == "S") {
      scheduler = new SRTFScheduler();
      quantum = 10000;
      maxprio = 4; 
      return true;
    }


    // Case 2: R<num> (e.g., R100)
    std::regex r_regex("^R([0-9]+)$");
    std::smatch r_match;
    if (std::regex_match(spec, r_match, r_regex)) {
        //std::cout << "Scheduling specification: R<num>\n";
        //std::cout << "  num: " << r_match[1] << std::endl;
        scheduler = new RoundRobinScheduler();
        //quantum = 10000;
        maxprio = 4; 
        string str_quantum = r_match[1];
        quantum = stoi(str_quantum);
        //cout << "quantum: " << quantum << endl;
        return true;
    }

    // Case 3: P<num>[:<maxprio>] (e.g., P10:20 or P10)
    std::regex p_regex("^P([0-9]+)(?::([0-9]+))?$");
    std::smatch p_match;
    if (std::regex_match(spec, p_match, p_regex)) {
        //std::cout << "Scheduling specification: P<num>[:<maxprio>]\n";
        //std::cout << "  num: " << p_match[1];
        scheduler = new PRIOScheduler();
        string str_quantum = p_match[1];
        quantum = stoi(str_quantum); // TO DO: check if quantum is required
        if (p_match[2].length() > 0) {
          //cout << p_match[2];
          string str_maxprios = p_match[2];
          maxprio = stoi(str_maxprios);
            //std::cout << ", maxprio: " << p_match[2];
        } else{
          maxprio=4;
        }
        //std::cout << std::endl;
        return true;

    }

    // Case 4: E<num>[:<maxprios>] (e.g., E5:10 or E5)
    std::regex e_regex("^E([0-9]+)(?::([0-9]+))?$");
    std::smatch e_match;
    if (std::regex_match(spec, e_match, e_regex)) {
        std::cout << "Scheduling specification: E<num>[:<maxprios>]\n";
        std::cout << "  num: " << e_match[1];
        if (e_match[2].length() > 0) {
            std::cout << ", maxprios: " << e_match[2];
        }
        std::cout << std::endl;
        return true;
    }

    // Invalid format
    std::cerr << "Invalid scheduling specification format: " << spec << std::endl;
    return false;
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
  //if (!schedspec.empty()) std::cout << "Scheduling specification: " << schedspec << std::endl;
  parse_schedspec(schedspec);
  // Ensure that the required input and random files are provided
  if (optind + 2 > argc) {
      std::cerr << "Error: Missing inputfile or randfile.\n";
      display_help();
  }

  char* input_file = argv[optind];
  string rand_file = argv[optind + 1];



  
  /*
  * Open file with random numbers
  */
  //ifstream randfile("lab2_assign/rfile");
  ifstream randfile(rand_file);
	string rs;
	while(randfile>>rs){
		randvals.push_back(atoi(rs.c_str()));
	}
  /*
  * Open input file
  */
  //inputfile = fopen("input0","r");
  //inputfile = fopen("lab2_assign/input0","r");
  inputfile = fopen(input_file,"r");
  //maxprio=3;
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
  //scheduler = new PRIOScheduler();
  //if (scheduler->get_type() == "FCFS"){
  //quantum = 5;
  //maxprio=3;
  //verbose=true;
 // }
  simulation();
  /*
  * Output
  */ 
  string type = scheduler->get_type();
  cout<<type;
  if (type == "RR" || type == "PRIO") { 
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
  //printf(last_event_FT);
  double utilization_CPU = 100.0*(totalCPU/(double)last_event_FT);
	double utilization_IO = 100.0*((totalIOTime)/(double)last_event_FT);

	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
		last_event_FT, utilization_CPU, utilization_IO, 
		(totalTT/(double)processes.size()), (totalCW/(double)processes.size()), 
		(processes.size()*(100/(double)last_event_FT))); 	

  return 0;
}