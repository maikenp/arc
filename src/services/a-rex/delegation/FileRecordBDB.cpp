#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <glibmm.h>

#include <arc/FileUtils.h>
#include <arc/Logger.h>

#include "uid.h"

#include "FileRecordBDB.h"

namespace ARex {

  #define FR_DB_NAME "list"

  static void db_env_clean(const std::string& base) {
    try {
      Glib::Dir dir(base);
      std::string name;
      while ((name = dir.read_name()) != "") {
        std::string fullpath(base);
        fullpath += G_DIR_SEPARATOR_S + name;
        struct stat st;
        if (::lstat(fullpath.c_str(), &st) == 0) {
          if(!S_ISDIR(st.st_mode)) {
            if(name != FR_DB_NAME) {
              Arc::FileDelete(fullpath.c_str());
            };
          };
        };
      };
    } catch(Glib::FileError& e) {
    };
  }

  bool FileRecordBDB::dberr(const char* s, int err) {
    if(err == 0) return true;
    error_num_ = err;
    error_str_ = std::string(s)+": "+DbEnv::strerror(err);
    return false;
  }

  FileRecordBDB::FileRecordBDB(const std::string& base, bool create):
      FileRecord(base, create),
      db_rec_(NULL),
      db_lock_(NULL),
      db_locked_(NULL),
      db_link_(NULL) {
    valid_ = open(create);
  }

  bool FileRecordBDB::verify(void) {
    // Performing various kinds of verifications
    std::string dbpath = basepath_ + G_DIR_SEPARATOR_S + FR_DB_NAME;
    {
      Db db_test(NULL,DB_CXX_NO_EXCEPTIONS);
      if(!dberr("Error verifying databases",
                db_test.verify(dbpath.c_str(),NULL,NULL,DB_NOORDERCHK))) {
        if(error_num_ != ENOENT) return false;
      };
    };
    {
      Db db_test(NULL,DB_CXX_NO_EXCEPTIONS);
      if(!dberr("Error verifying database 'meta'",
                db_test.verify(dbpath.c_str(),"meta",NULL,DB_ORDERCHKONLY))) {
        if(error_num_ != ENOENT) return false;
      };
    };
    // Skip 'link' - it is not of btree kind
    // Skip 'lock' - for unknown reason it returns DB_NOTFOUND
    // Skip 'locked' - for unknown reason it returns DB_NOTFOUND
    return true;
  }

  FileRecordBDB::~FileRecordBDB(void) {
    close();
  }

  bool FileRecordBDB::open(bool create) {
    int oflags = 0;
    int eflags = DB_INIT_CDB | DB_INIT_MPOOL;
    if(create) {
      oflags |= DB_CREATE;
      eflags |= DB_CREATE;
    };
    int mode = S_IRUSR|S_IWUSR;

    db_env_ = new DbEnv(DB_CXX_NO_EXCEPTIONS);
    if(!dberr("Error setting database environment flags",
          db_env_->set_flags(DB_CDB_ALLDB,1))) {
      delete db_env_; db_env_ = NULL;
      return false;
    }

    // If process is master (create = true) we should check by caling
    // failchk() and discard environment in case something is wrong.
    // But sicne we are allowed to discard environment we can do
    // that just in case.
    if(create) {
      db_env_clean(basepath_);
    };
    if(!dberr("Error opening database environment",
          db_env_->open(basepath_.c_str(),eflags,mode))) {
      delete db_env_; db_env_ = NULL;
      return false;
    };

    std::string dbpath = FR_DB_NAME;
    if(create) {
      // If creation is allowed that means we are master and can try verifying
      if(!verify()) return false;
    };
    // db_link
    //    |---db_lock
    //    \---db_locked
    db_rec_ = new Db(db_env_,DB_CXX_NO_EXCEPTIONS);
    db_lock_ = new Db(db_env_,DB_CXX_NO_EXCEPTIONS);
    db_locked_ = new Db(db_env_,DB_CXX_NO_EXCEPTIONS);
    db_link_ = new Db(db_env_,DB_CXX_NO_EXCEPTIONS);
    if(!dberr("Error setting flag DB_DUPSORT",db_lock_->set_flags(DB_DUPSORT))) return false;
    if(!dberr("Error setting flag DB_DUPSORT",db_locked_->set_flags(DB_DUPSORT))) return false;
    if(!dberr("Error associating databases",db_link_->associate(NULL,db_lock_,&lock_callback,0))) return false;
    if(!dberr("Error associating databases",db_link_->associate(NULL,db_locked_,&locked_callback,0))) return false;
    if(!dberr("Error opening database 'meta'",
          db_rec_->open(NULL,dbpath.c_str(),   "meta",  DB_BTREE,oflags,mode))) return false;
    if(!dberr("Error opening database 'link'",
          db_link_->open(NULL,dbpath.c_str(),  "link",  DB_RECNO,oflags,mode))) return false;
    if(!dberr("Error opening database 'lock'",
          db_lock_->open(NULL,dbpath.c_str(),  "lock",  DB_BTREE,oflags,mode))) return false;
    if(!dberr("Error opening database 'locked'",
          db_locked_->open(NULL,dbpath.c_str(),"locked",DB_BTREE,oflags,mode))) return false;
    return true;
  }

  void FileRecordBDB::close(void) {
    valid_ = false;
    if(db_locked_) db_locked_->close(0);
    if(db_lock_) db_lock_->close(0);
    if(db_link_) db_link_->close(0);
    if(db_rec_) db_rec_->close(0);
    if(db_env_) db_env_->close(0);
    delete db_locked_; db_locked_ = NULL;
    delete db_lock_; db_lock_ = NULL;
    delete db_link_; db_link_ = NULL;
    delete db_env_; db_env_ = NULL;
  }

  static void* store_string(const std::string& str, void* buf) {
    uint32_t l = str.length();
    unsigned char* p = (unsigned char*)buf;
    *p = (unsigned char)l; l >>= 8; ++p;
    *p = (unsigned char)l; l >>= 8; ++p;
    *p = (unsigned char)l; l >>= 8; ++p;
    *p = (unsigned char)l; l >>= 8; ++p;
    ::memcpy(p,str.c_str(),str.length());
    p += str.length();
    return (void*)p;
  }

  static void* parse_string(std::string& str, const void* buf, uint32_t& size) {
    uint32_t l = 0;
    const unsigned char* p = (unsigned char*)buf;
    if(size < 4) { p += size; size = 0; return (void*)p; };
    l |= ((uint32_t)(*p)) << 0; ++p; --size;
    l |= ((uint32_t)(*p)) << 8; ++p; --size;
    l |= ((uint32_t)(*p)) << 16; ++p; --size;
    l |= ((uint32_t)(*p)) << 24; ++p; --size;
    if(l > size) l = size;
    // TODO: sanity check
    str.assign((const char*)p,l);
    p += l; size -= l;
    return (void*)p;
  }

  static void make_string(const std::string& str, Dbt& rec) {
    rec.set_data(NULL); rec.set_size(0);
    uint32_t l = 4 + str.length();
    void* d = (void*)::malloc(l);
    if(!d) return;
    rec.set_data(d); rec.set_size(l);
    d = store_string(str,d);
  }

  static void make_link(const std::string& lock_id,const std::string& id, const std::string& owner, Dbt& rec) {
    rec.set_data(NULL); rec.set_size(0);
    uint32_t l = 4 + lock_id.length() + 4 + id.length() + 4 + owner.length();
    void* d = (void*)::malloc(l);
    if(!d) return;
    rec.set_data(d); rec.set_size(l);
    d = store_string(lock_id,d);
    d = store_string(id,d);
    d = store_string(owner,d);
  }

  static void make_key(const std::string& id, const std::string& owner, Dbt& key) {
    key.set_data(NULL); key.set_size(0);
    uint32_t l = 4 + id.length() + 4 + owner.length();
    void* d = (void*)::malloc(l);
    if(!d) return;
    key.set_data(d); key.set_size(l);
    d = store_string(id,d);
    d = store_string(owner,d);
  }
 
  static void make_record(const std::string& uid, const std::string& id, const std::string& owner, const std::list<std::string>& meta, Dbt& key, Dbt& data) {
    key.set_data(NULL); key.set_size(0);
    data.set_data(NULL); data.set_size(0);
    uint32_t l = 4 + uid.length();
    for(std::list<std::string>::const_iterator m = meta.begin(); m != meta.end(); ++m) {
      l += 4 + m->length();
    };
    make_key(id,owner,key);
    void* d = (void*)::malloc(l);
    if(!d) {
      ::free(key.get_data());
      key.set_data(NULL); key.set_size(0);
      return;
    };
    data.set_data(d); data.set_size(l);
    d = store_string(uid,d);
    for(std::list<std::string>::const_iterator m = meta.begin(); m != meta.end(); ++m) {
      d = store_string(*m,d);
    };
  }
 
  static void parse_record(std::string& uid, std::string& id, std::string& owner, std::list<std::string>& meta, const Dbt& key, const Dbt& data) {
    uint32_t size = 0;
    void* d = NULL;

    d = (void*)key.get_data();
    size = (uint32_t)key.get_size();
    d = parse_string(id,d,size);
    d = parse_string(owner,d,size);

    d = (void*)data.get_data();
    size = (uint32_t)data.get_size();
    d = parse_string(uid,d,size);
    for(;size;) {
      std::string s;
      d = parse_string(s,d,size);
      meta.push_back(s);
    };
  }

  int FileRecordBDB::locked_callback(Db * secondary, const Dbt * key, const Dbt * data, Dbt * result) {
    const void* p = data->get_data();
    uint32_t size = data->get_size();
    std::string str;
    p = parse_string(str,p,size);
    result->set_data((void*)p);
    result->set_size(size);
    return 0;
  }

  bool FileRecordBDB::Recover(void) {
    Glib::Mutex::Lock lock(lock_);
    // Real recovery not implemented yet.
    close();
    error_num_ = -1;
    error_str_ = "Recovery not implemented yet.";
    return false;
  }

  int FileRecordBDB::lock_callback(Db * secondary, const Dbt * key, const Dbt * data, Dbt * result) {
    const void* p = data->get_data();
    uint32_t size = data->get_size();
    uint32_t rest = size;
    std::string str;
    parse_string(str,p,rest);
    result->set_data((void*)p);
    result->set_size(size-rest);
    return 0;
  }

  std::string FileRecordBDB::Add(std::string& id, const std::string& owner, const std::list<std::string>& meta) {
    if(!valid_) return "";
    int uidtries = 10; // some sane number
    std::string uid;
    while(true) {
      if(!(uidtries--)) return "";
      Glib::Mutex::Lock lock(lock_);
      Dbt key;
      Dbt data;
      uid = rand_uid64().substr(4);
      make_record(uid,(id.empty())?uid:id,owner,meta,key,data);
      void* pkey = key.get_data();
      void* pdata = data.get_data();
      int dbres = db_rec_->put(NULL,&key,&data,DB_NOOVERWRITE);
      if(dbres == DB_KEYEXIST) {
        ::free(pkey); ::free(pdata);
        uid.resize(0);
        continue;
      };
      if(!dberr("Failed to add record to database",dbres)) {
        ::free(pkey); ::free(pdata);
        return "";
      };
      db_rec_->sync(0);
      ::free(pkey); ::free(pdata);
      break;
    };
    if(id.empty()) id = uid;
    make_file(uid);
    return uid_to_path(uid);
  }

  bool FileRecordBDB::Add(const std::string& uid, const std::string& id, const std::string& owner, const std::list<std::string>& meta) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbt key;
    Dbt data;
    make_record(uid,(id.empty())?uid:id,owner,meta,key,data);
    void* pkey = key.get_data();
    void* pdata = data.get_data();
    int dbres = db_rec_->put(NULL,&key,&data,DB_NOOVERWRITE);
    if(!dberr("Failed to add record to database",dbres)) {
      ::free(pkey); ::free(pdata);
      return false;
    };
    db_rec_->sync(0);
    ::free(pkey); ::free(pdata);
    return true;
  }

  std::string FileRecordBDB::Find(const std::string& id, const std::string& owner, std::list<std::string>& meta) {
    if(!valid_) return "";
    Glib::Mutex::Lock lock(lock_);
    Dbt key;
    Dbt data;
    make_key(id,owner,key);
    void* pkey = key.get_data();
    if(!dberr("Failed to retrieve record from database",db_rec_->get(NULL,&key,&data,0))) {
      ::free(pkey);
      return "";
    };
    std::string uid;
    std::string id_tmp;
    std::string owner_tmp;
    parse_record(uid,id_tmp,owner_tmp,meta,key,data);
    ::free(pkey);
    return uid_to_path(uid);
  }

  bool FileRecordBDB::Modify(const std::string& id, const std::string& owner, const std::list<std::string>& meta) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbt key;
    Dbt data;
    make_key(id,owner,key);
    void* pkey = key.get_data();
    if(!dberr("Failed to retrieve record from database",db_rec_->get(NULL,&key,&data,0))) {
      ::free(pkey);
      return false;
    };
    std::string uid;
    std::string id_tmp;
    std::string owner_tmp;
    std::list<std::string> meta_tmp;
    parse_record(uid,id_tmp,owner_tmp,meta_tmp,key,data);
    ::free(pkey);
    make_record(uid,id,owner,meta,key,data);
    if(!dberr("Failed to store record to database",db_rec_->put(NULL,&key,&data,0))) {
      ::free(key.get_data());
      ::free(data.get_data());
      return false;
    };
    db_rec_->sync(0);
    ::free(key.get_data());
    ::free(data.get_data());
    return true;
  }

  bool FileRecordBDB::Remove(const std::string& id, const std::string& owner) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbt key;
    Dbt data;
    make_key(id,owner,key);
    void* pkey = key.get_data();
    if(dberr("",db_locked_->get(NULL,&key,&data,0))) {
      ::free(pkey);
      error_str_ = "Record has active locks";
      return false; // have locks
    };
    if(!dberr("Failed to retrieve record from database",db_rec_->get(NULL,&key,&data,0))) {
      ::free(pkey);
      return false; // No such record?
    };
    std::string uid;
    std::string id_tmp;
    std::string owner_tmp;
    std::list<std::string> meta;
    parse_record(uid,id_tmp,owner_tmp,meta,key,data);
    if(!dberr("Failed to delete record from database",db_rec_->del(NULL,&key,0))) {
      // TODO: handle error
      ::free(pkey);
      return false;
    };
    db_rec_->sync(0);
    ::free(pkey);
    remove_file(uid);
    return true;
  }

  bool FileRecordBDB::AddLock(const std::string& lock_id, const std::list<std::string>& ids, const std::string& owner) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbt key;
    Dbt data;
    for(std::list<std::string>::const_iterator id = ids.begin(); id != ids.end(); ++id) {
      make_link(lock_id,*id,owner,data);
      void* pdata = data.get_data();
      if(!dberr("addlock:put",db_link_->put(NULL,&key,&data,DB_APPEND))) {
        ::free(pdata);
        return false;
      };
      ::free(pdata);
    };
    db_link_->sync(0);
    return true;
  }

  bool FileRecordBDB::RemoveLock(const std::string& lock_id) {
    std::list<std::pair<std::string,std::string> > ids;
    return RemoveLock(lock_id,ids);
  }

  bool FileRecordBDB::RemoveLock(const std::string& lock_id, std::list<std::pair<std::string,std::string> >& ids) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbc* cur = NULL;
    if(!dberr("removelock:cursor",db_lock_->cursor(NULL,&cur,DB_WRITECURSOR))) return false;
    Dbt key;
    Dbt data;
    make_string(lock_id,key);
    void* pkey = key.get_data();
    if(!dberr("removelock:get1",cur->get(&key,&data,DB_SET))) { // TODO: handle errors
      ::free(pkey);
      cur->close(); return false;
    };
    for(;;) {
      std::string id;
      std::string owner;
      uint32_t size = data.get_size();
      void* buf = data.get_data();
      buf = parse_string(id,buf,size); //  lock_id - skip
      buf = parse_string(id,buf,size);
      buf = parse_string(owner,buf,size);
      ids.push_back(std::pair<std::string,std::string>(id,owner));
      if(!dberr("removelock:del",cur->del(0))) {
        ::free(pkey);
        cur->close(); return false;
      };
      if(!dberr("removelock:get2",cur->get(&key,&data,DB_NEXT_DUP))) break;
    };
    db_lock_->sync(0);
    ::free(pkey);
    cur->close();
    return true;
  }


  bool FileRecordBDB::ListLocked(const std::string& lock_id, std::list<std::pair<std::string,std::string> >& ids) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbc* cur = NULL;
    if(!dberr("listlocked:cursor",db_lock_->cursor(NULL,&cur,0))) return false;
    Dbt key;
    Dbt data;
    make_string(lock_id,key);
    void* pkey = key.get_data();
    if(!dberr("listlocked:get1",cur->get(&key,&data,DB_SET))) { // TODO: handle errors
      ::free(pkey);
      cur->close(); return false;
    };
    for(;;) {
      std::string id;
      std::string owner;
      uint32_t size = data.get_size();
      void* buf = data.get_data();
      buf = parse_string(id,buf,size); //  lock_id - skip
      buf = parse_string(id,buf,size);
      buf = parse_string(owner,buf,size);
      ids.push_back(std::pair<std::string,std::string>(id,owner));
      if(cur->get(&key,&data,DB_NEXT_DUP) != 0) break;
    };
    ::free(pkey);
    cur->close();
    return true;
  }

  bool FileRecordBDB::ListLocks(std::list<std::string>& locks) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbc* cur = NULL;
    if(db_lock_->cursor(NULL,&cur,0)) return false;
    for(;;) {
      Dbt key;
      Dbt data;
      if(cur->get(&key,&data,DB_NEXT_NODUP) != 0) break; // TODO: handle errors
      std::string str;
      uint32_t size = key.get_size();
      parse_string(str,key.get_data(),size);
      locks.push_back(str);
    };
    cur->close();
    return true;
  }

  bool FileRecordBDB::ListLocks(const std::string& id, const std::string& owner, std::list<std::string>& locks) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    Dbc* cur = NULL;
    if(db_lock_->cursor(NULL,&cur,0)) return false;
    for(;;) {
      Dbt key;
      Dbt data;
      if(cur->get(&key,&data,DB_NEXT_NODUP) != 0) break; // TODO: handle errors
      std::string str;
      uint32_t size = key.get_size();
      parse_string(str,key.get_data(),size);
      {
        std::string id_tmp;
        std::string owner_tmp;
        uint32_t size = data.get_size();
        void* buf = data.get_data();
        buf = parse_string(id_tmp,buf,size); //  lock_id - skip
        buf = parse_string(id_tmp,buf,size);
        buf = parse_string(owner_tmp,buf,size);
        if((id_tmp != id) || (owner_tmp != owner)) continue;
      };
      locks.push_back(str);
    };
    cur->close();
    return true;
  }

  FileRecordBDB::Iterator::Iterator(FileRecordBDB& frec):FileRecord::Iterator(frec),cur_(NULL) {
    Glib::Mutex::Lock lock(frec.lock_);
    if(!frec.dberr("Iterator:cursor",frec.db_rec_->cursor(NULL,&cur_,0))) {
      if(cur_) {
        cur_->close(); cur_=NULL;
      };
      return;
    };
    Dbt key;
    Dbt data;
    if(!frec.dberr("Iterator:first",cur_->get(&key,&data,DB_FIRST))) {
      cur_->close(); cur_=NULL;
      return;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
  }

  FileRecordBDB::Iterator::~Iterator(void) {
    FileRecordBDB& frec((FileRecordBDB&)frec_);
    Glib::Mutex::Lock lock(frec.lock_);
    if(cur_) {
      cur_->close(); cur_=NULL;
    };
  }

  FileRecordBDB::Iterator& FileRecordBDB::Iterator::operator++(void) {
    if(!cur_) return *this;
    FileRecordBDB& frec((FileRecordBDB&)frec_);
    Glib::Mutex::Lock lock(frec.lock_);
    Dbt key;
    Dbt data;
    if(!frec.dberr("Iterator:first",cur_->get(&key,&data,DB_NEXT))) {
      cur_->close(); cur_=NULL;
      return *this;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
    return *this;
  }

  FileRecordBDB::Iterator& FileRecordBDB::Iterator::operator--(void) {
    if(!cur_) return *this;
    FileRecordBDB& frec((FileRecordBDB&)frec_);
    Glib::Mutex::Lock lock(frec.lock_);
    Dbt key;
    Dbt data;
    if(!frec.dberr("Iterator:first",cur_->get(&key,&data,DB_PREV))) {
      cur_->close(); cur_=NULL;
      return *this;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
    return *this;
  }

  void FileRecordBDB::Iterator::suspend(void) {
    FileRecordBDB& frec((FileRecordBDB&)frec_);
    Glib::Mutex::Lock lock(frec.lock_);
    if(cur_) {
      cur_->close(); cur_=NULL;
    }
  }

  bool FileRecordBDB::Iterator::resume(void) {
    FileRecordBDB& frec((FileRecordBDB&)frec_);
    Glib::Mutex::Lock lock(frec.lock_);
    if(!cur_) {
      if(id_.empty()) return false;
      if(!frec.dberr("Iterator:cursor",frec.db_rec_->cursor(NULL,&cur_,0))) {
        if(cur_) {
          cur_->close(); cur_=NULL;
        };
        return false;
      };
      Dbt key;
      Dbt data;
      make_key(id_,owner_,key);
      void* pkey = key.get_data();
      if(!frec.dberr("Iterator:first",cur_->get(&key,&data,DB_SET))) {
        ::free(pkey);
        cur_->close(); cur_=NULL;
        return false;
      };
      parse_record(uid_,id_,owner_,meta_,key,data);
      ::free(pkey);
    };
    return true;
  }

} // namespace ARex

