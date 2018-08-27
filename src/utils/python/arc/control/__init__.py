from __future__ import absolute_import

from .RunTimeEnvironment import RTEControl
from .Jobs import JobsControl
from .ThirdPartyDeployment import ThirdPartyControl
from .Services import ServicesControl
from .Accounting import AccountingControl
from .Config import ConfigControl
from .TestCA import TestCAControl
from .InstallationWizard import InstallationWizardControl

CTL_COMPONENTS = [
    RTEControl,
    JobsControl,
    ServicesControl,
    ThirdPartyControl,
    AccountingControl,
    ConfigControl,
    TestCAControl,
    InstallationWizardControl
]
