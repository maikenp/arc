// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATASTATUS_H__
#define __ARC_DATASTATUS_H__

#include <iostream>
#include <string>
#include <errno.h>

#include <arc/StringConv.h>
#include <arc/Utils.h>

namespace Arc {
  
#define DataStatusRetryableBase (100)

#define DataStatusErrnoBase 1000
#define EARCTRANSFERTIMEOUT  (DataStatusErrnoBase + 1) // Transfer timed out
#define EARCCHECKSUM         (DataStatusErrnoBase + 2) // Checksum mismatch
#define EARCLOGIC            (DataStatusErrnoBase + 3) // Bad logic, eg calling StartWriting on a
                                                       // DataPoint currently reading
#define EARCRESINVAL         (DataStatusErrnoBase + 4) // All results obtained from a service are invalid
#define EARCSVCTMP           (DataStatusErrnoBase + 5) // Temporary service error
#define EARCSVCPERM          (DataStatusErrnoBase + 6) // Permanent service error
#define EARCOTHER            (DataStatusErrnoBase + 7) // Other / unknown error

#define DataStatusErrnoMax EARCOTHER

  /// Status code returned by many DataPoint methods.
  /**
   * A class to be used for return types of all major data handling methods.
   * It describes the outcome of the method and contains three fields:
   * DataStatusType describes in which operation the error occurred, Errno
   * describes why the error occurred and desc gives more detail if available.
   * Errno is an integer corresponding to error codes defined in errno.h plus
   * additional ARC-specific error codes defined here.
   */
  class DataStatus {

  public:

    /// Status codes
    /** These codes describe in which operation an error occurred. Retryable
     * error codes are deprecated - the corresponding non-retryable error code
     * should be used with errno set to a retryable value. */
    enum DataStatusType {
      // Order is important! Must be kept synchronised with status_string[]

      /// Operation completed successfully
      Success,

      /// Source is bad URL or can't be used due to some reason
      ReadAcquireError,

      /// Destination is bad URL or can't be used due to some reason
      WriteAcquireError,

      /// Resolving of index service URL for source failed
      ReadResolveError,

      /// Resolving of index service URL for destination failed
      WriteResolveError,

      /// Can't read from source
      ReadStartError,

      /// Can't write to destination
      WriteStartError,

      /// Failed while reading from source
      ReadError,

      /// Failed while writing to destination
      WriteError,

      /// Failed while transfering data (mostly timeout)
      TransferError,

      /// Failed while finishing reading from source
      ReadStopError,

      /// Failed while finishing writing to destination
      WriteStopError,

      /// First stage of registration of index service URL failed
      PreRegisterError,

      /// Last stage of registration of index service URL failed
      PostRegisterError,

      /// Unregistration of index service URL failed
      UnregisterError,

      /// Error in caching procedure
      CacheError,

      /// Error due to provided credentials are expired
      CredentialsExpiredError,

      /// Error deleting location or URL
      DeleteError,

      /// No valid location available
      NoLocationError,

      /// No valid location available
      LocationAlreadyExistsError,

      /// Operation has no sense for this kind of URL
      NotSupportedForDirectDataPointsError,

      /// Feature is unimplemented
      UnimplementedError,

      /// DataPoint is already reading
      IsReadingError,

      /// DataPoint is already writing
      IsWritingError,

      /// Access check failed
      CheckError,

      /// Directory listing failed
      ListError,
      /// @deprecated ListError with errno set to ENOTDIR should be used instead
      ListNonDirError,

      /// File/dir stating failed
      StatError,
      /// @deprecated StatError with errno set to ENOENT should be used instead
      StatNotPresentError,

      /// Object initialization failed
      NotInitializedError,

      /// Error in OS
      SystemError,
    
      /// Staging error
      StageError,
      
      /// Inconsistent metadata
      InconsistentMetadataError,
 
      /// Can't prepare source
      ReadPrepareError,

      /// Wait for source to be prepared
      ReadPrepareWait,

      /// Can't prepare destination
      WritePrepareError,

      /// Wait for destination to be prepared
      WritePrepareWait,

      /// Can't finish source
      ReadFinishError,

      /// Can't finish destination
      WriteFinishError,

      /// Can't create directory
      CreateDirectoryError,

      /// Can't rename URL
      RenameError,

      /// Data was already cached
      SuccessCached,
      
      /// General error which doesn't fit any other error
      GenericError,

      /// Undefined
      UnknownError,

      // These Retryable error codes are deprecated but kept for backwards
      // compatibility. They will be removed in a future major release.
      // Instead of these codes the corresponding non-retryable code should be
      // used with an errno, and this is used to determine whether the error is
      // retryable.
      ReadAcquireErrorRetryable = DataStatusRetryableBase+ReadAcquireError, ///< @deprecated
      WriteAcquireErrorRetryable = DataStatusRetryableBase+WriteAcquireError, ///< @deprecated
      ReadResolveErrorRetryable = DataStatusRetryableBase+ReadResolveError, ///< @deprecated
      WriteResolveErrorRetryable = DataStatusRetryableBase+WriteResolveError, ///< @deprecated
      ReadStartErrorRetryable = DataStatusRetryableBase+ReadStartError, ///< @deprecated
      WriteStartErrorRetryable = DataStatusRetryableBase+WriteStartError, ///< @deprecated
      ReadErrorRetryable = DataStatusRetryableBase+ReadError, ///< @deprecated
      WriteErrorRetryable = DataStatusRetryableBase+WriteError, ///< @deprecated
      TransferErrorRetryable = DataStatusRetryableBase+TransferError, ///< @deprecated
      ReadStopErrorRetryable = DataStatusRetryableBase+ReadStopError, ///< @deprecated
      WriteStopErrorRetryable = DataStatusRetryableBase+WriteStopError, ///< @deprecated
      PreRegisterErrorRetryable = DataStatusRetryableBase+PreRegisterError, ///< @deprecated
      PostRegisterErrorRetryable = DataStatusRetryableBase+PostRegisterError, ///< @deprecated
      UnregisterErrorRetryable = DataStatusRetryableBase+UnregisterError, ///< @deprecated
      CacheErrorRetryable = DataStatusRetryableBase+CacheError, ///< @deprecated
      DeleteErrorRetryable = DataStatusRetryableBase+DeleteError, ///< @deprecated
      CheckErrorRetryable = DataStatusRetryableBase+CheckError, ///< @deprecated
      ListErrorRetryable = DataStatusRetryableBase+ListError, ///< @deprecated
      StatErrorRetryable = DataStatusRetryableBase+StatError, ///< @deprecated
      StageErrorRetryable = DataStatusRetryableBase+StageError, ///< @deprecated
      ReadPrepareErrorRetryable = DataStatusRetryableBase+ReadPrepareError, ///< @deprecated
      WritePrepareErrorRetryable = DataStatusRetryableBase+WritePrepareError, ///< @deprecated
      ReadFinishErrorRetryable = DataStatusRetryableBase+ReadFinishError, ///< @deprecated
      WriteFinishErrorRetryable = DataStatusRetryableBase+WriteFinishError, ///< @deprecated
      CreateDirectoryErrorRetryable = DataStatusRetryableBase+CreateDirectoryError, ///< @deprecated
      RenameErrorRetryable = DataStatusRetryableBase+RenameError, ///< @deprecated
      GenericErrorRetryable = DataStatusRetryableBase+GenericError ///< @deprecated
   };

    /// Constructor to use when errno-like information is not available
    DataStatus(const DataStatusType& status, std::string desc="")
      : status(status), Errno(0), desc(desc) {}

    /// Construct a new DataStatus with errno and optional text description
    /** If the status is an error condition then error_no must be set to a
     * non-zero value */
    DataStatus(const DataStatusType& status, int error_no, const std::string& desc="")
      : status(status), Errno(error_no), desc(desc) {}

    /// Construct a new DataStatus with fields initialised to success states
    DataStatus()
      : status(Success), Errno(0), desc("") {}

    bool operator==(const DataStatusType& s) {
      return status == s;
    }
    bool operator==(const DataStatus& s) {
      return status == s.status;
    }
  
    bool operator!=(const DataStatusType& s) {
      return status != s;
    }
    bool operator!=(const DataStatus& s) {
      return status != s.status;
    }

    DataStatus operator=(const DataStatusType& s) {
      status = s;
      Errno = 0;
      return *this;
    }

    bool operator!() const {
      return (status != Success) && (status != SuccessCached);
    }
    operator bool() const {
      return (status == Success) || (status == SuccessCached);
    }

    /// Returns true if no error occurred
    bool Passed() const {
      return ((status == Success) || (status == NotSupportedForDirectDataPointsError) ||
              (status == ReadPrepareWait) || (status == WritePrepareWait) ||
              (status == SuccessCached));
    }
  
    /// Returns true if the error was temporary and could be retried.
    /** Retryable error numbers are EAGAIN, EBUSY, ETIMEDOUT, EARCSVCTMP,
     * EARCTRANSFERTIMEOUT and EARCCHECKSUM. */
    bool Retryable() const;

    /// Set the error number
    void SetErrNo(int error_no) {
      Errno = error_no;
    }
  
    /// Get the error number
    int GetErrno() const {
      return Errno;
    }

    /// Get text description of the error number
    std::string GetStrErrno() const;

    /// Set a detailed description of the status, removing trailing new line if present
    void SetDesc(const std::string& d) {
      desc = trim(d);
    }
    
    /// Get a detailed description of the status
    std::string GetDesc() const {
      return desc;
    }

    /// Returns a human-friendly readable string with all error information
    operator std::string(void) const;

  private:
  
    /// status code
    DataStatusType status;
    /// error number (values defined in errno.h)
    int Errno;
    /// description of failure
    std::string desc;

  };

  inline std::ostream& operator<<(std::ostream& o, const DataStatus& d) {
    return (o << ((std::string)d));
  }
} // namespace Arc


#endif // __ARC_DATASTATUS_H__
