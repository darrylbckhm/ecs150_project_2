#include <iostream>
#include <unistd.h>
#include <vector>
#include <queue>
#include "stdint.h"

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {

  using namespace std;

  class TCB {

    public: 
      TVMMemorySize memsize;
      TVMStatus status;
      TVMTick tick;
      TVMThreadID threadID;
      TVMMutexID mutexID;
      TVMThreadPriority priority;
      TVMThreadState state;
      unsigned int sleepCount;
      // not sleeping = 0; sleeping = 1;
      unsigned int sleep;

      SMachineContextRef mcntxref;

      TVMThreadEntry entry;
      void *param;

      void (*TVMMainEntry)(int, char*[]);
      void (*TVMThreadEntry)(void *);

      void AlarmCall(void *param);

      TMachineSignalStateRef sigstate;

  };


  // vector of all threads
  static vector<TCB*> threads;

  // queue of threads sorted by priority
  static queue<TCB*> prioQueue;

  static TCB *curThread;
  static TVMThreadID currentThreadID;
  volatile static SMachineContext mcntx;

  // declaration of custom functions
  TVMMainEntry VMLoadModule(const char *module);
  void skeleton(void *param);
  void Scheduler();
  void idle(void *param);
  void printThreadInfo();

  void printThreadInfo()
  {
    cout << endl;
    cout << "size of threads vector: " << threads.size() << endl;

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      TCB *thread = *itr;
      cout << "threadID: " << thread->threadID << ", ";
      cout << "threadState: ";
      switch(thread->state) {
        case VM_THREAD_STATE_DEAD:  cout << "Dead";
                                    break;
        case VM_THREAD_STATE_RUNNING:  cout << "Running";
                                       break;
        case VM_THREAD_STATE_READY:  cout << "Ready";
                                     break;
        case VM_THREAD_STATE_WAITING:  cout << "Waiting";
                                       break;
        default:                    break;
      }
      cout << ", sleepStatus: " << thread->sleep;
      cout << endl;
    }
    cout << endl;
  }

  void skeleton(void *param)
  {
    //printThreadInfo();
    TCB *thread = (TCB *)param;

    thread->entry(thread->param);


    //VMThreadTerminate(curThread->threadID);

  }

  void Scheduler()
  {

    //status = VM_THREAD_STATE_WAITING; 

    /*for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
      {

      if((*itr)->status == VM_THREAD_STATE_READY)
      {

      if((*itr)->priority < (*(itr++))->priority)
      {

      continue;


      }

      else
      {

      prioQueue.push((*itr));

      }

      }

      }*/

    if (prioQueue.size() != 0)
    {


      TCB *newThread = prioQueue.front();
      prioQueue.pop();

      SMachineContextRef mcntxrefOld;

      for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
      {
        if ((*itr)->threadID == currentThreadID)
        {
          mcntxrefOld = (*itr)->mcntxref;
        }
      }


      SMachineContextRef mcntxrefNew = newThread->mcntxref;
      MachineContextSwitch(mcntxrefOld, mcntxrefNew);
    }

  }

  void idle(void *param)
  {
    while(1)
    {
    }
  }

  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    TVMThreadID id = threads.size();

    TCB *thread = new TCB;
    thread->memsize = memsize;
    thread->status = VM_STATUS_SUCCESS;
    //thread->tick;
    thread->sleep = 0;
    thread->threadID = id;
    thread->mutexID = 0;
    thread->priority = prio;
    thread->state = VM_THREAD_STATE_DEAD;
    thread->entry =  entry;
    thread->param = param;
    thread->mcntxref = new SMachineContext;

    threads.push_back(thread);

    curThread = thread;

    *tid = id;

    MachineResumeSignals(sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    int8_t *stack = new int8_t[curThread->memsize];
    size_t stacksize = sizeof(stack);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {
        (*itr)->mcntxref = new SMachineContext;
        MachineContextCreate((*itr)->mcntxref, skeleton, threads.back(), stack, stacksize);
        (*itr)->state = VM_THREAD_STATE_READY;
        prioQueue.push(*itr);
      }
    }

    MachineResumeSignals(sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadTerminate(TVMThreadID thread)
  {

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if((*itr)->threadID == thread)
      {

        if((*itr)->state == VM_THREAD_STATE_DEAD)
        {

          return VM_STATUS_ERROR_INVALID_STATE;

        }

        else
        {

          (*itr)->state = VM_THREAD_STATE_DEAD;
          return VM_STATUS_SUCCESS;

        }

      }

      else
      {

        return VM_STATUS_ERROR_INVALID_ID;

      }

    }

    return VM_STATUS_FAILURE;

  }

  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {
        *stateref = (*itr)->state;
      }
    }

    MachineResumeSignals(sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {
    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_RUNNING)
      {
        (*itr)->sleepCount = tick;
        (*itr)->state = VM_THREAD_STATE_WAITING;
        (*itr)->sleep = 1;
        Scheduler();
      }
    }


    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMTickMS(int *tickmsref)
  {
    return VM_STATUS_SUCCESS;

  }

  void AlarmCall(void *param)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_WAITING)
      {
        if ((*itr)->sleep == 1)
        {
          (*itr)->sleepCount = (*itr)->sleepCount - 1;
          cout << "sleepCount: " << (*itr)->sleepCount << endl;
          if ((*itr)->sleepCount == 0)
          {
            (*itr)->sleep = 0;
            (*itr)->state = VM_THREAD_STATE_READY;
          }
        }
      }
    }

    MachineResumeSignals(sigstate);
    Scheduler();

  }



  TVMStatus VMStart(int tickms, int argc, char *argv[])
  {
    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    MachineInitialize();
    MachineRequestAlarm(tickms*1000, AlarmCall, NULL);
    MachineEnableSignals();   

    TVMThreadID VMThreadID;
    VMThreadCreate(NULL, NULL, 0x100000, VM_THREAD_PRIORITY_HIGH, &VMThreadID);
    threads[0]->state = VM_THREAD_STATE_RUNNING;

    VMThreadCreate(idle, NULL, 0x100000, VM_THREAD_PRIORITY_LOW, &VMThreadID);
    VMThreadActivate(VMThreadID);

    main_entry(argc, argv);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
  { 
    if (*length == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    write(filedescriptor, data, *length);
    return VM_STATUS_SUCCESS;
  }

}
