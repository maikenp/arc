#! /usr/bin/env python
from ControlCommon import *
from TestCA import TestCAControl

import os, sys, re
import subprocess
import argparse
from jinja2 import Environment, FileSystemLoader
import yaml
from arc.utils import config


class InstallationWizardControl(ComponentControl):

    def __init__(self, arcconfig):

        self.logger = logging.getLogger('ARCCTL.InstallationWizard')
        self.control_dir = None
        self.arcconfig = arcconfig
        self.create_host_cert = False
     
        self.descr = {}
        self.descr['host_cert'] = 'hostcert path'
        self.descr['host_key'] = 'hostkey path'
        self.descr['hostname'] = ''
        
        self.descr['conf_path'] = 'arc.conf path'
        self.descr['logdir'] = 'logfile path'
        self.descr['controldir'] = 'controldir'
        self.descr['sessiondir'] = 'sessiondir'
        self.descr['grid_security_path'] = 'grid-security folder'

        self.descr['lrms_type'] = 'lrms type'
        self.descr['lrms_path'] = 'lrms bin path'

        self.descr['grid_user'] = 'linux grid user'
        self.descr['grid_group'] = 'linux grid group'

        self.descr['enable_emies'] = 'enable emies submission'
        self.descr['enable_gridftpd'] = 'enable gridftpd submission'


        self.settings = {}
        self.settings['host_cert'] = '/etc/grid-security/hostcert.pem'
        self.settings['host_key'] = '/etc/grid-security/hostkey.pem'
        self.settings['hostname'] = ''
        
        self.settings['conf_path'] = '/etc'
        self.settings['logdir'] = '/var/log/arc'
        self.settings['controldir'] = '/var/spool/arc/control'
        self.settings['sessiondir'] = '/var/spool/arc/session'
        self.settings['grid_security_path'] = '/etc/grid-security'

        self.settings['lrms_type'] = 'fork'
        self.settings['lrms_path'] = '/usr/bin'

        self.settings['grid_user'] = 'griduser'
        self.settings['grid_group'] = 'grid'

        self.settings['enable_emies'] = False
        self.settings['enable_gridftpd'] = False

        self.arcconfig = None




    def run_command(self,cmd):

        proc = subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell=True)
        stdout, stderr = proc.communicate()
        
        return stdout, stderr


    def print_summary(self,prepend_txt='',append_txt=''):
        __print_order = ['conf_path',
                         'host_cert',
                         'host_key',
                         'logdir',
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
        for key in __print_order:
            print('* {0:<30s} {1:<20s}'.format(self.descr[key] + ' :',str(self.settings[key])))
        if append_txt:
            print(append_txt + '\n')
        print '='*90



    
    def true_false(self,ask, default_val=''):
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


    def text_answ(self,ask,default_val=''):

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


        
    def get_user_input(self,args):


        use_all_defaults = self.true_false('Use all defaults? [y/n]: ')


        if not use_all_defaults:


            """ HOSTCERTIFICATE
            For a production server a host certificate should actually exist. 
            However, for quick  testing, before a real host certificate is installed, a test host certificate can be generated."""
            self.create_host_cert = self.true_false('Do you need to create a test host certificate? [y/n]')
            if not self.create_host_cert:
                non_default  = self.text_answ('Full path to your host certificate',self.settings['host_cert'])
                if non_default:
                    self.settings['host_cert'] = non_default
                non_default = self.text_answ('Full  path to your host key',self.settings['host_key'])
                if non_default:
                    self.settings['host_key'] = non_default

            lrms_type = self.text_answ('Which lrms [slurm/condor/fork]: ',self.settings['lrms_type'])
            
            non_default = self.text_answ('Path to log dir',self.settings['logdir'])
            if non_default:
                self.settings['logdir'] = non_default
                

            non_default = self.text_answ('Path to arc.conf',self.settings['conf_path'])
            if non_default:
                self.settings['conf_path'] = non_default
                
            non_default = self.text_answ('Path to sessiondir',self.settings['sessiondir'])
            if non_default:
                self.settings['sessiondir'] = non_default

                
            non_default = self.text_answ('Path to controldir', self.settings['controldir'])
            if non_default:
                self.settings['controldir'] = non_default
        

            if 'fork' not in self.settings['lrms_type']:
                non_default =  self.text_answ('LRMS bin path',self.settings['lrms_path'])
            if non_default:
                self.settings['lrms_path'] = non_default
            
            non_default = self.text_answ('Path to grid-security folder',self.settings['grid_security_path'])
            if non_default:
                self.settings['grid_security_path'] = non_default
            
            
            non_default = self.text_answ('What linux user to map grid jobs to',self.settings['grid_user'])
            if non_default:
                    self.settings['grid_user'] = non_default


            non_default = self.text_answ('What linux group to map grid jobs to',self.settings['grid_group'])
            if non_default:
                    self.settings['grid_group'] = non_default

                
            enable = self.true_false('Enable emies job submission [y/n]')
            if enable:
                self.settings['enable_emies'] = True
                print('==> Assuming default port 443. See man arc.conf for defaults and how to change.\n')

                
            enable = self.true_false('Enable gridftpd job submission [y/n]')
            if enable:
                self.settings['enable_gridftpd'] = True
                print('==> Assuming default port 2811. See man arc.conf for defaults and how to change.')
                print('==> Using port ranges 9000-10000, change in arc.conf if you want other port ranges.\n')




        """ Print out a summary of values selected by  user """
        prepend_txt='Your chosen configuration values are:'
        append_txt=''
        #if self.create_host_cert:
        #   append_txt='\n* A test host certificate will be created and placed in: ' \
        #      + self.settings['grid_security_path'] \
        #     + '\n* Please copy the CA and softlinks to your client machines grid-security folder (ignore if client and server are same machine).'
        self.print_summary(prepend_txt,append_txt)



    def create_conf(self,args):

        j2_env = Environment(loader=FileSystemLoader('/home/centos/fork-contrib/setupscript/templates'), trim_blocks=True)
        template = j2_env.get_template('arc.conf.j2')
        rendered = template.render(**self.settings)
        print(rendered)


    def create_griduser(self,args):
        import pwd

        
        stdout, stderr = self.run_command('getent group ' + self.settings['grid_user'])
        if stdout:
            pass
        else:
            print('Group ' +  self.settings['grid_group'] + ' does not exist, creating it')
            stdout, stderr = self.run_command('sudo groupadd ' + self.settings['grid_group'])
                                
        try:
            pwd.getpwnam(self.settings['grid_user'])
        except KeyError:
            print(self.settings['grid_user']+' does not exist, creating user')
            stdout, stderr = self.run_command('sudo useradd -g ' + self.settings['grid_group']  + ' ' + self.settings['grid_user'])


                
    def create_logdirs(self,args):
        pass

    def get_config(self,args):
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


    def create_testCA(self,cactrl,args):
        print 'In testCA'
        cactrl.createca(args)

    def create_testHostCert(self,cactrl,args):
        print 'In testhost'
        cactrl.signhostcert(args)

    def dump(self,args):
        pass
    
    def control(self,args):


        """ Ask user for custom values for arc.conf """
        if args.action == 'dump':
            """ save the configuration options in json format, and dump to screen"""
            self.dump(args)


        if args.action == 'fill_conf':

            """  Print out some general info """
            prepend_txt = 'A minimal configuration setup.\nYou can choose to use the default values requiring root priveleges. Default values are:'
            append_txt =  '\n\n[NOTE:]: sudo rights is required to check that linux user and group exists or should be created.\n' \
                          'Will:\n' \
                          '* Construct an arc.conf placed in default location or location you specify with some minimum required contents.\n' \
                          '* Create a linux griduser and gridgroup if such does not exist\n' \
                          '* Create grid-security folder if such does not exits\n' \
                          '* Create a test-host certificate if no real host certificate exist\n'
            self.print_summary(prepend_txt,append_txt)

            """ 
            User can insert values either as arguments to command or through program dialogue. 
            If no arguments are given program dialogue will be chosen 
            """
            if not args.with_args:
                self.get_user_input(args)

        

            """  Create arc.conf """
            self.create_conf(args)



        """ Ensure  log-dir exists """
        self.create_logdirs(args)


        if args.action == 'user':
            """ Create grid user and group """
            self.create_griduser(args)


        """  Create test-CA and host certificate """
        if args.action == 'testhostcert':
            self.arcconfig = self.get_config(args)
            cactrl = TestCAControl(self.arcconfig)
            self.create_testCA(cactrl,args)
            self.create_testHostCert(cactrl,args)


    @staticmethod
    def register_parser(root_parser):
        installwiz_ctl = root_parser.add_parser('install-wiz', help='ARC Installation Wizard control')
        installwiz_ctl.set_defaults(handler_class=InstallationWizardControl)
        installwiz_actions = installwiz_ctl.add_subparsers(title='Installation Wizard Actions', dest='action',
                                                   metavar='ACTION', help='DESCRIPTION')


        installwiz_conf = installwiz_actions.add_parser('fill_conf', help='Generate arc.conf file')
        installwiz_conf.add_argument('--with_args', action='store_true', help='Toggle this if you want to call the installation wizard using arguments to the command. Otherwise the installation wizard will prompt you for the necessary  configuration values')
        installwiz_conf.add_argument('--conf_path', help='')
        installwiz_conf.add_argument('--host_key', help='')
        installwiz_conf.add_argument('--host_cert', help='')
        installwiz_conf.add_argument('--logdir', help='')
        installwiz_conf.add_argument('--controldir', help='')
        installwiz_conf.add_argument('--sessiondir', help='')
        installwiz_conf.add_argument('--grid_security_path', help='')
        installwiz_conf.add_argument('--lrms_type', help='')
        installwiz_conf.add_argument('--lrms_path', help='')
        installwiz_conf.add_argument('-u', '--user', help='')
        installwiz_conf.add_argument('-g', '--group', help='')
        installwiz_conf.add_argument('--enable_emies', help='')
        installwiz_conf.add_argument('--enable_gridftpd', help='')


        installwiz_user = installwiz_actions.add_parser('user', help='Generate and sign testing host certificate')
        installwiz_user.add_argument('-u', '--user', action='store',
                                 help='Linux user to use for mapping jobs')
        installwiz_user.add_argument('-g', '--group', action='store',
                                 help='Linux group to use for mapping jobs')


        installwiz_testhost = installwiz_actions.add_parser('testhostcert', help='Generate testCA and signed test host certificate')
        installwiz_testhost.add_argument('--validity', default=90,help='')
        installwiz_testhost.add_argument('--digest', default='sha256',help='')
        installwiz_testhost.add_argument('--force', action='store_true',default=False,help='')
        installwiz_testhost.add_argument('--hostname', action='store',help='')



