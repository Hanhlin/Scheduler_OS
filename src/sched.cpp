#include <fstream>
#include <iostream>
#include <string.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <queue>
#include <deque>
#include <vector>
#include <getopt.h>
#include <climits>

using namespace std;

bool verbose = false;
string schedType;
int quantum = 10000;    //for non-preemptive schedulers
int maxprios = 4;       //default maxprios for P and E schedulers
char inBuffer[1048576];
char rBuffer[1048576];
vector<int> randvals;
int ranNum;             //total number of random number
int ofs = 0;            // offset of the random values
int ranVal;
int CURRENT_TIME = 0;
int AT, TC, CB, IO;
int ioInd = 0;

struct Process {
    int pid, AT, TC, CB, IO, FT, TT, IT, CW, PRIO, dynamicPRIO; 
    int pre_state_time; //the create time of the previous state
    int remainTC;       //remaining total duration of required CPU time
    int remainCB;      //remaining cpu burst from the last time
};
struct Event {
    int Timestamp;
    Process *proc;
    int oldstate;
    int newstate;
    int transition;
};

deque<Process*> pro_ptr;
deque<Event*> eventQ;
Process *CURRENT_RUNNING_PROCESS;
vector<pair<string,int>> ioTime;       //storing starting & ending time of I/O

enum PRO_TRANS { TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT }; 
enum PRO_STATE { STATE_CREATED, STATE_READY, STATE_RUNNING, STATE_BLOCKED, STATE_PREEMPTED }; 

class schBase {
    public:         
        //virtual function
         virtual void add_process(Process *p) = 0;
         virtual Process* get_next_process() = 0;
         virtual bool test_preempt(Process *p) = 0;
};

schBase *THE_SCHEDULER;

class F: public schBase {
    private:
        deque<Process*> readyQ;     
    public:
        void add_process(Process *p) {
            readyQ.push_back(p);
        }
        Process* get_next_process() {
            if (readyQ.empty()) {
                return nullptr;
            }
            else {
                Process* nextP = readyQ.front();
                readyQ.pop_front();
                return nextP;
            }
        }
        bool test_preempt(Process *P) { return false; }
};

class L: public schBase {

    private:
        deque<Process*> readyQ; 
    public:
        void add_process(Process *p) {
            readyQ.push_front(p);
        }
        Process* get_next_process() {
            if (readyQ.empty()) {
                return nullptr;
            }
            else {
                Process* nextP = readyQ.front();
                readyQ.pop_front();
                return nextP;
            }
        }
        bool test_preempt(Process *P) { return false; }
};

class S: public schBase {
    private:
        deque<Process*> readyQ;
    public:
        void add_process(Process *p) {
            if (readyQ.empty()) {
                readyQ.push_back(p);
            }
            else {
                for (auto it = readyQ.begin(); it != readyQ.end(); it++) {
                    if ((*it)->remainTC > p->remainTC) {
                        readyQ.insert(it, p);
                        return;
                    }
                }
                readyQ.push_back(p);
            }
        }
        Process* get_next_process() {
            if (readyQ.empty()) return nullptr;
            Process* nextP = readyQ.front();
            readyQ.pop_front();
            return nextP;
        }
        bool test_preempt(Process *P) { return false; }
};

class R: public schBase {
    private:
        deque<Process*> readyQ;
    public:
        void add_process(Process *p) {
            readyQ.push_back(p);
        }
        Process* get_next_process() {
            if (readyQ.empty()) return nullptr;
            Process* nextP = readyQ.front();
            readyQ.pop_front();
            return nextP;
        }
        bool test_preempt(Process *P) { return false; }
};

class P: public schBase {
    private:
        deque<Process*> *activeQ;
        deque<Process*> *expriedQ;
    public:
        P(int maxprios) {
            activeQ = new deque<Process*>[maxprios];
            expriedQ = new deque<Process*>[maxprios];
        }
        void add_process(Process *p) {
            int level = p->dynamicPRIO;
            if (level == -1) {
                p->dynamicPRIO = p->PRIO - 1;
                expriedQ[p->dynamicPRIO].push_back(p);
            }
            else {
                activeQ[level].push_back(p);
            }
        }
        Process* get_next_process() {
            for (int i = maxprios - 1; i >= 0; i--) {
                if (activeQ[i].empty()) continue;
                Process* nextP = activeQ[i].front();
                activeQ[i].pop_front();
                return nextP;
            }
            //swap activeQ and expiredQ ptr + call THE functin again
            deque<Process*> *tempQ = expriedQ;
            expriedQ = activeQ;
            activeQ = tempQ;

            for (int i = maxprios - 1; i >= 0; i--) {
                if (activeQ[i].empty()) continue;
                Process* nextP = activeQ[i].front();
                activeQ[i].pop_front();
                return nextP;
            }
            return nullptr;
        }
        bool test_preempt(Process *P) { return false; }
};

class E: public schBase {
    private:
        deque<Process*> *activeQ;
        deque<Process*> *expriedQ;
    public:
        E(int maxprios) {
            activeQ = new deque<Process*>[maxprios];
            expriedQ = new deque<Process*>[maxprios];
        }
        void add_process(Process *p) {
            int level = p->dynamicPRIO;
            if (level == -1) {
                p->dynamicPRIO = p->PRIO - 1;
                expriedQ[p->dynamicPRIO].push_back(p);
            }
            else {
                activeQ[level].push_back(p);
            }
        }
        Process* get_next_process() {
            for (int i = maxprios - 1; i >= 0; i--) {
                if (activeQ[i].empty()) continue;
                Process* nextP = activeQ[i].front();
                activeQ[i].pop_front();
                return nextP;
            }
            //swap activeQ and expiredQ ptr + call THE functin again
            deque<Process*> *tempQ = expriedQ;
            expriedQ = activeQ;
            activeQ = tempQ;

            for (int i = maxprios - 1; i >= 0; i--) {
                if (activeQ[i].empty()) continue;
                Process* nextP = activeQ[i].front();
                activeQ[i].pop_front();
                return nextP;
            }
            return nullptr;
        }
        bool test_preempt(Process *p) {
            Event* e = nullptr;
            for (auto it = eventQ.begin(); it != eventQ.end(); it++) {
                if ((*it)->proc == CURRENT_RUNNING_PROCESS) e = (*it);
            }
            bool f1 = (p->dynamicPRIO > CURRENT_RUNNING_PROCESS->dynamicPRIO);
            bool f2 = CURRENT_TIME < e->Timestamp;
            if (verbose) {
                printf("---> PRIO preemption %d by %d ? %d TS=%d now=%d) --> %s\n"
                , e->proc->pid, p->pid, f1, e->Timestamp, CURRENT_TIME, (f1 && f2) ? "YES" : "NO");
            }
            return (f1 && f2);
        }
};

Event* get_event() {
    if (eventQ.empty()) {
        return nullptr;
    }
    Event* e = eventQ.front();
    eventQ.pop_front();
    return e;    
}

void put_event(Event* e) {
    if (eventQ.empty()) {
        eventQ.push_back(e);
    }
    else {
        for (auto it =eventQ.begin(); it != eventQ.end(); it++) {
            if ((*it)->Timestamp > e->Timestamp) {
                eventQ.insert(it, e);
                return;
            }
        }
        eventQ.push_back(e);
    }
}

void rm_event(Process* p) {
    for (auto it = eventQ.begin(); it != eventQ.end(); it++) {
        if ((*it)->proc->pid == p->pid) {
            eventQ.erase(it);
            break;
        }
    }
}

int get_next_event_time() {
    return (!eventQ.empty()) ? eventQ.front()->Timestamp : -1;
}

int myrandom(int burst) {
    if (ofs > ranNum) ofs = 1;
    return 1 + (randvals[ofs] % burst);
}

void Simulation() {
    Event* evt;
    bool CALL_SCHEDULER = false;

    while ((evt = get_event())) {
        Process *proc = evt->proc;
        CURRENT_TIME = evt->Timestamp;
        int timeInPrecState = CURRENT_TIME - proc->pre_state_time;
        //update the previous_state time with current time so that we can compute the interval
        proc->pre_state_time = CURRENT_TIME;
        int transition = evt->transition;
        int old_state = evt->oldstate;
        int new_state = evt->newstate;
        delete evt; evt = nullptr;

        switch (transition) {
        #pragma region TRANS_TO_READY
        case TRANS_TO_READY:
            //update create time of the previous event
            proc->pre_state_time = CURRENT_TIME;
            //update I/O time(IT, time in Blocked state) of the process
            proc->IT += timeInPrecState;
            CALL_SCHEDULER = true;

            if (old_state == STATE_BLOCKED) {
                //storing IO start time
                ioTime.push_back(make_pair("E", CURRENT_TIME));
                //reset
                proc->dynamicPRIO = proc->PRIO - 1;
            }
            #pragma region Verbose: tracking info
            if (verbose) {
                string fromState;
                switch (old_state) {
                case STATE_CREATED:
                    fromState = "CREATED";
                    break;
                
                case STATE_BLOCKED:
                    fromState = "BLOCK";
                    break;

                case STATE_PREEMPTED:
                    fromState = "RUNNG";
                    break;   
                }
                printf("%d %d %d: %s -> %s\n"
                        , CURRENT_TIME, proc->pid, timeInPrecState, fromState.c_str(), "READY");
            }
            #pragma endregion Verbose: tracking info

            //Preemption Happens
            if (CURRENT_RUNNING_PROCESS && THE_SCHEDULER->test_preempt(proc)) {
                rm_event(CURRENT_RUNNING_PROCESS);
                put_event( new Event {CURRENT_TIME, CURRENT_RUNNING_PROCESS, STATE_RUNNING, STATE_PREEMPTED, TRANS_TO_PREEMPT} );
            }
            THE_SCHEDULER->add_process(proc);

            break;
        #pragma endregion TRANS_TO_READY

        #pragma region TRANS_TO_RUN
        case TRANS_TO_RUN:
            CURRENT_RUNNING_PROCESS = proc;
            //update CPU waiting time(CW, time in Ready state) of the process
            proc->CW += timeInPrecState;
            //update the create time of the previous event
            proc->pre_state_time = CURRENT_TIME;

            //create event for either PREEMPTION or BLOCKING
            if (proc->remainCB == 0) {
                ofs++;
                proc->remainCB = myrandom(proc->CB);
                if (proc->remainTC < proc->remainCB) proc->remainCB = proc->remainTC;
            }

            //the process is done
            if (proc->remainTC <= proc->remainCB && proc->remainTC <= quantum) {
                #pragma region Verbose: tracking info
                if (verbose) {
                    printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n"
                            , CURRENT_TIME, proc->pid, timeInPrecState, "READY", "RUNNG", proc->remainTC, proc->remainTC, proc->dynamicPRIO);
                }
                #pragma endregion Verbose: tracking info
                put_event( new Event {CURRENT_TIME + proc->remainTC, proc, new_state, STATE_BLOCKED, TRANS_TO_BLOCK} );
            }
            //the process transit to block
            else if (proc->remainCB <= proc->remainTC && proc->remainCB <= quantum) {
                #pragma region Verbose: tracking info
                if (verbose) {
                    printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n"
                            , CURRENT_TIME, proc->pid, timeInPrecState, "READY", "RUNNG", proc->remainCB, proc->remainTC, proc->dynamicPRIO);
                }
                #pragma endregion Verbose: tracking info  
                put_event( new Event {CURRENT_TIME + proc->remainCB, proc, new_state, STATE_BLOCKED, TRANS_TO_BLOCK} );
            }
            //the process transit to preemption
            else if (quantum <= proc->remainTC && quantum <= proc->remainCB) {
                #pragma region Verbose: tracking info
                if (verbose) {
                    printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n"
                            , CURRENT_TIME, proc->pid, timeInPrecState, "READY", "RUNNG", proc->remainCB, proc->remainTC, proc->dynamicPRIO);
                }
                #pragma endregion Verbose: tracking info
                put_event( new Event {CURRENT_TIME + quantum, proc, new_state, STATE_PREEMPTED, TRANS_TO_PREEMPT} );
            }        
            
            break;
        #pragma endregion TRANS_TO_RUN

        #pragma region TRANS_TO_BLOCK
        case TRANS_TO_BLOCK:
            CALL_SCHEDULER = true;
            CURRENT_RUNNING_PROCESS = nullptr;
            proc->remainTC -= timeInPrecState;
            proc->remainCB -= timeInPrecState;

            //process DONE!
            if (proc->remainTC == 0) {
                proc->FT = CURRENT_TIME;
                proc->TT = CURRENT_TIME - proc->AT;
                if(verbose) {
                    printf("%d %d %d: Done\n", CURRENT_TIME, proc->pid, timeInPrecState);
                }
            }
            else {
                //storing IO start time
                ioTime.push_back(make_pair("S", CURRENT_TIME));

                ofs++;
                ranVal = myrandom(proc->IO);
                #pragma region Verbose: tracking info
                if (verbose) {
                    printf("%d %d %d: %s -> %s  ib=%d rem=%d\n"
                            , CURRENT_TIME, proc->pid, timeInPrecState, "RUNNG", "BLOCK", ranVal, proc->remainTC);
                }
                #pragma endregion Verbose: tracking info
                put_event( new Event {(CURRENT_TIME + ranVal), proc, new_state, STATE_BLOCKED, TRANS_TO_READY} );
            }
            
            break;
        #pragma endregion TRANS_TO_BLOCK

        #pragma region TRANS_TO_PREEMPT
        case TRANS_TO_PREEMPT:
            CALL_SCHEDULER = true;
            CURRENT_RUNNING_PROCESS = nullptr;
            proc->remainTC -= timeInPrecState;
            proc->remainCB -= timeInPrecState;

            if (verbose) {
                printf("%d %d %d: %s -> %s  cb=%d rem=%d prio=%d\n"
                        , CURRENT_TIME, proc->pid, timeInPrecState, "RUNNG", "READY", proc->remainCB, proc->remainTC, proc->dynamicPRIO);
            }
            if (schedType == "P" || schedType == "E") proc->dynamicPRIO--;
            THE_SCHEDULER->add_process(proc);
            break;
        #pragma endregion TRANS_TO_PREEMPT
        }
        
        if (CALL_SCHEDULER) {
            if (get_next_event_time() == CURRENT_TIME) {
                continue;
            }
            CALL_SCHEDULER = false;
            if (CURRENT_RUNNING_PROCESS == nullptr) {
                CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
                if (CURRENT_RUNNING_PROCESS == nullptr) {
                    continue;
                }
                put_event( new Event {CURRENT_TIME, CURRENT_RUNNING_PROCESS, STATE_READY, STATE_RUNNING, TRANS_TO_RUN});
            }
        }
    }
}

void printSchInfo() {

    #pragma region print schedType
    if (schedType == "F") {
        printf("FCFS\n");
    }
    else if (schedType == "L") {
        printf("LCFS\n");
    }
    else if (schedType == "S") {
        printf("SRTF\n");
    }
    else if (schedType == "R") {
        printf("RR %d\n", quantum);
    }
    else if(schedType == "P")  {
        printf("PRIO %d\n", quantum);
    } 
    else if (schedType == "E") {
        printf("PREPRIO %d\n", quantum);
    }
    #pragma endregion print schedType

    int proNum = pro_ptr.size();
    int lastFT = INT_MIN;
    double totalCPU;
    double ioUtil;
    double totalTT;
    double totalCW;
    int proCounter = 0;
    int sTime = 0;
    int eTime = 0;

    for (auto it = pro_ptr.begin(); it != pro_ptr.end(); it++) {
        if ((*it)->FT > lastFT) {
            lastFT = (*it)->FT;
        }
        totalCPU += (*it)->TC;
        totalTT += (*it)->TT;
        totalCW += (*it)->CW;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", (*it)->pid, (*it)->AT, (*it)->TC, (*it)->CB, (*it)->IO, (*it)->PRIO, (*it)->FT, (*it)->TT, (*it)->IT, (*it)->CW);
    }
    //compute total IO time
    for (auto it = ioTime.begin(); it != ioTime.end(); it++) {
        if (proCounter == 0) sTime = it->second;
        if (it->first == "S") {
            proCounter++;
        }
        else {
            proCounter--;
        }
        if (proCounter == 0) {
            ioUtil += (it->second - sTime);
        }
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", lastFT, (totalCPU/lastFT)*100.0, (ioUtil/lastFT)*100.0, totalTT/proNum, totalCW/proNum, (proNum/(double)lastFT)*100.0);
}

int main(int argc, char* argv[]) {

    int arg;
    setbuf(stdout, NULL);
    while ((arg = getopt(argc, argv, "vs:")) != -1) {
        switch (arg) {
        case 'v':
            verbose = true;
            break;

        case 's':
            string optargS(optarg);
            schedType = optargS.substr(0,1);

            #pragma region get Quamtum and Maxprios
            if (schedType == "F") {
                THE_SCHEDULER = new F();
                break;
            }
            else if (schedType == "L") {
                THE_SCHEDULER = new L();
                break;
            }
            else if (schedType == "S") {
                THE_SCHEDULER = new S();
                break;
            }
            else if (schedType == "R") {
                quantum = stoi(optargS.substr(1));
                THE_SCHEDULER = new R();
            }
            else if(schedType == "P") {
                quantum = stoi(optargS.substr(1,optargS.find(":")-1));
                if (optargS.find(":") != string::npos) {
                    maxprios = stoi(optargS.substr(optargS.find(":")+1));
                }
                THE_SCHEDULER = new P(maxprios);
            }
            else if(schedType == "E") {
                quantum = stoi(optargS.substr(1,optargS.find(":")-1));
                if (optargS.find(":") != string::npos) {
                    maxprios = stoi(optargS.substr(optargS.find(":")+1));
                }
                THE_SCHEDULER = new E(maxprios);
            }
            #pragma endregion get Quamtum and Maxprios

            break;
        }
    }

    #pragma region read rFile into a vector
    bool Skip = true;
    ifstream rFile(argv[optind+1]);
    while (rFile.getline(rBuffer, 1048576)) {
        int random;
        if (Skip) {
            random = stoi(rBuffer);
            ranNum = random;
            randvals.push_back(random);
            Skip = false;
            continue;
        }

        random = stoi(rBuffer);
        randvals.push_back(random);
    }
    #pragma endregion read rFile into a vector

    #pragma region read inputfile into process vector/ process pointer queue, and create event
    int i = 0;
    ifstream inFile(argv[optind]);
    while (inFile.getline(inBuffer, 1048576) && !inFile.eof()) {
        sscanf(inBuffer, "%d %d %d %d", &AT, &TC, &CB, &IO);
        ofs++;
        int static_PRIO = myrandom(maxprios);
        pro_ptr.push_back( new Process {i, AT, TC, CB, IO, 0, 0, 0, 0, static_PRIO, static_PRIO - 1, AT, TC, 0} );

        //create a process event for each process with "CREATE -> READY"
        eventQ.push_back( new Event {AT, pro_ptr.back(), STATE_CREATED, STATE_READY, TRANS_TO_READY} );
        i++;
    }
    #pragma endregion read inputfile into process vector/ process pointer queue, and create event

    while (!eventQ.empty()) {
        Simulation();
    }
    printSchInfo();
    return 0;
}