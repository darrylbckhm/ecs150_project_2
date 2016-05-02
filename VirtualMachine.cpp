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


  };

  class Mutex {
    public:
      TVMMutexID mutexID;
      TCB *owner;
      unsigned int locked;
      queue<TCB *> highWaitingQueue;
      queue<TCB *> normalWaitingQueue;
      queue<TCB *> lowWaitingQueue;
  };

  // keep track of total ticks
  volatile unsigned int ticksElapsed;

  volatile unsigned int glbl_tickms;

  // vector of all threads
  static vector<TCB*> threads;
  static vector<Mutex *> mutexes;

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
    cout << "ticksElapsed: " << ticksElapsed << endl;
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

    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      Mutex *mutex = *itr;
      cout << "mutexID: " << mutex->mutexID;
      cout << ", locked: " << mutex->locked;
      TCB *thread = mutex->owner;
      if (thread)
        cout << ", owner: " << thread->threadID;
      else
        cout << ", owner: no owner";
      cout << endl;
    }

    cout << endl;
  }

  void skeleton(void *param)
  {
    //cout << "a" << endl;
    //printThreadInfo();
    TCB *thread = (TCB *)param;

    MachineEnableSignals();
    thread->entry(thread->param);

    VMThreadTerminate(curThread->threadID);

  }

  void Scheduler(bool activate)
  {
    //cout << "entering scheduler" << endl;

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

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

    if (curThread->threadID == prevThread->threadID)
    {
      //cout << "prevThread: " << prevThread->threadID << endl;
      //cout << "curThread (nextThread): " << curThread->threadID << endl;
      if ((threads.size() == 2) && (threads[0]->state == VM_THREAD_STATE_RUNNING))
      {
        //cout << "b" << endl;
        curThread = threads[0];
        return;
      }
      curThread = threads[1];
    }

    curThread->addedToQueue = 0;

    if (activate && (curThread->priority <= prevThread->priority))
    {
      curThread = prevThread;
      MachineResumeSignals(&sigstate);
      return;
    }

    if ((curThread->priority < prevThread->priority) && (prevThread->state == VM_THREAD_STATE_RUNNING))
    {
      curThread = prevThread;
      MachineResumeSignals(&sigstate);
      return;
    }

    if (prevThread->threadID == curThread->threadID)
    {
      MachineResumeSignals(&sigstate);
      return;
    }

    if (prevThread->state == VM_THREAD_STATE_RUNNING)
      prevThread->state = VM_THREAD_STATE_READY;

    //cout << "prevThread: " << prevThread->threadID << endl;
    //cout << "curThread (nextThread): " << curThread->threadID << endl;

    curThread->state = VM_THREAD_STATE_RUNNING; 
    //printThreadInfo();
    //cout << endl << "context save" << endl;
    MachineResumeSignals(&sigstate);
    //if (MachineContextSave(&prevThread->mcntx) == 0)
    //{
    //cout << "context restore" << endl;
    //cerr << static_cast<void *>(&curThread->mcntx) << endl;
    // MachineContextRestore(&curThread->mcntx);
    //cout << "a" << endl;
    //}
    MachineContextSwitch(&prevThread->mcntx, &curThread->mcntx);
    //cout << "done" << endl << endl;

    MachineResumeSignals(&sigstate);

  }

  void idle(void *param)
  {
    MachineEnableSignals();

    while(1)
    {

      ;

    }

  }

  TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    Mutex *mutex = new Mutex;
    mutex->locked = 0;
    mutex->mutexID = mutexes.size();

    mutexes.push_back(mutex);

    *mutexref = mutex->mutexID;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      if ((*itr)->mutexID == mutex)
      {
        if ((*itr)->locked == 0)
        {
          //cout << "unlocked" << endl;
          (*itr)->locked = 1;
          (*itr)->owner = curThread;
        }
        else
        {
          //cout << "mutexID: " << mutex << endl;
          //cout << "already locked" << endl;
          curThread->state = VM_THREAD_STATE_WAITING;

          if(curThread->priority == VM_THREAD_PRIORITY_HIGH)
          {

            (*itr)->highWaitingQueue.push(curThread);

          }

          else if(curThread->priority == VM_THREAD_PRIORITY_NORMAL)
          {

            (*itr)->normalWaitingQueue.push(curThread);

          }

          else if(curThread->priority == VM_THREAD_PRIORITY_LOW)
          {

            (*itr)->lowWaitingQueue.push(curThread);

          }

          MachineResumeSignals(&sigstate);
          Scheduler(false);

        }
      }
    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexRelease(TVMMutexID mutex)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      if ((*itr)->mutexID == mutex)
      {
        (*itr)->locked = 0;
        TCB *newOwner = NULL;
        if ((*itr)->highWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->highWaitingQueue.front();
          (*itr)->highWaitingQueue.pop();
        }
        else if ((*itr)->normalWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->normalWaitingQueue.front();
          (*itr)->normalWaitingQueue.pop();
        }
        else if ((*itr)->lowWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->lowWaitingQueue.front();
          (*itr)->lowWaitingQueue.pop();
        }

        if (newOwner != NULL)
        {
          (*itr)->locked = 1;
          (*itr)->owner = newOwner;
          newOwner->state = VM_THREAD_STATE_READY;
          MachineResumeSignals(&sigstate);
          Scheduler(false);
        }
      }
    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }


  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    if(entry == NULL || tid == NULL)
    {
      MachineResumeSignals(&sigstate);
      return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    TVMThreadID id = threads.size();

    TCB *thread = new TCB;
    thread->memsize = memsize;
    thread->status = VM_STATUS_SUCCESS;
    //thread->tick;
    thread->sleep = 0;
    thread->threadID = id;
    thread->mutexID = -1;
    thread->priority = prio;
    thread->state = VM_THREAD_STATE_DEAD;
    thread->entry =  entry;
    thread->param = param;

    threads.push_back(thread);

    *tid = id;
    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
  {

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMutexDelete(TVMMutexID mutex)
  {

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadDelete(TVMThreadID thread)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if(thread == curThread->threadID)
      {

        threads.erase(itr);

      }

      else
      {

        MachineResumeSignals(&sigstate);
        return VM_STATUS_ERROR_INVALID_ID;

      }

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadID(TVMThreadIDRef threadref)
  {

    if(threadref == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;
    else
      *threadref = curThread->threadID;

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

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

    //cout << "done activating before scheduling" << endl;

    MachineResumeSignals(&sigstate);
    Scheduler(true);
    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadTerminate(TVMThreadID thread)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

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
          MachineResumeSignals(&sigstate);
          Scheduler(false);
          return VM_STATUS_SUCCESS;

        }

      }

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_FAILURE;

  }

  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;

    if(stateref == NULL)
    {
      MachineResumeSignals(&sigstate);
      return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if ((*itr)->threadID == thread)
      {
        *stateref = (*itr)->state;
        found = 1;
      }

    }

    if(!found)
    {

      MachineResumeSignals(&sigstate);
      return VM_STATUS_ERROR_INVALID_ID;

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_RUNNING)
      {
        (*itr)->sleepCount = tick;
        (*itr)->state = VM_THREAD_STATE_WAITING;
        (*itr)->sleep = 1;
        //printThreadInfo();
        MachineResumeSignals(&sigstate);
        Scheduler(false);
      }
    }

    return VM_STATUS_SUCCESS;

  }

  void AlarmCall(void *param)
  {
    //cout << "a" << endl;
    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);
    ticksElapsed = ticksElapsed + 1;
    //printThreadInfo();

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      //cout << "c" << endl;
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
    //printThreadInfo();

    MachineResumeSignals(&sigstate);
    MachineEnableSignals();

    Scheduler(false);
    MachineResumeSignals(&sigstate);

  }

  TVMStatus VMTickMS(int *tickmsref)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    *tickmsref = glbl_tickms;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMTickCount(TVMTickRef tickref)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if(tickref == NULL)
    {

      MachineResumeSignals(&sigstate);    
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    *tickref = ticksElapsed; 
    //cout << "ticksElapsed: " << ticksElapsed << endl;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }



  TVMStatus VMStart(int tickms, int argc, char *argv[])
  {

    glbl_tickms = tickms;

    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    if(main_entry != NULL)
    {

      MachineInitialize();
      MachineRequestAlarm(tickms*1000, AlarmCall, NULL);
      MachineEnableSignals();   

      TVMThreadID VMThreadID;
      VMThreadCreate(skeleton, NULL, 0x100000, VM_THREAD_PRIORITY_NORMAL, &VMThreadID);
      threads[0]->state = VM_THREAD_STATE_RUNNING;
      curThread = threads[0];

      VMThreadCreate(idle, NULL, 0x100000, VM_THREAD_PRIORITY_IDLE, &VMThreadID);
      VMThreadActivate(VMThreadID);

      //printThreadInfo();

      main_entry(argc, argv);

    }

    else
    {

      return VM_STATUS_FAILURE;

    }

    return VM_STATUS_SUCCESS;

  }

  void fileCallback(void *calldata, int result)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    curThread->fileCallFlag = 1;

    TCB* thread = (TCB*)calldata;

    thread->fileCallData = result;

    thread->state = VM_THREAD_STATE_READY;

    //cout << "FileCallback Result: " << thread->fileCallData << endl;

    curThread->fileCallFlag = 0;

    MachineResumeSignals(&sigstate);

    Scheduler(false);    

  }

  TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if (newoffset != NULL)
      *newoffset = offset;

    else
      return VM_STATUS_FAILURE;

    MachineFileSeek(filedescriptor, offset, whence, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(&sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileClose(int filedescriptor)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    MachineFileClose(filedescriptor, fileCallback, curThread);

    MachineResumeSignals(&sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if (filename == NULL || filedescriptor == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    MachineFileOpen(filename, flags, mode, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    *filedescriptor = curThread->fileCallData; 

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS; 

  }

  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
  { 

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    MachineFileWrite(filedescriptor, data, *length, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(&sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    MachineFileRead(filedescriptor, data, *length, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    *length = curThread->fileCallData;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

}
