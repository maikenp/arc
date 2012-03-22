#! /usr/bin/env python
import arc
import sys
import os
import random

# arc.Logger.getRootLogger().addDestination(arc.LogStream(sys.stderr))
# arc.Logger.getRootLogger().setThreshold(arc.DEBUG)

def example():
    # Creating a UserConfig object with the user's proxy
    # and the path of the trusted CA certificates
    uc = arc.UserConfig()
    uc.ProxyPath("/tmp/proxy")
    uc.CACertificatesDirectory("/tmp/certificates")

    # Creating an endpoint for a Computing Element
    endpoint = arc.Endpoint("piff.hep.lu.se", arc.Endpoint.COMPUTINGINFO, "org.nordugrid.ldapglue2")

    # Get the ExecutionTargets of this ComputingElement
    retriever = arc.ComputingServiceRetriever(uc, [endpoint])
    retriever.wait()
    targets = retriever.GetExecutionTargets()    

    # Shuffle the targets to simulate a random broker
    targets = list(targets)
    random.shuffle(targets)
    
    # Create a JobDescription
    jobdesc = arc.JobDescription()
    jobdesc.Application.Executable.Path = "/bin/hostname"
    jobdesc.Application.Output = "stdout.txt"

    # create an empty job object which will contain our submitted job
    job = arc.Job()
    success = False;
    # Submit job directly to the execution targets, without a broker
    for target in targets:
        print "Trying to submit to", target.ComputingEndpoint.URLString, "(%s)" % target.ComputingEndpoint.InterfaceName, "...",
        sys.stdout.flush()
        success = target.Submit(uc, jobdesc, job)
        if success:
            print "succeeded!"
            break
        else:
            print "failed!"
    if success:
        print "Job was submitted:"
        job.SaveToStream(arc.CPyOstream(sys.stdout), False)
    else:
        print "Job submission failed"
    
# run the example and catch all Exceptions in order to make sure we can call _exit() at the end
try:
    example()
except:
    import traceback
    traceback.print_exc()
    os._exit(1)

# _exit() is needed to avoid segmentation faults by still running background threads
os._exit(0)
