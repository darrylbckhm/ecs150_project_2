#include <iostream>
#include <unistd.h>
#include <vector>
#include <queue>
#include <stdint.h>
#include <cstdlib>

#include "VirtualMachine.h"
#include "Machine.h"

#define VM_THREAD_PRIORITY_IDLE 0

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
      char *stackAddr;
      volatile unsigned int sleepCount;
      // not sleeping = 0; sleeping = 1;
      unsigned int sleep;
      unsigned int addedToQueue;
      unsigned int fileCallFlag;
      volatile unsigned int fileCallData;

      SMachineContext mcntx;

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
  static queue<TCB*> highQueue;
  static queue<TCB*> normalQueue;
  static queue<TCB*> lowQueue;

  static TCB *curThread;
  static TVMThreadID currentThreadID;

  // declaration of custom functions
  TVMMainEntry VMLoadModule(const char *module);
  void skeleton(void *param);
  void Scheduler(bool activate);
  void idle(void *param);
  void printThreadInfo();

  void printThreadInfo()
  {
    cout << endl;
    cout << "size of threads vector: " << threads.size() << endl;
    cout << "size of highQueue: " << highQueue.size() << endl;
    cout << "size of normalQueue: " << normalQueue.size() << endl;
    cout << "size of lowQueue: " << lowQueue.size() << endl;
    cout << "currentThread: " << curThread->threadID << endl;

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
      cout << ", addedToQueue: " << thread->addedToQueue;
      cout << endl;
    }
    cout << endl;
  }

  void skeleton(void *param)
  {
    //cout << "a" << endl;
    MachineEnableSignals();
    //printThreadInfo();
    TCB *thread = (TCB *)param;

    thread->entry(thread->param);

    VMThreadTerminate(curThread->threadID);

  }

  void Scheduler(bool activate)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    //MachineSuspendSignals(sigstate);

    TCB *prevThread = curThread;
    currentThreadID = prevThread->threadID;

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if(((*itr)->state == VM_THREAD_STATE_READY) && ((*itr)->addedToQueue == 0))
      {

        if((*itr)->priority == VM_THREAD_PRIORITY_HIGH)
        {
          
          (*itr)->addedToQueue = 1;
          highQueue.push((*itr));

        }

        else if((*itr)->priority == VM_THREAD_PRIORITY_NORMAL)
        {
          
          (*itr)->addedToQueue = 1;
          normalQueue.push((*itr));

        }

        else if((*itr)->priority == VM_THREAD_PRIORITY_LOW)
        {

          (*itr)->addedToQueue = 1;
          lowQueue.push((*itr));

        }

      }

    }
    
    if (!activate)
    {
      //printThreadInfo();

      if(!highQueue.empty())
      {

        curThread = highQueue.front();
        highQueue.pop();

      }

      else if(!normalQueue.empty())
      {

        curThread = normalQueue.front();
        normalQueue.pop();

      }

      else if(!lowQueue.empty())
      {

        curThread = lowQueue.front();
        lowQueue.pop();

      }

      curThread->addedToQueue = 0;


      if ((curThread->priority < prevThread->priority) && (prevThread->state == VM_THREAD_STATE_RUNNING))
      {
        curThread = prevThread;
        return;
      }
      


      if (prevThread->threadID == curThread->threadID)
        return;

      if (prevThread->state == VM_THREAD_STATE_RUNNING)
        prevThread->state = VM_THREAD_STATE_READY;
      
      //cout << "prevThread: " << prevThread->threadID << endl;
      //cout << "curThread (nextThread): " << curThread->threadID << endl;

      curThread->state = VM_THREAD_STATE_RUNNING; 
      //printThreadInfo();
      //cout << endl << "context save" << endl;
      if (MachineContextSave(&prevThread->mcntx) == 0)
      {
        //cout << "context restore" << endl;
        //cerr << static_cast<void *>(&curThread->mcntx) << endl;
        MachineContextRestore(&curThread->mcntx);
        //cout << "a" << endl;
      }
      //MachineContextSwitch(&prevThread->mcntx, &curThread->mcntx);
      //cout << "done" << endl << endl;
    }

      MachineResumeSignals(sigstate);

  }

  void idle(void *param)
  {
    //cout << "a" << endl;

    while(1)
    {

      ;

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

    threads.push_back(thread);

    //curThread = thread;

    *tid = id;

    MachineResumeSignals(sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {
        (*itr)->stackAddr = new char[(*itr)->memsize];
        size_t stacksize = (*itr)->memsize;

        MachineContextCreate(&(*itr)->mcntx, skeleton, *itr, (*itr)->stackAddr, stacksize);
        //cerr << static_cast<void *>(&(*itr)->mcntx) << endl;
        (*itr)->state = VM_THREAD_STATE_READY;
      }
    }

    MachineResumeSignals(sigstate);
    Scheduler(true);

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
          Scheduler(false);
          return VM_STATUS_SUCCESS;

        }

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
        //printThreadInfo();
        Scheduler(false);
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
    //cout << "a" << endl;
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_WAITING)
      {
        if ((*itr)->sleep == 1)
        {
          (*itr)->sleepCount = (*itr)->sleepCount - 1;
          //cout << "sleepCount: " << (*itr)->sleepCount << endl;
          if ((*itr)->sleepCount == 0)
          {
            (*itr)->sleep = 0;
            (*itr)->state = VM_THREAD_STATE_READY;
            //printThreadInfo();
          }
        }
      }
    }

    MachineResumeSignals(sigstate);
    Scheduler(false);

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
    curThread = threads[0];

    VMThreadCreate(idle, NULL, 0x100000, VM_THREAD_PRIORITY_LOW, &VMThreadID);
    VMThreadActivate(VMThreadID);

    //printThreadInfo();

    main_entry(argc, argv);

    return VM_STATUS_SUCCESS;
  }

  void fileCallback(void *calldata, int result)
  {

    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    curThread->fileCallFlag = 1;

    TCB* thread = (TCB*)calldata;
    thread->fileCallData = result;

    thread->state = VM_THREAD_STATE_READY;

    //cout << "FileCallback Result: " << thread->fileCallData << endl;

    curThread->fileCallFlag = 0;

    MachineResumeSignals(sigstate);

    Scheduler(false);    

  }

  TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
  {

    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    if (newoffset != NULL)
      *newoffset = offset;
    else
      return VM_STATUS_FAILURE;

    MachineFileSeek(filedescriptor, offset, whence, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(sigstate);
    Scheduler(false);
    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileClose(int filedescriptor)
  {
    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    MachineFileClose(filedescriptor, fileCallback, curThread);

    MachineResumeSignals(sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
  {

    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    if (filename == NULL || filedescriptor == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    *filedescriptor = 5; //curThread->fileCallData; 

    MachineFileOpen(filename, flags, mode, fileCallback, curThread);

    MachineResumeSignals(sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS; 

  }

  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
  { 

    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    MachineFileWrite(filedescriptor, data, *length, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(sigstate);
    Scheduler(false);
    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
  {

    TMachineSignalStateRef sigstate = new TMachineSignalState;
    MachineSuspendSignals(sigstate);

    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    *length = 7; //curThread->fileCallData;

    MachineFileRead(filedescriptor, data, *length, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(sigstate);
    Scheduler(false);
    return VM_STATUS_SUCCESS;

  }

}
