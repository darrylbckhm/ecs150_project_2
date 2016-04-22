#include <iostream>
#include <unistd.h>
#include <vector>

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {

  using namespace std;

  class TCB {

    public: 

      unsigned int TVMMemorySize, *TVMMemorySizeRef;
      unsigned int TVMStatus, *TVMStatusRef;
      unsigned int TVMTick, *TVMTickRef;
      unsigned int TVMThreadID, *TVMThreadIDRef;
      unsigned int TVMMutexID, *TVMMutexIDRef;
      unsigned int TVMThreadPriority, *TVMThreadPriorityRef;
      unsigned int TVMThreadState, *TVMThreadStateRef;

      void (*TVMMainEntry)(int, char*[]);
      void (*TVMThreadEntry)(void *);

      void AlarmCall(void *param);

      TMachineSignalStateRef sigstate;

  };


  volatile unsigned int counter = 0;
  static vector<TCB*> threads;


  TVMMainEntry VMLoadModule(const char *module);

  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {

    TCB *thread = new TCB;
    thread->TVMMemorySize = memsize;
    //thread->TVMStatus;
    //thread->TVMTick;
    thread->TVMThreadID = ((TVMThreadID)0);
    thread->TVMMutexID = 0;
    thread->TVMThreadPriority = prio;
    thread->TVMThreadState = VM_THREAD_STATE_DEAD;

    threads.push_back(thread); 

    TCB *thread2 = threads[0];
    cout << endl << "threadID: " << thread2->TVMThreadIDRef << endl;
    tid = threads[0]->TVMThreadIDRef;


    //MachineSuspendSignals(sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {
    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
  {
    cout << endl << "thread in VMThreadState: " << (unsigned int)thread << endl;
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
    VMPrint(s.c_str());
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
