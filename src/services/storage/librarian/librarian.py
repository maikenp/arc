import urlparse
import httplib
import arc
import random
import threading
import time

from arcom import get_child_nodes, get_child_values_by_name
from arcom.security import parse_ssl_config
from arcom.service import librarian_uri, ahash_servicetype, true, false, parse_node, create_response, node_to_data, librarian_servicetype

from arcom.xmltree import XMLTree
from storage.client import AHashClient, ISISClient
from storage.common import global_root_guid, sestore_guid, ahash_list_guid, parse_metadata, create_metadata, serialize_ids, deserialize_ids
from storage.common import OFFLINE, DELETED
import traceback
import copy

from arcom.logger import Logger
log = Logger(arc.Logger(arc.Logger_getRootLogger(), 'Storage.Librarian'))

class Librarian:
    
    def __init__(self, cfg, ssl_config, service_state):
        self.service_state = service_state
        self.ssl_config = ssl_config
        try:
            self.period = float(str(cfg.Get('CheckPeriod')))
            self.hbtimeout = float(str(cfg.Get('HeartbeatTimeout')))
        except:
            log.msg(arc.ERROR, 'Cannot find CheckPeriod, HeartbeatTimeout in the config')
            raise Exception, 'Librarian cannot run without CheckPeriod and HeartbeatTimeout'
        log.msg(arc.INFO,'Getting AHash URL from the config')
        ahash_urls =  get_child_values_by_name(cfg, 'AHashURL')
        self.SEs = {}
        if ahash_urls:
            log.msg(arc.INFO,'Got AHash URLs:', ahash_urls)
            log.msg(arc.VERBOSE, 'AHash URL found in the configuration.')
            self.master_ahash = arc.URL(ahash_urls[0])
            self.ahash_reader = AHashClient(ahash_urls, ssl_config = self.ssl_config)
            self.ahash_writer = AHashClient(ahash_urls, ssl_config = self.ssl_config)
            self.thread_is_running = True
            log.msg(arc.INFO,'Starting checking thread')
            threading.Thread(target = self.checkingThread).start()
            log.msg(arc.INFO,'Setting running state to True')
            self.service_state.running = True
        else:
            log.msg(arc.INFO,'No AHash from the config')
            isis_urls =  get_child_values_by_name(cfg, 'ISISURL')
            if not isis_urls:
	            log.msg(arc.ERROR, "AHash URL and ISIS URL not found in the configuration.")
	            raise Exception, 'Librarian cannot run with no A-Hash'
            log.msg(arc.INFO,'Got ISIS URL, starting initThread')
            self.thread_is_running = True
            threading.Thread(target = self.initThread, args = [isis_urls]).start()
        
    def initThread(self, isis_urls):
        found_ahash = False
        while not found_ahash:
            try:
                time.sleep(3)
                log.msg(arc.INFO,'Trying to get A-Hash from ISISes')
                for isis_url in isis_urls:
                    if not self.thread_is_running:
                        return
                    log.msg(arc.INFO,'Trying to get A-Hash from', isis_url)
                    isis = ISISClient(isis_url, ssl_config = self.ssl_config)
                    try:
                        ahash_urls = isis.getServiceURLs(ahash_servicetype)
                        log.msg(arc.INFO,'Got A-Hash from ISIS:', ahash_urls)
                        if ahash_urls:
                            self.master_ahash = arc.URL(ahash_urls[0])
                            self.ahash_reader = AHashClient(ahash_urls, ssl_config = self.ssl_config)
                            self.ahash_writer = AHashClient(ahash_urls, ssl_config = self.ssl_config)
                            found_ahash = True
                    except:
                        log.msg()
            except Exception, e:
                log.msg(arc.WARNING, 'Error in initThread: %s' % e)
        log.msg(arc.INFO,'initThread finishes, starting checkingThread')
        threading.Thread(target = self.checkingThread).start()
        self.service_state.running = True

    def _update_ahash_urls(self):
        try:
            ahash_list = self.ahash_reader.get(ahash_list_guid)[ahash_list_guid]
            if ahash_list:
                master = [url for type,url in ahash_list.items() 
                          if 'master' in type and type[1].endswith('online')]
                clients = [url for type,url in ahash_list.items() 
                           if 'client' in type and type[1].endswith('online')]
                if master:
                    self.master_ahash = arc.URL(master[0])
                    self.ahash_writer.urls = [arc.URL(url) for url in master]
                    self.ahash_writer.reset()
                if clients:
                    self.ahash_reader.urls = [arc.URL(url) for url in clients]
                    self.ahash_reader.reset()
        except:
            pass
        
    def ahash_get_tree(self, IDs, neededMetadata = []):
        """
        Wrapper for ahash.get_tree updating ahashes and calling ahash.get_tree
        """
        try:
            ret = self.ahash_reader.get_tree(IDs, neededMetadata)
            return ret
        except:
            # if get_tree fails, there are no clients available, so there is
            # nothing to do here
            #log.msg()
            raise

    def ahash_get(self, IDs, neededMetadata = []):
        """
        Wrapper for ahash.get updating ahashes and calling ahash.get
        """
        try:
            ret = self.ahash_reader.get(IDs, neededMetadata)
            return ret
        except:
            # if get fails, there are no clients available, so there is
            # nothing to do here
            #log.msg()
            raise

    def ahash_change(self, changes):
        """
        Wrapper for ahash.change replacing ahash.urls with master ahash, calling
        ahash.change, putting back ahash.urls
        ahash.change can only call master ahash
        """
        ret = None
        try:
            ret = self.ahash_writer.change(changes)
            return ret
        except:
            # put back all known ahashes so we don't only ask the master
            # for an update
            ahash_reader_urls = self.ahash_reader.urls
            ahash_writer_urls = self.ahash_writer.urls
            # retry, updating ahash urls in case master is outdated
            self._update_ahash_urls()
            try:
                ret = self.ahash_writer.change(changes)
                return ret
            except:
                # make sure ahash.urls are restored even is change failed
                self.ahash_reader.urls = ahash_reader_urls
                self.ahash_reader.reset()
                self.ahash_writer.urls = ahash_writer_urls
                self.ahash_writer.reset()
                raise
    
    def checkingThread(self):
        time.sleep(10)
        while self.thread_is_running:
            try:
                SEs = self.ahash_get([sestore_guid])[sestore_guid]
                # store it for use in 'get'
                self.SEs = SEs
                #print 'registered shepherds:', SEs
                now = time.time()
                late_SEs = [serviceID for (serviceID, property), nextHeartbeat in SEs.items() if property == 'nextHeartbeat' and float(nextHeartbeat) < now and nextHeartbeat != '-1']
                #print 'late shepherds (it is %s now)' % now, late_SEs
                if late_SEs:
                    serviceGUIDs = dict([(serviceGUID, serviceID) for (serviceID, property), serviceGUID in SEs.items() if property == 'serviceGUID' and serviceID in late_SEs])
                    #
                    # Don't set the replicas to offline!
                    #
                    ##print 'late shepherds serviceGUIDs', serviceGUIDs
                    #filelists = self.ahash_get(serviceGUIDs.keys())
                    #changes = []
                    #for serviceGUID, serviceID in serviceGUIDs.items():
                        #filelist = filelists[serviceGUID]
                        ##print 'filelist of late shepherd', serviceID, filelist
                        #changes.extend([(GUID, serviceID, referenceID, OFFLINE)
                            #for (_, referenceID), GUID in filelist.items()])
                    #change_response = self._change_states(changes)
                    for _, serviceID in serviceGUIDs.items():
                        self._set_next_heartbeat(serviceID, -1)
                    self.SEs = self.ahash_get([sestore_guid])[sestore_guid]
                time.sleep(self.period)
            except Exception, e:
                log.msg(arc.ERROR, "Error in Librarian's checking thread: %s" % e)
                #log.msg()
            # update list of ahashes
            self._update_ahash_urls()
            time.sleep(self.period)

    def _change_states(self, changes):
        # we got a list of (GUID, serviceID, referenceID, state) - where GUID is of the file,
        #   serviceID is of the shepherd, referenceID is of the replica within the shepherd, and state is of this replica
        # we need to put the serviceID, referenceID pair into one string (a location)
        with_locations = [(GUID, serialize_ids([serviceID, referenceID]), state)
            for GUID, serviceID, referenceID, state in changes]
        # we will ask the librarian for each file to modify the state of this location of this file (or add this location if it was not already there)
        # if state == DELETED this location will be removed
        # but only if the file itself exists
        change_request = dict([
            (location, (GUID, (state != DELETED) and 'set' or 'unset', 'locations', location, state,
                {'only if file exists' : ('is', 'entry', 'type', 'file')}))
                    for GUID, location, state in with_locations
        ])
        #print '_change_states request', change_request
        change_response = self.ahash_change(change_request)
        #print '_change_states response', change_response
        return change_response
        
    def _set_next_heartbeat(self, serviceID, next_heartbeat):
        ahash_request = {'report' : (sestore_guid, 'set', serviceID, 'nextHeartbeat', next_heartbeat, {})}
        #print '_set_next_heartbeat request', ahash_request
        ahash_response = self.ahash_change(ahash_request)
        #print '_set_next_heartbeat response', ahash_response
        if ahash_response['report'][0] != 'set':
            log.msg(arc.VERBOSE, 'ERROR setting next heartbeat time!')
    
    def report(self, serviceID, filelist):
        try:
            # we got the ID of the shepherd service, and a filelist which contains (GUID, referenceID, state) tuples
            # here we get the list of registered services from the A-Hash (stored with the GUID 'sestore_guid')
            ses = self.ahash_get([sestore_guid])[sestore_guid]
            # refresh the cached version for use in get
            self.SEs = ses
            # we get the GUID of the shepherd
            serviceGUID = ses.get((serviceID,'serviceGUID'), None)
            # or this shepherd was not registered yet
            if not serviceGUID:
                # print 'report se is not registered yet', serviceID
                # let's create a new GUID
                serviceGUID = arc.UUID()
                # let's add the new shepherd-GUID to the list of shepherd-GUIDs but only if someone else not done that just now
                ahash_request = {'report' : (sestore_guid, 'set', serviceID, 'serviceGUID', serviceGUID, {'onlyif' : ('unset', serviceID, 'serviceGUID', '')})}
                ## print 'report ahash_request', ahash_request
                ahash_response = self.ahash_change(ahash_request)
                ## print 'report ahash_response', ahash_response
                success, unmetConditionID = ahash_response['report']
                # if there was already a GUID for this service (which should have been created after we check it first but before we tried to create a new)
                if unmetConditionID:
                    # let's try to get the GUID of the shepherd once again
                    ses = self.ahash_get([sestore_guid])[sestore_guid]
                    serviceGUID = ses.get((serviceID, 'serviceGUID'))
            # if the next heartbeat time of this shepherd is -1 or nonexistent, 
            # then we suspect that we don't have everything
            # so we will send the list of files we think the shepherd have
            suspicious = int(ses.get((serviceID, 'nextHeartbeat'), -1)) == -1
            # calculate the next heartbeat time
            next_heartbeat = str(int(time.time() + self.hbtimeout))
            # register the next heartbeat time for this shepherd
            self._set_next_heartbeat(serviceID, next_heartbeat)
            # prepare to change the states of replicas in the metadata of the files to the just now reported values
            states_to_change = [(GUID, serviceID, referenceID, state) for GUID, referenceID, state in filelist if referenceID != '<NOTFOUND>']
            ## get the metadata of the shepherd
            # se = self.ahash_get([serviceGUID])[serviceGUID]
            ## print 'report se before:', se
            # we want to know which files this shepherd stores, that's why we collect the GUIDs and referenceIDs of all the replicas the shepherd reports
            # we store this information in the A-Hash by the GUID of this shepherd (serviceGUID)
            # so this request asks the A-Hash to store the referenceID and GUID of this file in the section called 'file' in the A-Hash entry of the Shepherd
            # but if the shepherd reports that a replica is DELETED then it will be removed from this list (unset)
            #print "Librarian got filelist from Shepherd", filelist
            # this files where not found on the shepherd
            notfounds = [guid for (guid, referenceID, _) in filelist if referenceID == '<NOTFOUND>']
            #print "These files were not found by the Shepherd", notfounds
            # this files have a change in their states
            change_request_list = [(referenceID, (serviceGUID, (state == DELETED) and 'unset' or 'set', 'file', referenceID, GUID, {})) for (GUID, referenceID, state) in filelist if referenceID != '<NOTFOUND>']
            # we have to figure out the referenceIDs of the notfound files in order to delete them
            if notfounds:
                if '<IMBACK>' in notfounds:
                    notfounds.remove('<IMBACK>')
                    suspicious = True
                try:
                    se = self.ahash_get([serviceGUID])[serviceGUID]
                    for (_, ref), gu in se.items():
                        if gu in notfounds:
                            change_request_list.append((ref, (serviceGUID, 'unset', 'file', ref, gu, {})))
                            states_to_change.append((gu, serviceID, ref, DELETED))
                except:
                    log.msg()
                    pass
            ## print 'report change_request:', change_request
            #print "These are the requested file changes", dict(change_request_list)
            if change_request_list:
                change_response = self.ahash_change(dict(change_request_list))
            # TODO: check the response and do something if something is wrong
            ## print 'report change_response:', change_response
            # se = self.ahash_get([serviceGUID])[serviceGUID]
            ## print 'report se after:', se
            # if we want the shepherd to send the state of all the files, not just the changed one, we return -1 as the next heartbeat's time
            if states_to_change:
                self._change_states(states_to_change)
        except:
            log.msg()
            log.msg(arc.ERROR, 'Error processing report message')
            suspicious = True
        if suspicious:
            try:
                ses = self.ahash_get([sestore_guid])[sestore_guid]
                # refresh the cached version for use in 'get'
                self.SEs = ses
                serviceGUID = ses[(serviceID,'serviceGUID')]
                se = self.ahash_get([serviceGUID])[serviceGUID]
                return -1, se.values()
            except:
                return -1, {}
        else:
            return int(self.hbtimeout), {}

    def new(self, requests):
        response = {}
        for rID, metadata in requests.items():
            #print 'Processing new request:', metadata
            try:
                type = metadata[('entry','type')]
                del metadata[('entry', 'type')]
            except:
                type = None
            if type is None:
                success = 'failed: no type given'
            else:
                try:
                    GUID = metadata[('entry','GUID')]
                except:
                    GUID = arc.UUID()
                    metadata[('entry', 'GUID')] = GUID
              
                check = self.ahash_change(
                      {'new': (GUID, 'set', 'entry', 'type', type, {'0' : ('unset','entry','type','')})}
                    )
                
                status, failedCondition = check['new']
                if status == 'set':
                    success = 'success'
                    changeID = 0
                    changes = {}
                    for ((section, property), value) in metadata.items():
                        changes[changeID] = (GUID, 'set', section, property, value, {})
                        changeID += 1
                    resp = self.ahash_change(changes)
                    for r in resp.keys():
                        if resp[r][0] != 'set':
                            success += ' (failed: %s - %s)' % (resp[r][0] + str(changes[r]))
                elif failedCondition == '0':
                    success = 'failed: entry exists'
                else:
                    success = 'failed: ' + status
            response[rID] = (GUID, success)
        return response

    def get(self, requests, neededMetadata = []):
        tree = self.ahash_get_tree(requests, neededMetadata)
        try:
            xml = arc.XMLNode(arc.NS({'lbr':librarian_uri}), 'lbr:getResponseList')
            tree.add_to_node(xml,'//')
            for response_node in get_child_nodes(xml):
                for m in get_child_nodes(response_node.Get('metadataList')):
                    if str(m.Get('section')) == 'locations':
                        serviceID, _ = deserialize_ids(str(m.Get('property')))
                        if self.SEs.get((serviceID, 'nextHeartbeat'),'-1') == '-1':
                            # the SE seems to be offline
                            #print serviceID, 'is offline, changing state', m.GetXML()
                            m.Get('value').Set(OFFLINE)
            tree = XMLTree(xml)
        except:
            log.msg()
        return tree

    def _parse_LN(self, LN):
        try:
            splitted = LN.split('/')
        except:
            raise Exception, 'Invalid Logical Name: `%s`' % LN
        guid = splitted[0]
        path = splitted[1:]
        return guid, path

    def _traverse(self, guid, metadata, path, traversed, GUIDs):
            try:
                while path:
                    if path[0] in ['', '.']:
                        child_guid = guid
                        child_metadata = metadata
                    else:
                        child_guid = metadata[('entries',path[0])]
                        child_metadata = self.ahash_get([child_guid])[child_guid]
                    traversed.append(path.pop(0))
                    GUIDs.append(child_guid)
                    guid = child_guid
                    metadata = child_metadata
                return metadata
            except KeyError:
                return metadata
            except Exception, e:
                log.msg(arc.ERROR, 'Error traversing: %s' % e)
                #log.msg()
                return {}


    def traverseLN(self, requests):
        response = {}
        for rID, LN in requests.items():
            guid0, path0 = self._parse_LN(LN)
            if not guid0:
                guid = global_root_guid
            else:
                guid = guid0
            traversed = [guid0]
            GUIDs = [guid]
            path = copy.deepcopy(path0)
            try:
                metadata0 = self.ahash_get([guid])[guid]
            except:
                # TODO: figure out this part
                raise
                metadata0 = {}
            if not metadata0.has_key(('entry','type')):
                response[rID] = ([], False, '', guid0, None, '/'.join(path))
            else:
                try:
                    metadata = self._traverse(guid, metadata0, path, traversed, GUIDs)
                    # TODO: this is a duplication of the logic in 'get'
                    for (section, prop) in metadata.keys():
                        if section == 'locations':
                            serviceID, _ = deserialize_ids(prop)
                            if self.SEs.get((serviceID, 'nextHeartbeat'),'-1') == '-1':
                                # the SE seems to be offline
                                #print serviceID, 'is offline, changing state', section, prop
                                metadata[(section, prop)] = OFFLINE
                    traversedList = [(traversed[i], GUIDs[i]) for i in range(len(traversed))]
                    wasComplete = len(path) == 0
                    traversedLN = guid0 + '/' + '/'.join(traversed[1:])
                    GUID = GUIDs[-1]
                    restLN = '/'.join(path)
                    response[rID] = (traversedList, wasComplete, traversedLN, GUID, metadata, restLN)
                except Exception, e:
                    log.msg(arc.ERROR, "Error in traverseLN method: %s" % s)
                    #log.msg()
                    response[rID] = ([], False, '', guid0, None, '/'.join(path))
            #print '?\n? traversedList, wasComplete, traversedLN, GUID, metadata, restLN\n? ', response
        return response

    def modifyMetadata(self, requests):
        changes = {}
        for changeID, (GUID, changeType, section, property, value) in requests.items():
            if changeType in ['set', 'unset']:
                conditions = {}
            elif changeType == 'add':
                changeType = 'set'
                conditions = {'0': ('unset', section, property, '')}
            elif changeType.startswith('setifvalue='):
                ifvalue = changeType[11:]
                changeType = 'set'
                conditions = {'0': ('is', section, property, ifvalue)}
            else:
                continue
            changes[changeID] = (GUID, changeType, section, property, value, conditions)
        ahash_response = self.ahash_change(changes)
        response = {}
        for changeID, (success, conditionID) in ahash_response.items():
            if success in ['set', 'unset']:
                response[changeID] = success
            elif conditionID == '0':
                response[changeID] = 'condition failed'
            else:
                response[changeID] = 'failed: ' + success
        return response

    def remove(self, requests):
        ahash_request = dict([(requestID, (GUID, 'delete', '', '', '', {}))
            for requestID, GUID in requests.items()])
        ahash_response = self.ahash_change(ahash_request)
        response = {}
        for requestID, (success, _) in ahash_response.items():
            if success == 'deleted':
                response[requestID] = 'removed'
            else:
                response[requestID] = 'failed: ' + success
        return response
    
from arcom.service import Service
    
class LibrarianService(Service):
    """ LibrarianService class implementing the XML interface of the storage Librarian service. """

    def __init__(self, cfg):
        """ Constructor of the LibrarianService

        LibrarianService(cfg)

        'cfg' is an XMLNode which containes the config of this service.
        """
        self.service_name = 'Librarian'
        # init logging
        # names of provided methods
        request_names = ['new','get','traverseLN', 'modifyMetadata', 'remove', 'report']
        # call the Service's constructor
        Service.__init__(self, [{'request_names' : request_names, 'namespace_prefix': 'lbr', 'namespace_uri': librarian_uri}], cfg, start_service = False)
        # this causes trouble on shutdown (the Librarian class would have a reference to the LibrarianService class, so the destructor would not be called)
        #ssl_config['get_trusted_dns_method'] = self._get_trusted_dns
        self.librarian = Librarian(cfg, self.ssl_config, self.state)
    
    def __del__(self):
        try:
            self.librarian.thread_is_running = False
        except:
            pass
        Service.__del__(self)
    
    def new(self, inpayload):
        requests0 = parse_node(inpayload.Child().Child(),
            ['requestID', 'metadataList'], single = True, string = False)
        requests = dict([(str(requestID), parse_metadata(metadataList)) 
            for requestID, metadataList in requests0.items()])
        resp = self.librarian.new(requests)
        return create_response('lbr:new',
            ['lbr:requestID', 'lbr:GUID', 'lbr:success'], resp, self._new_soap_payload())

    def get(self, inpayload):
        requests = [str(node.Get('GUID')) for node in get_child_nodes(inpayload.Child().Get('getRequestList'))]
        neededMetadata = [
            node_to_data(node, ['section', 'property'], single = True)
                for node in get_child_nodes(inpayload.Child().Get('neededMetadataList'))
        ]
        tree = self.librarian.get(requests, neededMetadata)
        out = arc.PayloadSOAP(self._new_soap_payload())
        response_node = out.NewChild('lbr:getResponse')
        tree.add_to_node(response_node)
        return out

    def traverseLN(self, inpayload):
        # if inpayload.auth:
        #     print 'Librarian auth "traverseLN": ', inpayload.auth
        requests = parse_node(inpayload.Child().Child(), ['requestID', 'LN'], single = True)
        response = self.librarian.traverseLN(requests)
        for rID, (traversedList, wasComplete, traversedLN, GUID, metadata, restLN) in response.items():
            traversedListTree = [
                ('lbr:traversedListElement', [
                    ('lbr:LNPart', LNpart),
                    ('lbr:GUID', partGUID)
                ]) for (LNpart, partGUID) in traversedList
            ]
            metadataTree = create_metadata(metadata, 'lbr')
            response[rID] = (traversedListTree, wasComplete and true or false,
                traversedLN, GUID, metadataTree, restLN)
        return create_response('lbr:traverseLN',
            ['lbr:requestID', 'lbr:traversedList', 'lbr:wasComplete',
                'lbr:traversedLN', 'lbr:GUID', 'lbr:metadataList', 'lbr:restLN'], response, self._new_soap_payload())

    def modifyMetadata(self, inpayload):
        requests = parse_node(inpayload.Child().Child(), ['lbr:changeID',
            'lbr:GUID', 'lbr:changeType', 'lbr:section', 'lbr:property', 'lbr:value'])
        response = self.librarian.modifyMetadata(requests)
        return create_response('lbr:modifyMetadata', ['lbr:changeID', 'lbr:success'],
            response, self._new_soap_payload(), single = True)

    def remove(self, inpayload):
        requests = parse_node(inpayload.Child().Child(), ['lbr:requestID', 'lbr:GUID'], single = True)
        response = self.librarian.remove(requests)
        return create_response('lbr:remove', ['lbr:requestID', 'lbr:success'],
            response, self._new_soap_payload(), single = True)
    
    def report(self, inpayload):
        request_node = inpayload.Child()
        serviceID = str(request_node.Get('serviceID'))
        filelist_node = request_node.Get('filelist')
        file_nodes = get_child_nodes(filelist_node)
        filelist = [(str(node.Get('GUID')), str(node.Get('referenceID')), str(node.Get('state'))) for node in file_nodes]
        nextReportTime, all_files = self.librarian.report(serviceID, filelist)
        out = self._new_soap_payload()
        response_node = out.NewChild('lbr:registerResponse')
        response_node.NewChild('lbr:nextReportTime').Set(str(nextReportTime))
        allfile_node = response_node.NewChild('lbr:allFiles')
        for guid in all_files:
            allfile_node.NewChild('lbr:GUID').Set(str(guid))
        return out

    def RegistrationCollector(self, doc):
        regentry = arc.XMLNode('<RegEntry />')
        regentry.NewChild('SrcAdv').NewChild('Type').Set(librarian_servicetype)
        #Place the document into the doc attribute
        doc.Replace(regentry)
        return True

    def GetAdditionalLocalInformation(self, service_node):
        service_node.NewChild('Type').Set(librarian_servicetype)

