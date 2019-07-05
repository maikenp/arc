#ifndef ARC_ACCOUNTING_DB_H
#define ARC_ACCOUNTING_DB_H

#include <string>

#include "AARContent.h"

namespace ARex {
    class GMJob;
    class GMConfig;

    /// Abstract class for storing A-REX accounting records (AAR)
    /**
     * This abstract class provides an interface which can be used to store
     * AAR information in the database
     *
     * \note This class is abstract. All functionality is provided by specialised
     * child classes.
     **/
    class AccountingDB {
    public:
        AccountingDB(const std::string& name) : name(name), isValid(false) {}
        virtual ~AccountingDB() {}

        /// Check if database connection is successfull
        /**
         * @return true if database connection successfull
         **/
        bool IsValid() const { return isValid; }
        /// Write new AAR into the database
        virtual bool writeAAR(const AAR& aar) = 0;
        /// Update existing AAR in the database (only dynamic parameters)
        virtual bool updateAAR(const AAR& aar) = 0;
        /// Add job even record to existing AAR
        virtual bool addJobEvent(aar_jobevent_t& events, const std::string& jobid) = 0;
    protected:
        const std::string name;
        bool isValid;
    };
}

#endif
