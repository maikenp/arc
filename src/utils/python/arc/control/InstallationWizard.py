#! /usr/bin/env python
from ControlCommon import *
from TestCA import TestCAControl
from ThirdPartyDeployment import ThirdPartyControl

import os, sys, re
import subprocess
import argparse
from jinja2 import Environment, FileSystemLoader
import yaml
from arc.utils import config

import errno
import datetime


lrmses = ['fork','slurm','condor','pbs','sge','lfs','ll','boinc','dbridge']


def run_command(cmd):

    proc = subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell=True)
    stdout, stderr = proc.communicate()

    return stdout, stderr



class InstallationWizardControl(ComponentControl):

    def __init__(self, arcconfig):

        self.logger = logging.getLogger('ARCCTL.InstallationWizard')

        self.defaults = (get_parsed_arcconf(config.defaults_defpath())).get_config_dict()


        self.control_dir = None
        self.arcconfig = arcconfig
        self.create_host_cert = False
     
        self.descr = {}
        self.descr['host_cert'] = 'hostcert path'
        self.descr['host_key'] = 'hostkey path'
        self.descr['hostname'] = 'hostname'
        
        self.descr['conf_path'] = 'arc.conf path'
        self.descr['log_rootdir'] = 'logfile path'
        self.descr['controldir'] = 'controldir'
        self.descr['sessiondir'] = 'sessiondir'
        self.descr['grid_security_path'] = 'grid-security folder'

        self.descr['lrms_type'] = 'lrms type'
        self.descr['lrms_path'] = 'lrms bin path'

        self.descr['grid_user'] = 'linux grid user'
        self.descr['grid_group'] = 'linux grid group'

        self.descr['enable_emies'] = 'enable emies submission'
        self.descr['enable_gridftpd'] = 'enable gridftpd submission'


        self.confdict = {}
        self.confdict['timestamp']=None
        self.confdict['conf_path'] = '/etc/arc.conf'

        self.confdict['host_cert'] = self.defaults['common']['x509_host_cert']
        self.confdict['host_key'] = self.defaults['common']['x509_host_key']

        self.confdict['hostname'] = self.defaults['common']['hostname']
    
        logdir = '/'.join(self.defaults['arex']['logfile'].split('/')[:-1])
        self.confdict['log_rootdir'] = logdir
        self.confdict['controldir'] = self.defaults['arex']['controldir']
        self.confdict['sessiondir'] = self.defaults['arex']['sessiondir']

        self.confdict['grid_security_path'] = '/etc/grid-security'

        self.confdict['lrms_type'] = self.defaults['lrms']['lrms']
        self.confdict['lrms_path'] = '/usr/bin'

        self.confdict['lrms_condor_config'] = self.defaults['lrms']['condor_config']

        self.confdict['lrms_sge_root'] = self.defaults['lrms']['sge_root']

        self.confdict['lrms_boinc_db_host'] = self.defaults['lrms']['boinc_db_host']
        self.confdict['lrms_boinc_db_port'] = self.defaults['lrms']['boinc_db_port']
        self.confdict['lrms_boin_db_user'] = ''
        self.confdict['lrms_boinc_db_pass'] = ''

        self.confdict['grid_user'] = 'griduser'
        self.confdict['grid_group'] = 'grid'

        self.confdict['enable_emies'] = False
        self.confdict['enable_gridftpd'] = False

        self.confdict['queue'] = None

        self.confdict['auth_groups'] = []


    def print_notice(self,text):
        print('\n')
        print '='*90
        print(text)
        print '='*90
        print('\n')

    def print_summary(self,prepend_txt='',append_txt=''):
        print_order = ['host_cert',
                       'host_key',
                       'log_rootdir',
                       'controldir',
                       'sessiondir',
                       'grid_security_path',
                       'lrms_type',
                       'lrms_path',
                       'grid_user',
                       'grid_group',
                       'enable_emies',
                       'enable_gridftpd']
        
        print('\n')
        print '='*90
        if prepend_txt:
            print(prepend_txt +'\n')
        for key in print_order:
            print('* {0:<30s} {1:<20s}'.format(self.descr[key] + ' :',str(self.confdict[key])))
        if append_txt:
            print(append_txt + '\n')
        print '='*90



    @staticmethod
    def true_false(ask, default_val=''):
        if default_val:
            ask = '\n' + ask + ' (ENTER for default value: ' + default_val + ')'
        else:
            ask = '\n' + ask 

        _in = None
        while _in is None:
            try:
                _in = str(raw_input('{0:<80}\n==> '.format(ask)).lower())
            except KeyboardInterrupt:
                break
            except:
                print('Sorry, something went wrong, try again')
                
        try:
            if 'y' in _in:
                return True
            else:
                return False
        except:
            print('Something went wrong, (or got KeyboardInterrupt), exiting...')
            sys.exit(0)

    @staticmethod
    def text_answ(ask,default_val=''):

        if default_val:
            ask = '\n' + ask + ' (ENTER for default value: ' + default_val + ')'
        else:
            ask = '\n' + ask 

        while True:
            try:
                return str(raw_input('{0:<80}\n==> '.format(ask)).lower())
            except KeyboardInterrupt:
                break
            except:
                print('Sorry, something went wrong, try again')
                continue
            else:
                break

    def fill_values_fromargs(self,args):
        """ 
        If arguments are issued when invoking arcctl install-wiz runall or arcctl install-wiz create_conf 
        the arc.conf settings are filled using these values and not from program dialogue.
        For any arguments not issued, default values are used. 
        """

        for key,val in vars(args).iteritems():
            if val:
                self.confdict[key]=val

        """ Print out a summary of values selected by  user """
        prepend_txt='Your chosen configuration values are:'
        append_txt=''
        self.print_summary(prepend_txt,append_txt)


    def get_user_input_condor(self):

        non_default = self.text_answ('Full path to Condor config file',self.confdict['lrms_condor_config'])
        if non_default:
            self.confdict['lrms_condor_config'] = non_default


    def get_user_input_sge(self):

        non_default = self.text_answ('Path to SGE installation directory',self.confdict['lrms_sge_root'])
        if non_default:
            self.confdict['lrms_sge_root'] = non_default


    def get_user_input_boinc(self):

        non_default = self.text_answ('hostname - Connection string for the boinc database: host',self.confdict['lrms_boinc_db_host'])
        if non_default:
            self.confdict['lrms_boinc_db_host'] = non_default

        non_default = self.text_answ('',self.confdict['lrms_boinc_db_host'])
        if non_default:
            self.confdict['lrms_boinc_db_host'] = non_default

        non_default = self.text_answ('',self.confdict['lrms_boinc_db_port'])
        if non_default:
            self.confdict['lrms_boinc_db_port'] = non_default

        non_default = self.text_answ('',self.confdict['lrms_boinc_db_user'])
        if non_default:
            self.confdict['lrms_boinc_db_user'] = non_default

        non_default = self.text_answ('',self.confdict['lrms_boinc_db_pass'])
        if non_default:
            self.confdict['lrms_boinc_db_pass'] = non_default


        
    def get_user_input(self):

        #use_all_defaults = self.true_false('Use all defaults? (If yes, you might as well just use the zero-conf shipped with ARC which is placed at /tmp/arc.conf)  [y/n]: ')
        use_all_defaults=False
        if not use_all_defaults:
            non_default = self.text_answ('Full path to arc.conf',self.confdict['conf_path'])
            if non_default:
                self.confdict['conf_path'] = non_default


            """ Authorization groups """
            print('The recommended way of authorizing groups of users is by using authorization groups. \nA group can be identified either by voms, by a userlist produced by the nordugridmap utility or by pointing to a grid-mapfile.')


            if(self.true_false('Use voms identification [y/n]')):
                vomses = self.text_answ('Enter a comma-separated list of vomses like: atlas * * * *, atlas * lcgadmin *')
                vomses = (vomses).split(',')
                
                print 'vomses: ', vomses
                for vo  in vomses:
                    vo = vo.strip()
                    print 'vo: ', vo
                    txt = 'Which (custom-named) authgroup does the vo:  ' + vo + ' belong to'
                    auth_group = self.text_answ(txt)
                    try:
                        self.confdict[auth_group].append(vo)
                    except KeyError:
                        self.confdict[auth_group] = [vo]

                    try:
                        self.confdict['auth_groups'][auth_group].append(vo)
                    except TypeError:
                        self.confdict['auth_groups']={}
                        self.confdict['auth_groups'][auth_group]=[]
                        self.confdict['auth_groups'][auth_group].append(vo)
                    except KeyError:
                        self.confdict['auth_groups'][auth_group]=[]
                        self.confdict['auth_groups'][auth_group].append(vo)



            """ HOSTCERTIFICATE
            For a production server a host certificate should actually exist. 
            However, for quick  testing, before a real host certificate is installed, a test host certificate can be generated."""
            self.create_host_cert = self.true_false('Do you need to create a test host certificate? [y/n]')
            if not self.create_host_cert:
                non_default  = self.text_answ('Full path to your existing host certificate',self.confdict['host_cert'])
                if non_default:
                    self.confdict['host_cert'] = non_default
                non_default = self.text_answ('Full  path to your existing host key',self.confdict['host_key'])
                if non_default:
                    self.confdict['host_key'] = non_default




            non_default = self.text_answ('Path to log dir',self.confdict['log_rootdir'])
            if non_default:
                self.confdict['log_rootdir'] = non_default
                
                
            non_default = self.text_answ('Path to sessiondir',self.confdict['sessiondir'])
            if non_default:
                self.confdict['sessiondir'] = non_default

                
            non_default = self.text_answ('Path to controldir', self.confdict['controldir'])
            if non_default:
                self.confdict['controldir'] = non_default
        

            self.confdict['lrms_type'] = self.text_answ('Which lrms : ',self.confdict['lrms_type'])

            if 'fork' not in self.confdict['lrms_type']:
                non_default =  self.text_answ('LRMS bin path',self.confdict['lrms_path'])
            if non_default:
                self.confdict['lrms_path'] = non_default

 
            if 'condor' in self.confdict['lrms_type']:
                get_user_input_condor()
            elif 'sge' in self.confdict['lrms_type']:
                get_user_input_sge()
            elif 'boinc' in self.confdict['lrms_type']:
                get_user_input_boinc()

            
            non_default = self.text_answ('Path to grid-security folder',self.confdict['grid_security_path'])
            if non_default:
                self.confdict['grid_security_path'] = non_default
            
            
            non_default = self.text_answ('What linux user to map grid jobs to',self.confdict['grid_user'])
            if non_default:
                    self.confdict['grid_user'] = non_default


            non_default = self.text_answ('What linux group to map grid jobs to',self.confdict['grid_group'])
            if non_default:
                    self.confdict['grid_group'] = non_default

                
            enable = self.true_false('Enable emies job submission [y/n]')
            if enable:
                self.confdict['enable_emies'] = True
                print('==> Assuming default port 443. See man arc.conf for defaults and how to change.\n')

                
            enable = self.true_false('Enable gridftpd job submission [y/n]')
            if enable:
                self.confdict['enable_gridftpd'] = True
                print('==> Assuming default port 2811. See man arc.conf for defaults and how to change.')
                print('==> Using port ranges 9000-10000, change in arc.conf if you want other port ranges.\n')


            """ How many queues to allow? Multiple?  """
            self.confdict['queue'] = self.text_answ('What is your default job queue')
            

    
        """ Print out a summary of values selected by  user """
        prepend_txt='Your chosen configuration values for ' +  self.confdict['conf_path'] + ' are:'
        append_txt=''
        self.print_summary(prepend_txt,append_txt)




    def create_conf(self,args):
        j2_env = Environment(loader=FileSystemLoader('./arc/control/templates'), trim_blocks=True)
        template = j2_env.get_template('arc.conf.j2')
        self.confdict['timestamp']  = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        rendered = template.render(**self.confdict)
        conf_path  = self.confdict['conf_path']
        try:
            with open(conf_path, 'w') as f:
                f.write(rendered)
                #print(rendered)
        except IOError as e:
            if e.errno == errno.EACCES: 
                logger.debug('***** You do not have permission to write to %s Instead writing it to /tmp/arc.conf - please copy it manually to %s *****', conf_path,conf_path)
                print_str = '====>>> IMPORTANT MESSAGE'
                print_str += '====>>> You do not have permission to write to ' + conf_path + '. File is temporarily stored in /tmp/arc.conf - please copy file manually to ' + conf_path
                self.print_notice(print_str)

                with open('/tmp/arc.conf','w') as f:
                    f.write(rendered)
        except Exception as e:
            logger.error('Creating %s failed.',conf_path)
            logger.error('%s',str(e))
            
        

    @staticmethod
    def create_griduser(_group,_user,args):
        """ 
        Creates the linux user:group that grid groups map to. Reflects the [mapping] section in arc.conf 
        Uses sudo to create group and user
        TODO: find better way to handle sudo need.
        """
        
        import pwd
        
        stdout, stderr = run_command('getent group ' + _group)
        if not stdout:
            """ stdout empty if group  does not exist """
            print(_group  + ' group does not exist: =>  creating it')
            stdout, stderr = run_command('groupadd ' + _group)
                                
        try:
            pwd.getpwnam(_user)
        except KeyError:
            print(_user +' does not exist: => creating user')
            stdout, stderr = run_command('useradd -g ' + _group  + ' ' + _user)


    @staticmethod
    def create_dir(_dir,args):
        """  
        Ensures that directories to store the ARC log-files exist.
        TODO: Handle sudo need.
        """

        if not os.path.exists(_dir):
            logger.debug('Creating dir %s',_dir)
            os.makedirs(_dir)


    @staticmethod
    def get_config(args=None):
        print config
        __runconfig = '/tmp/.arcctl.arc.run.conf'
        try:
            config.parse_arc_conf()
            config.save_run_config(__runconfig)
            arcconfig = config
        except IOError:
            logger.error('Failed to open ARC configuration file %s', conf_f)
            arcconfig = None
        return arcconfig

    @staticmethod
    def create_testCA(cactrl,args):
        cactrl.createca(args)

    @staticmethod
    def create_testHostCert(cactrl,args):
        cactrl.signhostcert(args)

    def dump(self,args):
        pass
    
    def control(self,args):



        """ Ask user for custom values for arc.conf """
        if args.action == 'dump':
            """ save the configuration options in json format, and dump to screen"""
            self.dump(args)


        if args.action == 'create_conf' or args.action == 'runall':
            
            """  Print out some general info """
            prepend_txt = 'This program requires root priveleges and helps set up a minimal configuration.\nYou can choose to use the default values which are:'
            append_txt =  '\n\n[NOTE:]: sudo rights is required to check that linux user and group exists or should be created.\n' \
                          'Will:\n' \
                          '* Construct an arc.conf placed in default location or location you specify with some minimum required contents.\n' \
                          '* Create a linux griduser and gridgroup if such does not exist\n' \
                          '* Create log folder if such does not exits\n' \
                          '* Create grid-security folder if such does not exits\n' \
                          '* Create a test-host certificate if no real host certificate exist\n'  \
                          '* Prepare ports according to arc.conf \n'
            self.print_summary(prepend_txt,append_txt)

            """ 
            User can insert values either as arguments to command or through program dialogue. 
            If no arguments are given program dialogue will be chosen 
            ./arcctl install-wiz <action>  (3 arguments to  python)
            """
            if len(sys.argv) <4:
                self.get_user_input()
            else:
                self.fill_values_fromargs(args)

            """  Create arc.conf """
            self.create_conf(args)

            """ Ensure logdirs exist """
            self.create_dir(self.confdict['log_rootdir'],args)

            """ Ensure grid-security dir exist """
            self.create_dir(self.confdict['grid_security_path'],args)

            """  Ensure users exist """
            self.create_griduser(self.confdict['grid_group'],self.confdict['grid_user'],args)
        
            """  Information to create host certificate """
            if self.create_host_cert:
                self.print_notice('====>>  To create a test host certificate please run \narcctl test-ca init\nto create a self-signed TestCA, followed by \narcctl test-ca hostcert\nand optionally \narcctl test-ca usercert\n if you need both a test host certificate and test user certificate')

        if args.action == 'testhostcert' or args.action == 'runall':
            """  Create test-CA and host certificate """
            if args.action == 'testhostcert':
                self.arcconfig = self.get_config(args)
            cactrl = TestCAControl(self.arcconfig)
            self.create_testCA(cactrl,args)
            self.create_testHostCert(cactrl,args)

        if args.action == 'iptables-config' or args.action == 'runall':
            """ Configure iptables for running ARC services """
            ipctrl = ThirdPartyControl(self.arcconfig)
            ipctrl.iptables_config(args.multiport, args.any_state)
            

    @staticmethod
    def register_parser(root_parser):

        help_arcconf = 'Your path to the resulting arc.conf file. Defaults to /etc/arc.conf'
        help_hostkey = 'Your path to your existing host certificate key. If you have no host certificate, ignore this option.'
        help_hostcert = 'Your path to your existing host certificate. If you have no host certificate, ignore this option. '
        help_logdir = 'Your base path of all your ARC logfiles.'
        help_controldir = 'Your path to ARC controldir'
        help_sessiondir = 'Your path to ARC sessiondir'
        help_gridsecurity = 'Your path to the grid-security folder'

        help_lrmstype = 'Please specify which LRMS you are using.'
        help_lrmspath = 'Your path to the lrsm binary.'

        help_condor = 'Must be set if condor lrms is chosen'
        help_sge = 'Must be set if sge lrms is chosen'
        help_boinc = 'Must be set if boinc lrms is chosen'

        installwiz_ctl = root_parser.add_parser('install-wiz', help='ARC Installation Wizard control')
        installwiz_ctl.set_defaults(handler_class=InstallationWizardControl)
        installwiz_actions = installwiz_ctl.add_subparsers(title='Installation Wizard Actions', dest='action',
                                                   metavar='ACTION', help='DESCRIPTION')


        installwiz_conf = installwiz_actions.add_parser('create_conf', help='Generate arc.conf file',formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        installwiz_conf.add_argument('--conf_path', help=help_arcconf)
        installwiz_conf.add_argument('--host_key', help=help_hostkey)
        installwiz_conf.add_argument('--host_cert', help=help_hostcert)
        installwiz_conf.add_argument('--log_rootdir', help=help_logdir)
        installwiz_conf.add_argument('--controldir', help=help_controldir)
        installwiz_conf.add_argument('--sessiondir', help=help_sessiondir)
        installwiz_conf.add_argument('--grid_security_path', help=help_gridsecurity)


        installwiz_conf.add_argument('--validity', default=90,help='Number of days the certificates will be valid.')
        installwiz_conf.add_argument('--digest', default='sha256',help='')
        installwiz_conf.add_argument('--force', action='store_true',default=False,help='If the TestCA or host key and/or cert already exists, the generation of the CA and/or host certificate will fail. Select --force if you want to delete the old files and create new ones.')
        installwiz_conf.add_argument('--hostname', action='store',help='')

        installwiz_conf.add_argument('--lrms_type', choices=lrmses,help=help_lrmstype)
        installwiz_conf.add_argument('--lrms_path', help=help_lrmspath)

        installwiz_conf.add_argument('--lrms_condor_config', default='etc/condor/condor_config',help=help_condor)
        installwiz_conf.add_argument('--lrms_sge_root', default='/gridware/sge',help=help_sge)
        installwiz_conf.add_argument('--lrms_boinc_db_host', default='localhost', help=help_boinc)
        installwiz_conf.add_argument('--lrms_boinc_db_port', default='3306', help=help_boinc)
        installwiz_conf.add_argument('--lrms_boin_db_user', help=help_boinc)
        installwiz_conf.add_argument('--lrms_boinc_db_pass',help=help_boinc)


        installwiz_conf.add_argument('-u', '--user', help='')
        installwiz_conf.add_argument('-g', '--group', help='')
        installwiz_conf.add_argument('--enable_emies', help='')
        installwiz_conf.add_argument('--enable_gridftpd', help='')


        installwiz_testhost = installwiz_actions.add_parser('testhostcert', help='Generate testCA and signed test host certificate')
        installwiz_testhost.add_argument('--validity', default=90,help='Number of days the certificates will be valid.')
        installwiz_testhost.add_argument('--digest', default='sha256',help='')
        installwiz_testhost.add_argument('--force', action='store_true',default=False,help='If the TestCA or host key and/or cert already exists, the generation of the CA and/or host certificate will fail. Select --force if you want to delete the old files and create new ones.')
        installwiz_testhost.add_argument('--hostname', action='store',help='')



        """ For running the full ARC Server InstallWizard """
        ## RawTextHelpFormatter not actually working, have not sorted out  why
        installwiz_runall = installwiz_actions.add_parser('runall', formatter_class=argparse.RawTextHelpFormatter,
                                                        help='''Do full installation wizard chain
                                                        * Set up of arc.conf 
                                                        * Create necessary directories
                                                        * Create linux users for mapping grid jobs
                                                        * Create test CA and host certificate if required
                                                        * Run validator and start services''')
        installwiz_runall.add_argument('--conf_path', help=help_arcconf)
        installwiz_runall.add_argument('--host_key', help=help_hostkey)
        installwiz_runall.add_argument('--host_cert', help=help_hostcert)
        installwiz_runall.add_argument('--log_rootdir', help=help_logdir)
        installwiz_runall.add_argument('--controldir', help=help_controldir)
        installwiz_runall.add_argument('--sessiondir', help=help_sessiondir)
        installwiz_runall.add_argument('--grid_security_path', help=help_gridsecurity)

        installwiz_runall.add_argument('--lrms_type', choices=lrmses,help=help_lrmstype)
        installwiz_runall.add_argument('--lrms_path', help=help_lrmspath)

        installwiz_runall.add_argument('--lrms_condor_config', default='etc/condor/condor_config',help=help_condor)
        installwiz_runall.add_argument('--lrms_sge_root', default='/gridware/sge',help=help_sge)
        installwiz_runall.add_argument('--lrms_boinc_db_host', default='localhost', help=help_boinc)
        installwiz_runall.add_argument('--lrms_boinc_db_port', default='3306', help=help_boinc)
        installwiz_runall.add_argument('--lrms_boin_db_user', help=help_boinc)
        installwiz_runall.add_argument('--lrms_boinc_db_pass',help=help_boinc)


        installwiz_runall.add_argument('-u', '--user', help='')
        installwiz_runall.add_argument('-g', '--group', help='')
        installwiz_runall.add_argument('--enable_emies', help='')
        installwiz_runall.add_argument('--enable_gridftpd', help='')
        installwiz_runall.add_argument('--validity', default=90,help='Number of days the certificates will be valid.')
        installwiz_runall.add_argument('--digest', default='sha256',help='')
        installwiz_runall.add_argument('--force', action='store_true',default=False,help='If the TestCA or host key and/or cert already exists, the generation of the CA and/or host certificate will fail. Select --force if you want to delete the old files and create new ones.')
        installwiz_runall.add_argument('--hostname', action='store',help='')

        installwiz_runall.add_argument('--any-state', default=False,action='store_true',
                                         help='Do not add \'--state NEW\' to filter configuration')
        installwiz_runall.add_argument('--multiport', default=False,action='store_true',
                                         help='Use one-line multiport filter instead of per-service entries')


        installwiz_ipconfig = installwiz_actions.add_parser('iptables-config',
                                                            help='Generate iptables config to allow ARC CE configured services')
        installwiz_ipconfig.add_argument('--any-state', default=False,action='store_true',
                                         help='Do not add \'--state NEW\' to filter configuration')
        installwiz_ipconfig.add_argument('--multiport', default=False,action='store_true',
                                         help='Use one-line multiport filter instead of per-service entries')
