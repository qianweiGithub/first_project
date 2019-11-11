#ifndef SEARCH_EVERYTHING_HELPER_H_
#define SEARCH_EVERYTHING_HELPER_H_ 1


enum ServiceStatus {
  kServiceStatus_Unknown = 0,
  kServiceStatus_AccessFailed,
  kServiceStatus_ServiceNotInstalled,
  kServiceStatus_ServiceInstalled
};

class EverythingHelper {
 public:
   static bool Initialize();

};  // class EverythingHelper

#endif  // SEARCH_EVERYTHING_HELPER_H_