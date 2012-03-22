#include <cppunit/extensions/HelperMacros.h>

#include <stdlib.h>

#include <arc/URL.h>
#include <arc/UserConfig.h>
#include <arc/Utils.h>
#include <arc/client/Job.h>
#include <arc/client/Submitter.h>
#include <arc/Thread.h>

class SubmitterTest
  : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SubmitterTest);
  CPPUNIT_TEST(LoadTest);
  CPPUNIT_TEST_SUITE_END();

public:
  SubmitterTest();
  ~SubmitterTest() { delete sl; }

  void setUp() {}
  void tearDown() { Arc::ThreadInitializer().waitExit(); }

  void LoadTest();

private:
  Arc::Submitter *s;
  Arc::SubmitterLoader *sl;
  Arc::UserConfig usercfg;
};

SubmitterTest::SubmitterTest() : s(NULL), usercfg(Arc::initializeCredentialsType(Arc::initializeCredentialsType::SkipCredentials)) {
  sl = new Arc::SubmitterLoader();
}

void SubmitterTest::LoadTest()
{
  s = sl->load("", usercfg);
  CPPUNIT_ASSERT(s == NULL);

  s = sl->load("NON-EXISTENT", usercfg);
  CPPUNIT_ASSERT(s == NULL);

  s = sl->load("TEST", usercfg);
  CPPUNIT_ASSERT(s != NULL);
}

CPPUNIT_TEST_SUITE_REGISTRATION(SubmitterTest);
