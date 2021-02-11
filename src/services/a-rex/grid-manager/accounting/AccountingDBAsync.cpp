#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <arc/Utils.h>

#include "AccountingDBAsync.h"

namespace ARex {
  class AccountingDBThread: public Arc::Thread {
   friend class AccountingDBAsync;
   public:
    static const std::size_t MaxQueueDepth = 10000;

    static AccountingDBThread& Instance();
    bool Push(AccountingDBAsync::Event* event);

   private:
    AccountingDBThread();
    virtual ~AccountingDBThread();
    void thread();

    Arc::SimpleCondition lock_;
    AccountingDBThread* instance_;

    std::map< std::string,Arc::AutoPointer<AccountingDB> > dbs_;
    std::list<AccountingDBAsync::Event*> queue_; // this queue is emptied in destructor
    bool exited_;
  };

  AccountingDBThread& AccountingDBThread::Instance() {
    static AccountingDBThread instance;
    return instance;
  }

  AccountingDBThread::AccountingDBThread():exited_(false) {
    start();
  }

  AccountingDBThread::~AccountingDBThread() {
    Push(new AccountingDBAsync::EventQuit());
    while(!exited_) sleep(1);
    Arc::AutoLock<Arc::SimpleCondition> lock(lock_);
    while(!queue_.empty()) { delete queue_.front(); queue_.pop_front(); }
  }

  bool AccountingDBThread::Push(AccountingDBAsync::Event* event) {
    Arc::AutoLock<Arc::SimpleCondition> lock(lock_);
    while(queue_.size() >= MaxQueueDepth) {
      lock.unlock();
      sleep(1);  // TODO: something clever needed
      lock.lock();
    };
    queue_.push_back(event);
    lock_.signal_nonblock();
    return true;
  }

  void AccountingDBThread::thread() {
    while(true) {
      Arc::AutoLock<Arc::SimpleCondition> lock(lock_);
      if(queue_.empty()) {
        lock_.wait_nonblock();
        if(queue_.empty()) continue;
      }
      Arc::AutoPointer<AccountingDBAsync::Event> event(queue_.front());
      queue_.pop_front();
      AccountingDBAsync::EventQuit* eventQuit = dynamic_cast<AccountingDBAsync::EventQuit*>(event.Ptr());
      if(eventQuit) break;
      std::map< std::string,Arc::AutoPointer<AccountingDB> >::iterator db = dbs_.find(event->name);
      if(db == dbs_.end()) continue; // not expected
      lock.unlock(); // no need to keep lock anymore - db and event are picked up

      AccountingDBAsync::EventCreateAAR* eventCreateAAR = dynamic_cast<AccountingDBAsync::EventCreateAAR*>(event.Ptr());
      if(eventCreateAAR) {
        db->second->createAAR(eventCreateAAR->aar);
        continue;
      };
      AccountingDBAsync::EventUpdateAAR* eventUpdateAAR = dynamic_cast<AccountingDBAsync::EventUpdateAAR*>(event.Ptr());
      if(eventUpdateAAR) {
        db->second->updateAAR(eventUpdateAAR->aar);
        continue;
      };
      AccountingDBAsync::EventAddJobEvent* eventAddJobEvent = dynamic_cast<AccountingDBAsync::EventAddJobEvent*>(event.Ptr());
      if(eventAddJobEvent) {
        db->second->addJobEvent(eventAddJobEvent->events, eventAddJobEvent->jobid);
        continue;
      };
    };
  }



  AccountingDBAsync::AccountingDBAsync(const std::string& name, AccountingDB* (*ctr)(const std::string&)) : AccountingDB(name) {
    AccountingDBThread& thread(AccountingDBThread::Instance());
    Arc::AutoLock<Arc::SimpleCondition> lock(thread.lock_);
    std::map< std::string,Arc::AutoPointer<AccountingDB> >::iterator dbIt = thread.dbs_.find(name);
    if(dbIt == thread.dbs_.end()) {
      AccountingDB* db = ctr(name);
      if(!db || !db->IsValid()) {
        delete db;
        return;
      }
      thread.dbs_[name] = db;
    }
    isValid = true;
  }

  AccountingDBAsync::~AccountingDBAsync() {
  }

   bool AccountingDBAsync::createAAR(AAR& aar) {
     return AccountingDBThread::Instance().Push(new EventCreateAAR(name, aar));
   }

   bool AccountingDBAsync::updateAAR(AAR& aar) {
     return AccountingDBThread::Instance().Push(new EventUpdateAAR(name, aar));
   }

   bool AccountingDBAsync::addJobEvent(aar_jobevent_t& events, const std::string& jobid) {
     return AccountingDBThread::Instance().Push(new EventAddJobEvent(name, events, jobid));
   }

   AccountingDBAsync::Event::Event(std::string const& name): name(name) {
   }

   AccountingDBAsync::Event::~Event() {
   }

   AccountingDBAsync::EventCreateAAR::EventCreateAAR(std::string const& name, AAR const& aar): Event(name), aar(aar) {
   }

   AccountingDBAsync::EventUpdateAAR::EventUpdateAAR(std::string const& name, AAR const& aar): Event(name), aar(aar) {
   }

   AccountingDBAsync::EventAddJobEvent::EventAddJobEvent(std::string const& name, aar_jobevent_t const& events, std::string const& jobid):
            Event(name), events(events), jobid(jobid) {
   }

   AccountingDBAsync::EventQuit::EventQuit(): Event("") {
   }

}

