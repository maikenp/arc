// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <list>
#include <map>

#include <sys/types.h>
#include <unistd.h>

#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/compute/JobDescription.h>

#include "RSLParser.h"
#include "XRSLParser.h"

namespace Arc {

  /// \mapname xRSL xRSL (nordugrid:xrsl)
  /// The libarccompute library has full support for xRSL. The
  /// reference manual is located <a href="http://www.nordugrid.org/documents/xrsl.pdf">here</a>.
  /// By default the xRSL parser expects and produces user-side RSL (see
  /// reference manual), however if GM-side RSL is passed as input or wanted as
  /// output, then the "GRIDMANAGER" dialect should be used.
  XRSLParser::XRSLParser(PluginArgument* parg)
    : JobDescriptionParserPlugin(parg) {
    supportedLanguages.push_back("nordugrid:xrsl");
  }

  XRSLParser::~XRSLParser() {}

  Plugin* XRSLParser::Instance(PluginArgument *arg) {
    return new XRSLParser(arg);
  }

  static Software::ComparisonOperator convertOperator(const RSLRelOp& op) {
    if (op == RSLNotEqual) return &Software::operator!=;
    if (op == RSLLess) return &Software::operator<;
    if (op == RSLGreater) return &Software::operator>;
    if (op == RSLLessOrEqual) return &Software::operator <=;
    if (op == RSLGreaterOrEqual) return &Software::operator>=;
    return &Software::operator==;
  }

  static RSLRelOp convertOperator(const Software::ComparisonOperator& op) {
    if (op == &Software::operator==) return RSLEqual;
    if (op == &Software::operator<)  return RSLLess;
    if (op == &Software::operator>)  return RSLGreater;
    if (op == &Software::operator<=) return RSLLessOrEqual;
    if (op == &Software::operator>=) return RSLGreaterOrEqual;
    return RSLNotEqual;
  }

  static std::list<const RSL*> SplitRSL(const RSL *r) {
    const RSLBoolean *b;
    std::list<const RSL*> l;
    if ((b = dynamic_cast<const RSLBoolean*>(r)) && b->Op() == RSLMulti)
      for (std::list<RSL*>::const_iterator it = b->begin();
           it != b->end(); it++) {
        std::list<const RSL*> L = SplitRSL(*it);
        l.insert(l.end(), L.begin(), L.end());
      }
    else
      l.push_back(r);
    return l;
  }

  /// \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseExecutablesAttribute(JobDescription& j) {
    std::map<std::string, std::string>::iterator itExecsAtt = j.OtherAttributes.find("nordugrid:xrsl;executables");
    if (itExecsAtt == j.OtherAttributes.end()) {
      return true;
    }

    RSLParser rp("&(executables = " + itExecsAtt->second + ")");
    const RSL* rexecs = rp.Parse(false);
    const RSLBoolean* bexecs;
    const RSLCondition* cexecs;
    std::list<std::string> execs;

    if (rexecs == NULL ||
        (bexecs = dynamic_cast<const RSLBoolean*>(rexecs)) == NULL ||
        (cexecs = dynamic_cast<const RSLCondition*>(*bexecs->begin())) == NULL ||
        !ListValue(cexecs, execs)) {
      // Should not happen.
      logger.msg(DEBUG, "Error parsing the internally set executables attribute.");
      return false;
    }

    for (std::list<std::string>::const_iterator itExecs = execs.begin();
         itExecs != execs.end(); itExecs++) {
      bool fileExists = false;
      for (std::list<InputFileType>::iterator itFile = j.DataStaging.InputFiles.begin(); itFile != j.DataStaging.InputFiles.end(); itFile++) {
        if (itFile->Name == (*itExecs)) {
          itFile->IsExecutable = true;
          fileExists = true;
        }
      }

      if (!fileExists) {
        logger.msg(INFO, "File \"%s\" in the executables attribute is not present in the inputfiles attribute", *itExecs);
        return false;
      }
    }

    // executables attribute only stored for later parsing, removing it now.
    j.OtherAttributes.erase(itExecsAtt);

    return true;
  }

  /// TODO \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseFTPThreadsAttribute(JobDescription& j) {
    std::map<std::string, std::string>::iterator itAtt;
    itAtt = j.OtherAttributes.find("nordugrid:xrsl;ftpthreads");
    if (itAtt == j.OtherAttributes.end()) {
      return true;
    }

    int threads;
    if (!stringto(itAtt->second, threads) || threads < 1 || 10 < threads) {
      logger.msg(INFO, "The value of the ftpthreads attribute must be a number from 1 to 10.");
      return false;
    }

    for (std::list<InputFileType>::iterator itF = j.DataStaging.InputFiles.begin();
         itF != j.DataStaging.InputFiles.end(); itF++) {
      for (std::list<SourceType>::iterator itS = itF->Sources.begin();
           itS != itF->Sources.end(); itS++) {
        itS->AddOption("threads", itAtt->second);
      }
    }

    for (std::list<OutputFileType>::iterator itF = j.DataStaging.OutputFiles.begin();
         itF != j.DataStaging.OutputFiles.end(); itF++) {
      for (std::list<TargetType>::iterator itT = itF->Targets.begin();
           itT != itF->Targets.end(); itT++) {
        itT->AddOption("threads", itAtt->second);
      }
    }

    j.OtherAttributes.erase(itAtt);

    return true;
  }

  /// TODO \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseCacheAttribute(JobDescription& j) {
    std::map<std::string, std::string>::iterator itAtt;
    itAtt = j.OtherAttributes.find("nordugrid:xrsl;cache");
    if (itAtt == j.OtherAttributes.end()) {
      return true;
    }

    for (std::list<InputFileType>::iterator itF = j.DataStaging.InputFiles.begin();
         itF != j.DataStaging.InputFiles.end(); itF++) {
      if (!itF->IsExecutable) {
        for (std::list<SourceType>::iterator itS = itF->Sources.begin();
             itS != itF->Sources.end(); itS++) {
          itS->AddOption("cache", itAtt->second);
        }
      }
    }

    j.OtherAttributes.erase(itAtt);

    return true;
  }

  /// TODO \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseJoinAttribute(JobDescription& j) {
    std::map<std::string, std::string>::iterator itAtt;
    itAtt = j.OtherAttributes.find("nordugrid:xrsl;join");
    if (itAtt == j.OtherAttributes.end() || (itAtt->second != "yes" && itAtt->second != "true")) {
      return true;
    }

    if (j.Application.Output.empty()) {
      logger.msg(ERROR, "Xrsl attribute join is set but attribute stdout is not set");
      return false;
    }

    if (!j.Application.Error.empty()) {
      if (j.Application.Error != j.Application.Output) {
        logger.msg(ERROR, "Xrsl attribute join is set but attribute stderr is also set");
        return false;
      }
    }

    j.Application.Error = j.Application.Output;

    j.OtherAttributes.erase(itAtt);

    return true;
  }

  /// TODO \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseGridTimeAttribute(JobDescription& j) { // Must be called after the 'count' attribute has been parsed.
    std::map<std::string, std::string>::iterator itAtt;
    itAtt = j.OtherAttributes.find("nordugrid:xrsl;gridtime");

    if (itAtt == j.OtherAttributes.end()) {
      return true;
    }

    if (j.Resources.TotalCPUTime.range.max != -1) {
      logger.msg(ERROR, "The XRSL attributes gridtime and cputime cannot be specified together.");
      return false;
    }
    if (j.Resources.IndividualWallTime.range.max != -1) {
      logger.msg(ERROR, "The XRSL attributes gridtime and walltime cannot be specified together.");
      return false;
    }

    j.Resources.TotalCPUTime.range = Period(itAtt->second, PeriodMinutes).GetPeriod();
    j.Resources.TotalCPUTime.benchmark = std::pair<std::string, double>("clock rate", 2800);

    int slots = (j.Resources.SlotRequirement.NumberOfSlots > 0 ? j.Resources.SlotRequirement.NumberOfSlots : 1);
    j.Resources.IndividualWallTime.range = Period(itAtt->second, PeriodMinutes).GetPeriod()*slots;
    j.Resources.IndividualWallTime.benchmark = std::pair<std::string, double>("clock rate", 2800);

    j.OtherAttributes.erase(itAtt);
    
    return true;
  }

  /// TODO \mapattr executables -> InputFileType::IsExecutable
  bool XRSLParser::ParseCountPerNodeAttribute(JobDescription& j) { // Must be called after the 'count' attribute has been parsed.
    std::map<std::string, std::string>::iterator   itAtt;
    itAtt = j.OtherAttributes.find("nordugrid:xrsl;countpernode");
    
    if (itAtt == j.OtherAttributes.end()) return true;
    
    if (j.Resources.SlotRequirement.NumberOfSlots == -1) {
      logger.msg(ERROR, "When specifying the countpernode XRSL attribute, the count attribute must also be specified.");
      return false;
    }
    
    if (!stringto(itAtt->second, j.Resources.SlotRequirement.SlotsPerHost)) {
      logger.msg(ERROR, "The countpernode XRSL attribute must be an integer.");
      return false;
    }

    return true;
  }


  JobDescriptionParserPluginResult XRSLParser::Parse(const std::string& source, std::list<JobDescription>& jobdescs, const std::string& language, const std::string& dialect) const {
    if (language != "" && !IsLanguageSupported(language)) {
      return false;
    }

    jobdescs.clear();

    logger.msg(VERBOSE, "Parsing string using XRSLParser");

    RSLParser parser(source);
    const RSL *r = parser.Parse();
    if (!r) {
      logger.msg(VERBOSE, "RSL parsing error");
      return false;
    }

    std::list<const RSL*> l = SplitRSL(r);

    for (std::list<const RSL*>::iterator it = l.begin(); it != l.end(); it++) {
      jobdescs.push_back(JobDescription());

      if (!Parse(*it, jobdescs.back(), dialect)) {
        logger.msg(ERROR, "XRSL parsing error");
        jobdescs.clear();
        return false;
      }

      // Parse remaining attributes if any.
      if (!ParseExecutablesAttribute(jobdescs.back())) {
        return false;
      }
      if (!ParseFTPThreadsAttribute(jobdescs.back())) {
        return false;
      }
      if (!ParseCacheAttribute(jobdescs.back())) {
        return false;
      }
      if (!ParseCountPerNodeAttribute(jobdescs.back())) {
        return false;
      }
      if (dialect != "GRIDMANAGER" && !ParseJoinAttribute(jobdescs.back())) { // join is a client side attribute
        return false;
      }
      if (dialect != "GRIDMANAGER" && !ParseGridTimeAttribute(jobdescs.back())) { // gridtime is a client side attribute
        return false;
      }
      for (std::list<JobDescription>::iterator itJob = jobdescs.back().GetAlternatives().begin();
           itJob != jobdescs.back().GetAlternatives().end(); itJob++) {
        if (!ParseExecutablesAttribute(*itJob)) {
          return false;
        }
        if (!ParseFTPThreadsAttribute(*itJob)) {
          return false;
        }
        if (!ParseCacheAttribute(*itJob)) {
          return false;
        }
        if (!ParseCountPerNodeAttribute(*itJob)) {
          return false;
        }
        if (dialect != "GRIDMANAGER" && !ParseJoinAttribute(*itJob)) { // join is a client side attribute
          return false;
        }
        if (dialect != "GRIDMANAGER" && !ParseGridTimeAttribute(*itJob)) { // gridtime is a client side attribute
          return false;
        }
      }

      std::stringstream ss;
      ss << **it;
      jobdescs.back().OtherAttributes["nordugrid:xrsl;clientxrsl"] = ss.str();
      SourceLanguage(jobdescs.back()) = (!language.empty() ? language : supportedLanguages.front());
      for (std::list<JobDescription>::iterator itAltJob = jobdescs.back().GetAlternatives().begin();
         itAltJob != jobdescs.back().GetAlternatives().end(); ++itAltJob) {
        itAltJob->OtherAttributes["nordugrid:xrsl;clientxrsl"] = ss.str();
        SourceLanguage(*itAltJob) = jobdescs.back().GetSourceLanguage();
      }
    }

    if(jobdescs.empty()) {
      // Probably never happens so check is just in case of future changes
      logger.msg(WARNING, "No RSL content in job desription found");
      return false;
    }

    if(dialect == "GRIDMANAGER") {
      if (jobdescs.size() > 1) {
        jobdescs.clear();
        return false;
      }

      std::string action = "request";
      if (jobdescs.front().OtherAttributes.find("nordugrid:xrsl;action") != jobdescs.front().OtherAttributes.end()) {
        action = jobdescs.front().OtherAttributes["nordugrid:xrsl;action"];
      }
      // action = request means real job description.
      // Any other action may (and currently should) have almost
      // empty job description.
      if (action == "request") {
        if(jobdescs.front().Application.Executable.Path.empty()) {
          jobdescs.clear();
          return false;
        }
      }
    }
    else {
      // action is not expected in client side job request
      for (std::list<JobDescription>::iterator it = jobdescs.begin(); it != jobdescs.end(); it++) {
        if (it->OtherAttributes.find("nordugrid:xrsl;action") != it->OtherAttributes.end()) {
          jobdescs.clear();
          return false;
        }
        if (it->Application.Executable.Path.empty()) {
          jobdescs.clear();
          return false;
        }
      }
    }

    logger.msg(INFO, "String successfully parsed as %s.", jobdescs.front().GetSourceLanguage());
    return true;
  }

  bool XRSLParser::SingleValue(const RSLCondition *c,
                               std::string& value) {
    if (!value.empty()) {
      logger.msg(ERROR, "XRSL attribute %s multiply defined", c->Attr());
      return false;
    }
    if (c->size() != 1) {
      logger.msg(ERROR, "XRSL attribute %s is not a single value", c->Attr());
      return false;
    }
    const RSLLiteral *n = dynamic_cast<const RSLLiteral*>(*c->begin());
    if (!n) {
      logger.msg(ERROR, "XRSL attribute %s is not a string", c->Attr());
      return false;
    }
    value = n->Value();
    return true;
  }

  bool XRSLParser::ListValue(const RSLCondition *c,
                             std::list<std::string>& value) {
    if (!value.empty()) {
      logger.msg(ERROR, "XRSL attribute %s multiply defined", c->Attr());
      return false;
    }
    for (std::list<RSLValue*>::const_iterator it = c->begin();
         it != c->end(); it++) {
      const RSLLiteral *n = dynamic_cast<const RSLLiteral*>(*it);
      if (!n) {
        logger.msg(ERROR, "XRSL attribute %s is not a string", c->Attr());
        return false;
      }
      value.push_back(n->Value());
    }
    return true;
  }

  bool XRSLParser::SeqListValue(const RSLCondition *c,
                                std::list<std::list<std::string> >& value,
                                int seqlength) {
    if (!value.empty()) {
      logger.msg(ERROR, "XRSL attribute %s multiply defined", c->Attr());
      return false;
    }
    for (std::list<RSLValue*>::const_iterator it = c->begin();
         it != c->end(); it++) {
      const RSLSequence *s = dynamic_cast<const RSLSequence*>(*it);
      if (!s) {
        logger.msg(ERROR, "XRSL attribute %s is not sequence", c->Attr());
        return false;
      }
      if (seqlength != -1 && int(s->size()) != seqlength) {
        logger.msg(ERROR, "XRSL attribute %s has wrong sequence length",
                   c->Attr());
        return false;
      }
      std::list<std::string> l;
      for (std::list<RSLValue*>::const_iterator it = s->begin();
           it != s->end(); it++) {
        const RSLLiteral *n = dynamic_cast<const RSLLiteral*>(*it);
        if (!n) {
          logger.msg(ERROR, "XRSL attribute %s is not a string", c->Attr());
          return false;
        }
        l.push_back(n->Value());
      }
      value.push_back(l);
    }
    return true;
  }

  static char StateToShortcut(const std::string& state) {
    if(state == "PREPARING") {
      return 'b';
    }
    if(state == "INLRMS") {
      return 'q';
    }
    if(state == "FINISHING") {
      return 'f';
    }
    if(state == "FINISHED") {
      return 'e';
    }
    if(state == "DELETED") {
      return 'd';
    }
    if(state == "CANCELING") {
      return 'c';
    }
    return ' ';
  }

  static std::string ShortcutToState(char state) {
    if(state == 'b') {
      return "PREPARING";
    }
    if(state == 'q') {
      return "INLRMS";
    }
    if(state == 'f') {
      return "FINISHING";
    }
    if(state == 'e') {
      return "FINISHED";
    }
    if(state == 'd') {
      return "DELETED";
    }
    if(state == 'c') {
      return "CANCELING";
    }
    return "";
  }

  static bool AddNotificationState(
         NotificationType &notification,
         const std::string& states) {
    for (int n = 0; n<(int)states.length(); n++) {
      std::string state = ShortcutToState(states[n]);
      if (state.empty()) {
        return false;
      }
      for (std::list<std::string>::const_iterator s = notification.States.begin();
           s != notification.States.end(); s++) {
        if(*s == state) { // Check if state is already added.
          state.resize(0);
          break;
        }
      }
      if (!state.empty()) {
        notification.States.push_back(state);
      }
    }
    return true;
  }

  static bool AddNotification(
         std::list<NotificationType> &notifications,
         const std::string& states, const std::string& email) {
    for (std::list<NotificationType>::iterator it = notifications.begin();
         it != notifications.end(); it++) {
      if (it->Email == email) { // If email already exist in the list add states to that entry.
        return AddNotificationState(*it,states);
      }
    }

    NotificationType notification;
    notification.Email = email;
    if (!AddNotificationState(notification,states)) {
      return false;
    }
    notifications.push_back(notification);

    return true;
  }

  bool XRSLParser::Parse(const RSL *r, JobDescription& j, const std::string& dialect) const {
    const RSLBoolean *b;
    const RSLCondition *c;
    if ((b = dynamic_cast<const RSLBoolean*>(r))) {
      if (b->Op() == RSLAnd) {
        for (std::list<RSL*>::const_iterator it = b->begin();
             it != b->end(); it++)
          if (!Parse(*it, j, dialect)) {
            return false;
          }
      }
      else if (b->Op() == RSLOr) {
        if (b->size() == 0) {
          return true;
        }

        JobDescription jcopy(j, false);

        if (!Parse(*b->begin(), j, dialect)) {
          return false;
        }

        std::list<RSL*>::const_iterator it = b->begin();
        for (it++; it != b->end(); it++) {
          JobDescription aj(jcopy);
          if (!Parse(*it, aj, dialect)) {
            return false;
          }
          j.AddAlternative(aj);
        }

        return true;
     }
      else {
        logger.msg(ERROR, "Unexpected RSL type");
        return false;
      }
    }
    else if ((c = dynamic_cast<const RSLCondition*>(r))) {
      /// \mapattr executable -> ExecutableType::Path
      if (c->Attr() == "executable") {
        bool r = SingleValue(c, j.Application.Executable.Path);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Application.Executable.Path);
        }

        return r;
      }

      /// \mapattr arguments -> ExecutableType::Argument
      if (c->Attr() == "arguments") {
        bool r = ListValue(c, j.Application.Executable.Argument);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= ListValue(c, it->Application.Executable.Argument);
        }

        return r;
      }

      /// \mapattr stdin -> Input
      if (c->Attr() == "stdin") {
        bool r = SingleValue(c, j.Application.Input);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Application.Input);
        }

        return r;
      }

      /// \mapattr stdout -> Output
      if (c->Attr() == "stdout") {
        bool r = SingleValue(c, j.Application.Output);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Application.Output);
        }

        return r;
      }

      /// \mapattr stderr -> Error
      if (c->Attr() == "stderr") {
        bool r = SingleValue(c, j.Application.Error);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Application.Error);
        }

        return r;
      }

      /// TODO \mapattr inputfiles -> DataStagingType::InputFiles
      if (c->Attr() == "inputfiles") {
        std::list<std::list<std::string> > ll;
        if (!SeqListValue(c, ll))
          return false;
        for (std::list<std::list<std::string> >::iterator it = ll.begin();
             it != ll.end(); it++) {
          std::list<std::string>::iterator it2 = it->begin();

          // Check whether the first string exists in the list
          if (it2 == it->end()) {
            return false;
          }

          InputFileType file;
          file.Name = *it2++;

          // Check whether the second string exists in the list
          if (it2 == it->end()) {
            return false;
          }

          bool is_file = true;
          long fileSize = -1;
          std::string fileChecksum;
          // For USER dialect (default) the second string must be a path to a file or an URL.
          // For GRIDMANAGER dialect the second string in the list might either be a URL or filesize.checksum.
          if (dialect == "GRIDMANAGER" && !it2->empty()) {
            std::string::size_type sep = it2->find('.');
            if(sep == std::string::npos) {
              if(!stringto(*it2, fileSize)) is_file = false;
            } else {
              fileChecksum = it2->substr(sep+1);
              if(!stringto(it2->substr(0,sep), fileSize)) is_file = false;
            }
            if(fileSize < 0) is_file = false;
          }
          if (dialect != "GRIDMANAGER" || !is_file) {
            URL turl(*it2);
            if (!turl) {
              return false;
            }
            URLLocation location;
            for (it2++; it2 != it->end(); ++it2) {
              // add any options and locations
              // an option applies to the URL preceding it (main or location)
              std::string::size_type pos = it2->find('=');
              if (pos == std::string::npos) {
                logger.msg(ERROR, "Invalid URL option syntax in option %s for input file %s", *it2, file.Name);
                return false;
              }
              std::string attr_name(it2->substr(0, pos));
              std::string attr_value(it2->substr(pos+1));
              if (attr_name == "location") {
                if (location)
                  turl.AddLocation(location);
                location = URLLocation(attr_value);
                if (!location) {
                  logger.msg(ERROR, "Invalid URL: %s in input file %s", attr_value, file.Name);
                  return false;
                }
              } else if (location) {
                location.AddOption(attr_name, attr_value, true);
              } else {
                turl.AddOption(attr_name, attr_value, true);
              }
            }
            if (location)
              turl.AddLocation(location);
            file.Sources.push_back(turl);
          }
          else {
            file.FileSize = fileSize;
            file.Checksum = fileChecksum;
            if(dialect != "GRIDMANAGER") file.Sources.push_back(URL(file.Name));
          }
          file.IsExecutable = false;

          j.DataStaging.InputFiles.push_back(file);
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->DataStaging.InputFiles.push_back(file);
          }
        }
        return true;
      }

      // Mapping documented above
      if (c->Attr() == "executables") {
        std::list<std::string> execs;
        if (!ListValue(c, execs))
          return false;
        /* Store value in the OtherAttributes member and set it later when all
         * the attributes it depends on has been parsed.
         */
        std::ostringstream os;
        c->List().Print(os);
        j.OtherAttributes["nordugrid:xrsl;executables"] = os.str();
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;executables"] = os.str();
        }

        return true;
      }

      // Mapping documented above
      if (c->Attr() == "cache") {
        std::string cache;
        if (!SingleValue(c, cache))
          return false;
        /* Store value in the OtherAttributes member and set it later when all
         * the attributes it depends on has been parsed.
         */
        j.OtherAttributes["nordugrid:xrsl;cache"] = cache;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;cache"] = cache;
        }

        return true;
      }

      /// TODO \mapattr outputfiles -> DataStagingType::OutputFiles
      if (c->Attr() == "outputfiles") {
        std::list<std::list<std::string> > ll;
        if (!SeqListValue(c, ll))
          return false;
        for (std::list<std::list<std::string> >::iterator it = ll.begin();
             it != ll.end(); it++) {
          std::list<std::string>::iterator it2 = it->begin();

          // Check whether the first string exists in the list
          if (it2 == it->end()) {
            return false;
          }

          OutputFileType file;
          file.Name = *it2++;

          // Check whether the second string exists in the list
          if (it2 == it->end()) {
            return false;
          }

          URL turl(*it2);
          // The second string in the list (it2) might be a URL or empty
          if (!it2->empty() && turl.Protocol() != "file") {
            if (!turl) {
              return false;
            }
            URLLocation location;
            for (it2++; it2 != it->end(); ++it2) {
              // add any options and locations
              // an option applies to the URL preceding it (main or location)
              std::string::size_type pos = it2->find('=');
              if (pos == std::string::npos) {
                logger.msg(ERROR, "Invalid URL option syntax in option %s for output file %s", *it2, file.Name);
                return false;
              }
              std::string attr_name(it2->substr(0, pos));
              std::string attr_value(it2->substr(pos+1));
              if (attr_name == "location") {
                if (location)
                  turl.AddLocation(location);
                location = URLLocation(attr_value);
                if (!location) {
                  logger.msg(ERROR, "Invalid URL: %s in output file %s", attr_value, file.Name);
                  return false;
                }
              } else if (location) {
                location.AddOption(attr_name, attr_value, true);
              } else {
                turl.AddOption(attr_name, attr_value, true);
              }
            }
            if (location)
              turl.AddLocation(location);
            file.Targets.push_back(turl);
          }

          j.DataStaging.OutputFiles.push_back(file);
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->DataStaging.OutputFiles.push_back(file);
          }
        }
        return true;
      }

      /// \mapattr queue -> QueueName
      /// TODO \mapattr queue -> JobDescription::OtherAttributes["nordugrid:broker;reject_queue"]
      if (c->Attr() == "queue") {
        std::string queueName;
        if (!SingleValue(c, queueName))
          return false;
        if (dialect == "GRIDMANAGER" && c->Op() != RSLEqual) {
          logger.msg(ERROR, "Parsing the queue xrsl attribute failed. An invalid comparison operator was used, only \"=\" is allowed.");
          return false;
        }
        if (c->Op() != RSLNotEqual && c->Op() != RSLEqual) {
          logger.msg(ERROR, "Parsing the queue xrsl attribute failed. An invalid comparison operator was used, only \"!=\" or \"=\" are allowed.");
          return false;
        }

        if (c->Op() == RSLNotEqual) {
          j.OtherAttributes["nordugrid:broker;reject_queue"] = queueName;
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->OtherAttributes["nordugrid:broker;reject_queue"] = queueName;
          }
        }
        else {
          j.Resources.QueueName = queueName;
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->Resources.QueueName = queueName;
          }
        }

        return true;
      }

      /// \mapattr starttime -> ProcessingStartTime
      if (c->Attr() == "starttime") {
        std::string time;
        if (!SingleValue(c, time))
          return false;
        j.Application.ProcessingStartTime = time;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.ProcessingStartTime = time;
        }
        return true;
      }

      /// \mapattr lifetime -> SessionLifeTime
      if (c->Attr() == "lifetime") {
        std::string time;
        if (!SingleValue(c, time))
          return false;
        if(dialect == "GRIDMANAGER") {
          // No alternatives allowed for GRIDMANAGER dialect.
          j.Resources.SessionLifeTime = Period(time, PeriodSeconds);
        } else {
          j.Resources.SessionLifeTime = Period(time, PeriodMinutes);
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->Resources.SessionLifeTime = Period(time, PeriodMinutes);
          }
        }
        return true;
      }

      /// \mapattr cputime -> TotalCPUTime "With user-side RSL minutes is expected, while for GM-side RSL seconds."
      if (c->Attr() == "cputime") {
        std::string time;
        if (!SingleValue(c, time))
          return false;
        if(dialect == "GRIDMANAGER") {
          // No alternatives allowed for GRIDMANAGER dialect.
          j.Resources.TotalCPUTime = Period(time, PeriodSeconds).GetPeriod();
        } else {
          j.Resources.TotalCPUTime = Period(time, PeriodMinutes).GetPeriod();
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->Resources.TotalCPUTime = Period(time, PeriodMinutes).GetPeriod();
          }
        }
        return true;
      }

      /// TotalWallTime is a reference to IndividualWallTime
      /// \mapattr walltime -> IndividualWallTime
      /// TODO cputime dialect/units
      if (c->Attr() == "walltime") {
        std::string time;
        if (!SingleValue(c, time))
          return false;
        if(dialect == "GRIDMANAGER") {
          // No alternatives allowed for GRIDMANAGER dialect.
          j.Resources.TotalWallTime = Period(time, PeriodSeconds).GetPeriod();
        } else {
          j.Resources.TotalWallTime = Period(time, PeriodMinutes).GetPeriod();
          for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
               it != j.GetAlternatives().end(); it++) {
            it->Resources.TotalWallTime = Period(time, PeriodMinutes).GetPeriod();
          }
        }
        return true;
      }

      // Documented above.
      if (c->Attr() == "gridtime") {
        std::string time;
        if (!SingleValue(c, time))
          return false;

        /* Store value in the OtherAttributes member and set it later when all
         * the attributes it depends on has been parsed.
         */
        j.OtherAttributes["nordugrid:xrsl;gridtime"] = time;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;gridtime"] = time;
        }

        return true;
      }

      // TODO \mapattr benchmarks -> ResourcesType::TotalWallTime
      if (c->Attr() == "benchmarks") {
        std::list<std::list<std::string> > bm;
        if (!SeqListValue(c, bm, 3))
          return false;
        double bValue;
        // Only the first parsable benchmark is currently supported.
        for (std::list< std::list<std::string> >::const_iterator it = bm.begin();
             it != bm.end(); it++) {
          std::list<std::string>::const_iterator itB = it->begin();
          if (!stringto(*++itB, bValue))
            continue;
          if(dialect == "GRIDMANAGER") {
            j.Resources.TotalCPUTime.range = Period(*++itB, PeriodSeconds).GetPeriod();
          } else {
            j.Resources.TotalCPUTime.range = Period(*++itB, PeriodMinutes).GetPeriod();
          }
          j.Resources.TotalCPUTime.benchmark = std::pair<std::string, double>(it->front(), bValue);
          return true;
        }
        return false;
      }

      /// \mapattr memory -> IndividualPhysicalMemory
      if (c->Attr() == "memory") {
        std::string mem;
        if (!SingleValue(c, mem))
          return false;

        j.Resources.IndividualPhysicalMemory = stringto<int>(mem);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Resources.IndividualPhysicalMemory = j.Resources.IndividualPhysicalMemory;
        }

        return true;
      }

      /// \mapattr disk -> DiskSpace
      if (c->Attr() == "disk") {
        std::string disk;
        if (!SingleValue(c, disk))
          return false;
        j.Resources.DiskSpaceRequirement.DiskSpace = stringto<int>(disk);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Resources.DiskSpaceRequirement.DiskSpace = j.Resources.DiskSpaceRequirement.DiskSpace;
        }

        return true;
      }

      // TODO: Maybe add support for RTE options.
      /// \mapattr runtimeenvironment -> RunTimeEnvironment
      if (c->Attr() == "runtimeenvironment") {
        std::string runtime;
        if (!SingleValue(c, runtime))
          return false;

        j.Resources.RunTimeEnvironment.add(Software(runtime), convertOperator(c->Op()));
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Resources.RunTimeEnvironment.add(Software(runtime), convertOperator(c->Op()));
        }

        return true;
       }

      /// \mapattr middleware -> CEType
      // This attribute should be passed to the broker and should not be stored.
      if (c->Attr() == "middleware") {
        std::string cetype;
        if (!SingleValue(c, cetype))
          return false;
        j.Resources.CEType.add(Software(cetype), convertOperator(c->Op()));
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Resources.CEType.add(Software(cetype), convertOperator(c->Op()));
        }

        return true;
      }

      /// \mapattr opsys -> OperatingSystem
      if (c->Attr() == "opsys") {
        std::string opsys;
        if (!SingleValue(c, opsys))
          return false;
        j.Resources.OperatingSystem.add(Software(opsys), convertOperator(c->Op()));
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Resources.OperatingSystem.add(Software(opsys), convertOperator(c->Op()));
        }

        return true;
      }

      // Documented above.
      if (c->Attr() == "join") {
        if (dialect == "GRIDMANAGER") {
          // Ignore the join attribute for GM (it is a client side attribute).
          return true;
        }

        std::string join;
        if (!SingleValue(c, join)) {
          return false;
        }
        /* Store value in the OtherAttributes member and set it later when all
         * the attributes it depends on has been parsed.
         */
        j.OtherAttributes["nordugrid:xrsl;join"] = join;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;join"] = join;
        }

        return true;
      }

      /// \mapattr gmlog -> LogDir
      if (c->Attr() == "gmlog") {
        bool r = SingleValue(c, j.Application.LogDir);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Application.LogDir);
        }

        return r;
      }

      /// \mapattr jobname -> JobName
      if (c->Attr() == "jobname") {
        bool r = SingleValue(c, j.Identification.JobName);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Identification.JobName);
        }

        return r;
      }

      // Documented above.
      if (c->Attr() == "ftpthreads") {
        std::string sthreads;
        if (!SingleValue(c, sthreads))
          return false;
        /* Store value in the OtherAttributes member and set it later when all
         * the attributes it depends on has been parsed.
         */
        j.OtherAttributes["nordugrid:xrsl;ftpthreads"] = sthreads;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;ftpthreads"] = sthreads;
        }

        return true;
      }

      /// \mapattr acl -> AccessControl
      if (c->Attr() == "acl") {
        std::string acl;
        if (!SingleValue(c, acl))
          return false;

        XMLNode node(acl);
        if (!node) {
          logger.msg(ERROR, "The value of the acl XRSL attribute isn't valid XML.");
          return false;
        }
        
        node.New(j.Application.AccessControl);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          node.New(it->Application.AccessControl);
        }
        return true;
      }

      // TODO Document non existent mapping.
      if (c->Attr() == "cluster") {
        logger.msg(ERROR, "The cluster XRSL attribute is currently unsupported.");
        return false;
      }

      /// TODO: \mapattr notify -> ApplicationType::Notification
      if (c->Attr() == "notify") {
        std::list<std::string> l;
        if (!ListValue(c, l))
          return false;
        for (std::list<std::string>::const_iterator notf = l.begin();
             notf != l.end(); ++notf) {
          std::list<std::string> ll;
          tokenize(*notf, ll, " \t");
          std::list<std::string>::const_iterator it = ll.begin();
          std::string states = "be"; // Default value.
          if (it->find('@') == std::string::npos) { // The first string is state flags.
            if (ll.size() == 1) { // Only state flags in value.
              logger.msg(ERROR, "Syntax error in notify attribute value ('%s'), it must contain an email address.", *notf);
              return false;
            }
            states = *it;
            it++;
          }
          for (; it != ll.end(); it++) {
            if (it->find('@') == std::string::npos) {
              logger.msg(ERROR, "Syntax error in notify attribute value ('%s'), it must only contain email addresses after state flag(s).", *notf);
              return false;
            }
            if(!AddNotification(j.Application.Notification,states,*it)) {
              logger.msg(ERROR, "Syntax error in notify attribute value ('%s'), it contains unknown state flags.", *notf);
              return false;
            }
          }
        }

        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.Notification = j.Application.Notification;
        }

        return true;
      }

      /// \mapattr replicacollection -> OtherAttributes
      /// TODO \mapattr replicacollection -> OtherAttributes["nordugrid:xrsl;replicacollection"]
      // Is this attribute supported?
      if (c->Attr() == "replicacollection") {
        std::string collection;
        if (!SingleValue(c, collection))
          return false;
        if (!URL(collection))
          return false;
        j.OtherAttributes["nordugrid:xrsl;replicacollection"] = collection;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;replicacollection"] = collection;
        }
      }

      /// \mapattr rerun -> Rerun
      if (c->Attr() == "rerun") {
        std::string rerun;
        if (!SingleValue(c, rerun))
          return false;
        j.Application.Rerun = stringtoi(rerun);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.Rerun = j.Application.Rerun;
        }
        return true;
      }

      /// \mapattr priority -> Priority
      if (c->Attr() == "priority") {
        std::string priority;
        if (!SingleValue(c, priority))
          return false;
        j.Application.Priority = stringtoi(priority);
        if (j.Application.Priority > 100) {
          logger.msg(VERBOSE, "priority is too large - using max value 100");
          j.Application.Priority = 100;
        }
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.Priority = j.Application.Priority;
        }
        return true;
      }

      /// \mapattr architecture -> Platform
      if (c->Attr() == "architecture") {
        bool r = SingleValue(c, j.Resources.Platform);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          r &= SingleValue(c, it->Resources.Platform);
        }
        return r;
      }

      /// \mapattr nodeaccess -> NodeAccess
      if (c->Attr() == "nodeaccess") {
        std::list<std::string> l;
        if (!ListValue(c, l))
          return false;
        for (std::list<std::string>::iterator it = l.begin();
             it != l.end(); it++)
          if (*it == "inbound") {
            j.Resources.NodeAccess = ((j.Resources.NodeAccess == NAT_OUTBOUND || j.Resources.NodeAccess == NAT_INOUTBOUND) ? NAT_INOUTBOUND : NAT_INBOUND);
          }
          else if (*it == "outbound") {
            j.Resources.NodeAccess = ((j.Resources.NodeAccess == NAT_INBOUND || j.Resources.NodeAccess == NAT_INOUTBOUND) ? NAT_INOUTBOUND : NAT_OUTBOUND);
          }
          else {
            logger.msg(VERBOSE, "Invalid nodeaccess value: %s", *it);
            return false;
          }
        return true;
      }

      /// \mapattr dryrun -> DryRun
      if (c->Attr() == "dryrun") {
        std::string dryrun;
        if (!SingleValue(c, dryrun)) {
          return false;
        }

        j.Application.DryRun = (lower(dryrun) == "yes");
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.DryRun = j.Application.DryRun;
        }

        return true;
      }

      // Underscore, in 'rsl_substitution', is removed by normalization.
      if (c->Attr() == "rslsubstitution") {
        // Handled internally by the RSL parser
        return true;
      }

      /// \mapattr environment -> Environment
      if (c->Attr() == "environment") {
        std::list<std::list<std::string> > ll;
        if (!SeqListValue(c, ll, 2))
          return false;
        for (std::list<std::list<std::string> >::iterator it = ll.begin();
             it != ll.end(); it++) {
          j.Application.Environment.push_back(std::make_pair(it->front(), it->back()));
        }
        return true;
      }

      /// \mapattr count -> NumberOfSlots
      if (c->Attr() == "count") {
        std::string count;
        if (!SingleValue(c, count) || !stringto(count, j.Resources.SlotRequirement.NumberOfSlots)) return false;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); ++it) {
          it->Resources.SlotRequirement.NumberOfSlots = j.Resources.SlotRequirement.NumberOfSlots;
        }
        return true;
      }

      /// \mapattr countpernode -> SlotsPerHost
      if (c->Attr() == "countpernode") {
        std::string countpernode;
        if (!SingleValue(c, countpernode))
          return false;
        
        j.OtherAttributes["nordugrid:xrsl;countpernode"] = countpernode;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); ++it) {
          it->OtherAttributes["nordugrid:xrsl;countpernode"] = countpernode;
        }

        return true;
      }

      /// \mapattr exclusiveexecution -> ExclusiveExecution
      if (c->Attr() == "exclusiveexecution") {
        std::string ee;
        if (!SingleValue(c, ee)) return false;
        ee = lower(ee);
        if (ee != "yes" && ee != "true" && ee != "no" && ee != "false") return false;
        j.Resources.SlotRequirement.ExclusiveExecution = (ee == "yes" || ee == "true") ? SlotRequirementType::EE_TRUE : SlotRequirementType::EE_FALSE;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); ++it) {
          it->Resources.SlotRequirement.ExclusiveExecution = j.Resources.SlotRequirement.ExclusiveExecution;
        }
        return true;
      }

      /// TODO: \mapattr jobreport -> RemoteLogging
      if (c->Attr() == "jobreport") {
        std::string jobreport;
        if (!SingleValue(c, jobreport))
          return false;
        if (!URL(jobreport))
          return false;
        j.Application.RemoteLogging.push_back(RemoteLoggingType());
        j.Application.RemoteLogging.back().Location = URL(jobreport);
        j.Application.RemoteLogging.back().ServiceType = "SGAS";
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.RemoteLogging.push_back(j.Application.RemoteLogging.back());
        }
        return true;
      }

      /// \mapattr credentialserver -> CredentialService
      if (c->Attr() == "credentialserver") {
        std::string credentialserver;
        if (!SingleValue(c, credentialserver))
          return false;
        if (!URL(credentialserver))
          return false;
        j.Application.CredentialService.push_back(credentialserver);
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->Application.CredentialService.push_back(j.Application.CredentialService.back());
        }
        return true;
      }

      // GM-side attributes.
      if (c->Attr() == "action") {
        std::string action;
        if (!SingleValue(c, action))
          return false;
        if (action != "request" && action != "cancel" && action != "clean" && action != "renew" && action != "restart") {
          logger.msg(VERBOSE, "Invalid action value %s", action);
          return false;
        }
        j.OtherAttributes["nordugrid:xrsl;action"] = action;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;action"] = action;
        }

        return true;
      }

      if (c->Attr() == "hostname") {
        std::string hostname;
        if (!SingleValue(c, hostname))
          return false;
        j.OtherAttributes["nordugrid:xrsl;hostname"] = hostname;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;hostname"] = hostname;
        }
        return true;
      }

      if (c->Attr() == "jobid") {
        std::string jobid;
        if (!SingleValue(c, jobid))
          return false;
        j.OtherAttributes["nordugrid:xrsl;jobid"] = jobid;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;jobid"] = jobid;
        }
        return true;
      }

      if (c->Attr() == "clientxrsl") {
        std::string clientxrsl;
        if (!SingleValue(c, clientxrsl))
          return false;
        j.OtherAttributes["nordugrid:xrsl;clientxrsl"] = clientxrsl;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;clientxrsl"] = clientxrsl;
        }
        return true;
      }

      if (c->Attr() == "clientsoftware") {
        std::string clientsoftware;
        if (!SingleValue(c, clientsoftware))
          return false;
        j.OtherAttributes["nordugrid:xrsl;clientsoftware"] = clientsoftware;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;clientsoftware"] = clientsoftware;
        }
        return true;
      }

      if (c->Attr() == "savestate") {
        std::string savestate;
        if (!SingleValue(c, savestate))
          return false;
        j.OtherAttributes["nordugrid:xrsl;savestate"] = savestate;
        for (std::list<JobDescription>::iterator it = j.GetAlternatives().begin();
             it != j.GetAlternatives().end(); it++) {
          it->OtherAttributes["nordugrid:xrsl;savestate"] = savestate;
        }
        return true;
      }

      // Unsupported Globus RSL attributes.
      if (c->Attr() == "resourcemanagercontact" ||
          c->Attr() == "directory" ||
          c->Attr() == "maxcputime" ||
          c->Attr() == "maxwalltime" ||
          c->Attr() == "maxtime" ||
          c->Attr() == "maxmemory" ||
          c->Attr() == "minmemory" ||
          c->Attr() == "grammyjob" ||
          c->Attr() == "project" ||
          c->Attr() == "hostcount" ||
          c->Attr() == "label" ||
          c->Attr() == "subjobcommstype" ||
          c->Attr() == "subjobstarttype" ||
          c->Attr() == "filecleanup" ||
          c->Attr() == "filestagein" ||
          c->Attr() == "filestageinshared" ||
          c->Attr() == "filestageout" ||
          c->Attr() == "gasscache" ||
          c->Attr() == "jobtype" ||
          c->Attr() == "librarypath" ||
          c->Attr() == "remoteiourl" ||
          c->Attr() == "scratchdir") {
        logger.msg(WARNING, "The specified Globus attribute (%s) is not supported. %s ignored.", c->Attr(), c->Attr());
        return true;
      }

      logger.msg(VERBOSE, "Unknown XRSL attribute: %s - Ignoring it.", c->Attr());
      return true;
    }
    else {
      logger.msg(ERROR, "Unexpected RSL type");
      return false;
    }

    // This part will run only when the parsing is at the end of the xrsl file
    return true;
  }

  JobDescriptionParserPluginResult XRSLParser::UnParse(const JobDescription& j, std::string& product, const std::string& language, const std::string& dialect) const {
    if (!IsLanguageSupported(language)) {
      logger.msg(DEBUG, "Wrong language requested: %s",language);
      return false;
    }

    // First check if the job description is valid.
    if (j.Application.Executable.Path.empty()) {
      logger.msg(DEBUG, "Missing executable");
      return false;
    }

    RSLBoolean r(RSLAnd);

    /// \mapattr executable <- ExecutableType::Path
    if (!j.Application.Executable.Path.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.Executable.Path));
      r.Add(new RSLCondition("executable", RSLEqual, l));
    }

    /// \mapattr arguments <- ExecutableType::Argument
    if (!j.Application.Executable.Argument.empty()) {
      RSLList *l = new RSLList;
      for (std::list<std::string>::const_iterator it = j.Application.Executable.Argument.begin();
           it != j.Application.Executable.Argument.end(); it++)
        l->Add(new RSLLiteral(*it));
      r.Add(new RSLCondition("arguments", RSLEqual, l));
    }

    /// \mapattr stdin <- Input
    if (!j.Application.Input.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.Input));
      r.Add(new RSLCondition("stdin", RSLEqual, l));
    }

    /// \mapattr stdout <- Output
    if (!j.Application.Output.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.Output));
      r.Add(new RSLCondition("stdout", RSLEqual, l));
    }

    /// \mapattr stderr <- Error
    if (!j.Application.Error.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.Error));
      r.Add(new RSLCondition("stderr", RSLEqual, l));
    }

    /// \mapattr cputime <- TotalCPUTime
    if (j.Resources.TotalCPUTime.range > -1) {
      RSLList *l = new RSLList;
      if(dialect == "GRIDMANAGER") {
        // Seconds
        l->Add(new RSLLiteral(tostring(j.Resources.TotalCPUTime.range)));
      } else {
        // Free format
        l->Add(new RSLLiteral((std::string)Period(j.Resources.TotalCPUTime.range)));
      }
      r.Add(new RSLCondition("cputime", RSLEqual, l));
    }

    /// \mapattr walltime <- IndividualWallTime
    if (j.Resources.TotalWallTime.range > -1) {
      RSLList *l = new RSLList;
      if(dialect == "GRIDMANAGER") {
        // Seconds
        l->Add(new RSLLiteral(tostring(j.Resources.TotalWallTime.range)));
      } else {
        // Free format
        l->Add(new RSLLiteral((std::string)Period(j.Resources.TotalWallTime.range)));
      }
      r.Add(new RSLCondition("walltime", RSLEqual, l));
    }

    /// \mapattr memory <- IndividualPhysicalMemory
    if (j.Resources.IndividualPhysicalMemory > -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Resources.IndividualPhysicalMemory)));
      r.Add(new RSLCondition("memory", RSLEqual, l));
    }

    /// \mapattr environment <- Environment
    if (!j.Application.Environment.empty()) {
      RSLList *l = new RSLList;
      for (std::list< std::pair<std::string, std::string> >::const_iterator it = j.Application.Environment.begin();
           it != j.Application.Environment.end(); it++) {
        RSLList *s = new RSLList;
        s->Add(new RSLLiteral(it->first));
        s->Add(new RSLLiteral(it->second));
        l->Add(new RSLSequence(s));
      }
      r.Add(new RSLCondition("environment", RSLEqual, l));
    }

    // TODO Document mapping.
    if(dialect == "GRIDMANAGER") {
      RSLList *l = NULL;

      // inputfiles
      //   name url
      //   name size.checksum
      for (std::list<InputFileType>::const_iterator it = j.DataStaging.InputFiles.begin();
           it != j.DataStaging.InputFiles.end(); it++) {
        RSLList *s = new RSLList;
        s->Add(new RSLLiteral(it->Name));
        if (it->Sources.empty() || (it->Sources.front().Protocol() == "file")) { // Local file
          std::string fsizechecksum = ".";
          if(it->FileSize != -1) fsizechecksum = tostring(it->FileSize)+fsizechecksum;
          if (!it->Checksum.empty()) fsizechecksum = fsizechecksum+it->Checksum;
          if(fsizechecksum == ".") fsizechecksum = "";
          s->Add(new RSLLiteral(fsizechecksum));
        } else {
          s->Add(new RSLLiteral(it->Sources.front().fullstr()));
        }
        if (!l) l = new RSLList;
        l->Add(new RSLSequence(s));
      }
      if (l) r.Add(new RSLCondition("inputfiles", RSLEqual, l));
      l = NULL;

      // Executables
      /// \mapattr executables <- InputFileType::IsExecutable
      for (std::list<InputFileType>::const_iterator it = j.DataStaging.InputFiles.begin();
           it != j.DataStaging.InputFiles.end(); it++) {
        if (it->IsExecutable) {
          if (!l) l = new RSLList;
          l->Add(new RSLLiteral(it->Name));
        }
      }
      if (l) r.Add(new RSLCondition("executables", RSLEqual, l));
      l = NULL;

      // outputfiles
      //   name url
      //   name void
      for (std::list<OutputFileType>::const_iterator it = j.DataStaging.OutputFiles.begin();
           it != j.DataStaging.OutputFiles.end(); it++) {
        if (it->Targets.empty() || (it->Targets.front().Protocol() == "file")) {
          // file to keep
          // normally must be no file:// here - just a protection
          RSLList *s = new RSLList;
          s->Add(new RSLLiteral(it->Name));
          s->Add(new RSLLiteral(""));
          if (!l) l = new RSLList;
          l->Add(new RSLSequence(s));
        } else {
          // file to stage
          RSLList *s = new RSLList;
          s->Add(new RSLLiteral(it->Name));
          s->Add(new RSLLiteral(it->Targets.front().fullstr()));
          if (!l) l = new RSLList;
          l->Add(new RSLSequence(s));
        }
      }
      if (l) r.Add(new RSLCondition("outputfiles", RSLEqual, l));
      l = NULL;

    } else { // dialect != "GRIDMANAGER"

      if (!j.DataStaging.InputFiles.empty() || !j.Application.Executable.Path.empty() || !j.Application.Input.empty()) {
        RSLList *l = NULL;
        for (std::list<InputFileType>::const_iterator it = j.DataStaging.InputFiles.begin();
             it != j.DataStaging.InputFiles.end(); it++) {
          RSLList *s = new RSLList;
          s->Add(new RSLLiteral(it->Name));
          if (it->Sources.empty()) {
            s->Add(new RSLLiteral(""));
          } else if (it->Sources.front().Protocol() == "file" && it->FileSize != -1) {
            s->Add(new RSLLiteral(it->Sources.front().Path()));
          }
          else {
            s->Add(new RSLLiteral(it->Sources.front().fullstr()));
          }
          if (!l) {
            l = new RSLList;
          }
          l->Add(new RSLSequence(s));
        }

        if (l) {
          r.Add(new RSLCondition("inputfiles", RSLEqual, l));
        }

        // Executables
        l = NULL;
        for (std::list<InputFileType>::const_iterator it = j.DataStaging.InputFiles.begin();
             it != j.DataStaging.InputFiles.end(); it++)
          if (it->IsExecutable) {
            if (!l) {
              l = new RSLList;
            }
            l->Add(new RSLLiteral(it->Name));
          }
        if (l) {
          r.Add(new RSLCondition("executables", RSLEqual, l));
        }
      }

      if (!j.DataStaging.OutputFiles.empty() || !j.Application.Output.empty() || !j.Application.Error.empty()) {
        RSLList *l = NULL;
        for (std::list<OutputFileType>::const_iterator it = j.DataStaging.OutputFiles.begin();
             it != j.DataStaging.OutputFiles.end(); it++) {
          if (!it->Targets.empty()) {
            RSLList *s = new RSLList;
            s->Add(new RSLLiteral(it->Name));
            if (!it->Targets.front() || it->Targets.front().Protocol() == "file")
              s->Add(new RSLLiteral(""));
            else {
              URL url(it->Targets.front());
              s->Add(new RSLLiteral(url.fullstr()));
            }
            if (!l) {
              l = new RSLList;
            }
            l->Add(new RSLSequence(s));
          }
          else {
            RSLList *s = new RSLList;
            s->Add(new RSLLiteral(it->Name));
            s->Add(new RSLLiteral(""));
            if (!l)
              l = new RSLList;
            l->Add(new RSLSequence(s));
          }
        }

        if (l) {
          r.Add(new RSLCondition("outputfiles", RSLEqual, l));
        }
      }
    } // (dialect == "GRIDMANAGER")

    /// \mapattr queue <- QueueName
    if (!j.Resources.QueueName.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Resources.QueueName));
      r.Add(new RSLCondition("queue", RSLEqual, l));
    }

    /// \mapattr rerun <- Rerun
    if (j.Application.Rerun != -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Application.Rerun)));
      r.Add(new RSLCondition("rerun", RSLEqual, l));
    }

    /// \mapattr priority <- Priority
    if (j.Application.Priority != -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Application.Priority)));
      r.Add(new RSLCondition("priority", RSLEqual, l));
    }

    /// TODO: dialect/units
    /// \mapattr lifetime <- SessionLifeTime 
    if (j.Resources.SessionLifeTime != -1) {
      RSLList *l = new RSLList;
      if(dialect == "GRIDMANAGER") {
        // Seconds
        l->Add(new RSLLiteral(tostring(j.Resources.SessionLifeTime.GetPeriod())));
      } else {
        // Free format
        l->Add(new RSLLiteral((std::string)j.Resources.SessionLifeTime));
      }
      r.Add(new RSLCondition("lifetime", RSLEqual, l));
    }

    /// \mapattr disk <- DiskSpace
    if (j.Resources.DiskSpaceRequirement.DiskSpace > -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Resources.DiskSpaceRequirement.DiskSpace)));
      r.Add(new RSLCondition("disk", RSLEqual, l));
    }

    /// \mapattr runtimeenvironment <- RunTimeEnvironment
    if (!j.Resources.RunTimeEnvironment.empty()) {
      std::list<Software>::const_iterator itSW = j.Resources.RunTimeEnvironment.getSoftwareList().begin();
      std::list<Software::ComparisonOperator>::const_iterator itCO = j.Resources.RunTimeEnvironment.getComparisonOperatorList().begin();
      for (; itSW != j.Resources.RunTimeEnvironment.getSoftwareList().end(); itSW++, itCO++) {
        RSLList *l = new RSLList;
        l->Add(new RSLLiteral(*itSW));
        r.Add(new RSLCondition("runtimeenvironment", convertOperator(*itCO), l));
      }
    }

    /// \mapattr middleware <- CEType
    if (!j.Resources.CEType.empty()) {
      std::list<Software>::const_iterator itSW = j.Resources.CEType.getSoftwareList().begin();
      std::list<Software::ComparisonOperator>::const_iterator itCO = j.Resources.CEType.getComparisonOperatorList().begin();
      for (; itSW != j.Resources.CEType.getSoftwareList().end(); itSW++, itCO++) {
        RSLList *l = new RSLList;
        l->Add(new RSLLiteral(*itSW));
        r.Add(new RSLCondition("middleware", convertOperator(*itCO), l));
      }
    }

    /// \mapattr opsys <- OperatingSystem
    if (!j.Resources.OperatingSystem.empty()) {
      std::list<Software>::const_iterator itSW = j.Resources.OperatingSystem.getSoftwareList().begin();
      std::list<Software::ComparisonOperator>::const_iterator itCO = j.Resources.OperatingSystem.getComparisonOperatorList().begin();
      for (; itSW != j.Resources.OperatingSystem.getSoftwareList().end(); itSW++, itCO++) {
        RSLList *l = new RSLList;
        l->Add(new RSLLiteral((std::string)*itSW));
        r.Add(new RSLCondition("opsys", convertOperator(*itCO), l));
      }
    }

    /// \mapattr architecture <- Platform
    if (!j.Resources.Platform.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Resources.Platform));
      r.Add(new RSLCondition("architecture", RSLEqual, l));
    }

    /// \mapattr count <- NumberOfSlots
    if (j.Resources.SlotRequirement.NumberOfSlots > -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Resources.SlotRequirement.NumberOfSlots)));
      r.Add(new RSLCondition("count", RSLEqual, l));
    }

    /// \mapattr countpernode <- SlotsPerHost
    if (j.Resources.SlotRequirement.SlotsPerHost > -1) {
      if (j.Resources.SlotRequirement.NumberOfSlots <= -1) {
        logger.msg(ERROR, "Cannot output XRSL representation: The Resources.SlotRequirement.NumberOfSlots attribute must be specified when the Resources.SlotRequirement.SlotsPerHost attribute is specified.");
        return false;
      }
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(tostring(j.Resources.SlotRequirement.SlotsPerHost)));
      r.Add(new RSLCondition("countpernode", RSLEqual, l));
    }

    /// \mapattr exclusiveexecution <- ExclusiveExecution
    if (j.Resources.SlotRequirement.ExclusiveExecution != SlotRequirementType::EE_DEFAULT) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Resources.SlotRequirement.ExclusiveExecution == SlotRequirementType::EE_TRUE ? "yes" : "no"));
      r.Add(new RSLCondition("exclusiveexecution", RSLEqual, l));
    }

    /// \mapattr starttime <- ProcessingStartTime
    if (j.Application.ProcessingStartTime != -1) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.ProcessingStartTime.str(MDSTime)));
      r.Add(new RSLCondition("starttime", RSLEqual, l));
    }

    /// \mapattr gmlog <- LogDir
    if (!j.Application.LogDir.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.LogDir));
      r.Add(new RSLCondition("gmlog", RSLEqual, l));
    }

    /// \mapattr jobname <- JobName
    if (!j.Identification.JobName.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Identification.JobName));
      r.Add(new RSLCondition("jobname", RSLEqual, l));
    }

    /// \mapattr acl <- AccessControl
    if (j.Application.AccessControl) {
      RSLList *l = new RSLList;
      std::string acl;
      j.Application.AccessControl.GetXML(acl, false);
      l->Add(new RSLLiteral(acl));
      r.Add(new RSLCondition("acl", RSLEqual, l));
    }

    /// TODO \mapattr notify <- ApplicationType::Notification
    if (!j.Application.Notification.empty()) {
      RSLList *l = new RSLList;
      for (std::list<NotificationType>::const_iterator it = j.Application.Notification.begin();
           it != j.Application.Notification.end(); it++) {
        // Suboptimal, group emails later
        std::string states;
        for (std::list<std::string>::const_iterator s = it->States.begin();
             s != it->States.end(); s++) {
          char state = StateToShortcut(*s);
          if (state != ' ') states+=state;
        }
        l->Add(new RSLLiteral(states + " " + it->Email));
      }
      r.Add(new RSLCondition("notify", RSLEqual, l));
    }

    /// TODO \mapattr jobreport <- RemoteLogging
    if (!j.Application.RemoteLogging.empty()) {
      // Pick first SGAS remote logging service.
      for (std::list<RemoteLoggingType>::const_iterator it = j.Application.RemoteLogging.begin();
           it != j.Application.RemoteLogging.end(); ++it) {
        if (it->ServiceType == "SGAS") {
          // Ignoring the optional attribute.
          RSLList *l = new RSLList;
          l->Add(new RSLLiteral(it->Location.str()));
          r.Add(new RSLCondition("jobreport", RSLEqual, l));
          break;
        }
      }
    }

    /// \mapattr credentialserver <- CredentialService
    if (!j.Application.CredentialService.empty()) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(j.Application.CredentialService.front().fullstr()));
      r.Add(new RSLCondition("credentialserver", RSLEqual, l));
    }

    /// \mapattr dryrun <- DryRun
    if (j.Application.DryRun) {
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral("yes"));
      r.Add(new RSLCondition("dryrun", RSLEqual, l));
    }

    /// TODO \mapnote 
    for (std::map<std::string, std::string>::const_iterator it = j.OtherAttributes.begin();
         it != j.OtherAttributes.end(); it++) {
      std::list<std::string> keys;
      tokenize(it->first, keys, ";");
      if (keys.size() != 2 || keys.front() != "nordugrid:xrsl") {
        continue;
      }
      if (keys.back() == "action" && dialect != "GRIDMANAGER") {
        // Dont put the action attribute into non GRIDMANAGER xRSL
        continue;
      }
      RSLList *l = new RSLList;
      l->Add(new RSLLiteral(it->second));
      r.Add(new RSLCondition(keys.back(), RSLEqual, l));
    }

    std::stringstream ss;
    ss << r;
    product = ss.str();
    return true;
  }

} // namespace Arc
