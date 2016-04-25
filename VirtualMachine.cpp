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

      SMachineContext mcntx;
      SMachineContextRef mcntxref;

      void (*TVMMainEntry)(int, char*[]);
      void (*TVMThreadEntry)(void *);

      void AlarmCall(void *param);

      TMachineSignalStateRef sigstate;

  };


  volatile unsigned int counter = 0;
  static vector<TCB*> threads;
  static queue<TCB*> prioQueue;
  static TCB *curThread;
  volatile static SMachineContext mcntx;

  void skeleton(void *param)
  {

    VMThreadTerminate(curThread->threadID);

  }

  void Scheduler()
  {

    //status = VM_THREAD_STATE_WAITING; 

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
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


    }

  }

/*
  TVMStatus VMFileOpen(int filedescriptor)
  {

    

  }

  TVMStatus VMFileClose(int filedescriptor)
  {

    

  }
*/

  TVMMainEntry VMLoadModule(const char *module);

  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {

    TVMThreadID id = 0;

    TCB *thread = new TCB;
    thread->memsize = memsize;
    thread->status = VM_STATUS_SUCCESS;
    //thread->tick;
    thread->threadID = id;
    thread->mutexID = 0;
    thread->priority = prio;
    thread->state = VM_THREAD_STATE_DEAD;

    threads.push_back(thread);

    curThread = thread;

    *tid = id;

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {

    int8_t *stack = new int8_t[curThread->memsize];
    size_t stacksize = sizeof(stack);
    SMachineContextRef mcntxref = new SMachineContext;

    MachineContextCreate(mcntxref, skeleton, threads.back(), stack, stacksize);

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {
        (*itr)->state = VM_THREAD_STATE_READY;
      }
    }
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

  }

  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
  {
    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {
        *stateref = (*itr)->state;
      }
    }

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {

    counter = tick;

    while(counter)
    {

      //cout << counter << "\n";
      //cout << "Sleeping\n"; 

    }

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMTickMS(int *tickmsref)
  {
    return VM_STATUS_SUCCESS;

  }

  void AlarmCall(void *param)
  {

    if(counter > 0)
    {

      --counter;

    } 

  }



  TVMStatus VMStart(int tickms, int argc, char *argv[])
  {
    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    MachineInitialize();
    MachineRequestAlarm(tickms*1000, AlarmCall, NULL);
    MachineEnableSignals();   

    string s = "module: " + module_name + "\n";
    VMFilePrint(1, s.c_str());

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
