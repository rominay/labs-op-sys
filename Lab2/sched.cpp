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

typedef enum {STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_PREEMPT, STATE_DONE} process_state_t;

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
  // constructor
  Process(int AT, int TC, int CB, int IO){
    this->AT = AT;
    this->TC = TC;
    this->CB = CB;
    this->IO = IO;
    FT = 0;
		TT = 0;
		IT = 0;
    static_priority = myrandom(maxprio);
    dynamic_priority = static_priority-1;
		CW = 0;
    CPU_time=0;
    remaining_CPU_burst=0;
    state_ts=0;
    time_prev_state=0; 
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
  // add setters
  //void set_oldstate(process_state_t new_old_state) {old_state=new_old_state;}
  void set_remaining_CPU_burst(int new_remaining_CPU_burst){remaining_CPU_burst=new_remaining_CPU_burst;}
  void set_FT(int new_FT) { FT=new_FT; }
  void set_state_ts(int new_state_ts) {state_ts=new_state_ts;}
  void set_time_prev_state(int new_time_prev_state) {time_prev_state=new_time_prev_state;}
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
    int quantum;
    virtual ~BaseScheduler() {} // virtual destructor 
    virtual Process* get_next_process() = 0;
    virtual void add_process(Process* p) = 0; 
    virtual string get_type() = 0;
    //virtual bool test_preempt(Process* activated_process);
};
BaseScheduler *scheduler;
// FCFS Scheduler 
class FCFSScheduler : public BaseScheduler {
private:
    queue<Process*> runQueue; 

public:
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
    int timeInPrevState =  CURRENT_TIME - proc->state_ts;
    proc->set_time_prev_state(timeInPrevState);
    delete event; event = nullptr;

    switch(transition){
      case STATE_READY: // TRANS_TO_READY
        // must come from BLOCKED or CREATED
        // add to run queue, no event created
        proc->state_ts=CURRENT_TIME;
        scheduler->add_process(proc);
        CALL_SCHEDULER = true;
        break;
      case STATE_PREEMPT: // similar to TRANS_TO_READY
        // must come from RUNNING (preemption)
        // add to runqueue (no event is generated)
        proc->dynamic_priority-=1;
        if (proc->dynamic_priority==-1) proc->static_priority-1; // TO DO: check if this is correct
        scheduler->add_process(proc);
        CALL_SCHEDULER = true;
        break;
      case STATE_RUNNING:
        {
        // create event for either preemption or blocking
        //current_running_process = proc;
        proc->CW+=proc->time_prev_state;
        int time_to_run;
        int time_remaining_to_run = proc->get_remaining_CPU_burst();
        if (time_remaining_to_run > 0){ // it means we did not exhaust previous CPU burst
          time_to_run = time_remaining_to_run;
        }
        else{ // we get a new CPU bust 
          int CPU_burst= myrandom(proc->get_CB());
          // the CPU_time of a process is the total time it used CPU 
          int remaining_CPU_time = proc->get_TC() - (*proc).CPU_time;
          if (remaining_CPU_time < CPU_burst){//if the remaining time is less than the CPU_burst , we run for the remaining time
            time_to_run = remaining_CPU_time;
          }
          else {// we run for the CPU_burst
            time_to_run = CPU_burst;
          }
        }
        if (time_to_run > quantum){ // we will not finish 
          (*proc).CPU_time += time_to_run;
          proc->set_remaining_CPU_burst(time_to_run-quantum);
          process_state_t transition = STATE_PREEMPT;
          deslayer.put_event(CURRENT_TIME+time_to_run, proc, transition);
        }
        else{ // it goes to I/O
          (*proc).CPU_time += time_to_run;
          process_state_t transition;
          if ((*proc).CPU_time < proc->get_TC()){ // we are still not done
            transition = STATE_BLOCKED; 
          }
          else{ // we are done 
            transition = STATE_DONE; 
            proc->set_FT(CURRENT_TIME+time_to_run);
          }
          deslayer.put_event(CURRENT_TIME+time_to_run, proc, transition);
        }
				//}
        }
        break;
      case STATE_BLOCKED:
        {
        //create an event for when process becomes READY again
        int IO_burst= myrandom(proc->get_IO());
        (*proc).IT += IO_burst;
        process_state_t transition = STATE_READY;
        deslayer.put_event(CURRENT_TIME+IO_burst, proc, transition);
        CALL_SCHEDULER = true;
        break;
        }
      case STATE_DONE:
        last_event_FT = CURRENT_TIME;
        current_running_process = NULL;
    }

    if (CALL_SCHEDULER) {
      if (deslayer.get_next_event_time() == CURRENT_TIME){
        //event = deslayer.get_event();
        continue; 
      }
      CALL_SCHEDULER = false;
      if (current_running_process == nullptr){
        current_running_process = scheduler->get_next_process();
        if (current_running_process == nullptr){
          //event = deslayer.get_event();
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
        
      return true;
    }
    if (spec == "S") {
        
      return true;
    }

    // Case 2: R<num> (e.g., R100)
    std::regex r_regex("^R([0-9]+)$");
    std::smatch r_match;
    if (std::regex_match(spec, r_match, r_regex)) {
        std::cout << "Scheduling specification: R<num>\n";
        std::cout << "  num: " << r_match[1] << std::endl;
        return true;
    }

    // Case 3: P<num>[:<maxprio>] (e.g., P10:20 or P10)
    std::regex p_regex("^P([0-9]+)(?::([0-9]+))?$");
    std::smatch p_match;
    if (std::regex_match(spec, p_match, p_regex)) {
        std::cout << "Scheduling specification: P<num>[:<maxprio>]\n";
        std::cout << "  num: " << p_match[1];
        if (p_match[2].length() > 0) {
            std::cout << ", maxprio: " << p_match[2];
        }
        std::cout << std::endl;
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
  //inputfile = fopen("lab2_assign/input2","r");
  inputfile = fopen(input_file,"r");

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
    processes.push_back(process);
    process_state_t transition = STATE_READY;
    deslayer.put_event(AT, process, transition);
    newProcess=true;   
  }
  /*
  * Logic for the quantum
  */
  //scheduler = new FCFSScheduler();
  //if (scheduler->get_type() == "FCFS"){
  //  quantum = 10000;
  //}
  simulation();
  /*
  * Output
  */
  cout<<scheduler->get_type()<<endl;
  /*
  * For each process 
  */
  int index=0;
	for(auto proc : processes){
		totalCPU += proc->get_TC();
		totalTT += proc->get_TT();
		totalCW += proc->get_CW();
    totalIO += proc->get_IT();
		printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
			index,proc->get_AT(),proc->get_TC(),proc->get_CB(),proc->get_IO(),proc->get_static_priority(),
      // the calculated stats
      proc->get_FT(), proc->get_TT(),proc->get_IT(),proc->get_CW());
    index++;
	}
  /*
  * Summary information
  */
  double utilization_CPU = 100.0*totalCPU/(double)last_event_FT;
	double utilization_IO = 100.0*totalIO/(double)last_event_FT;

	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
		last_event_FT, utilization_CPU, utilization_IO, 
		(totalTT/(double)processes.size()), (totalCW/(double)processes.size()), 
		(processes.size()*100/(double)last_event_FT)); 	

  return 0;
}