#
# Common block for cancel scripts
# must be called with the grami file as argument
# remember to set $joboption_lrms

if [ -z ${ARC_LOCATION} ] ; then
    echo "ARC_LOCATION not set."  1>&2
    exit 1
fi
if [ ! -f "${ARC_LOCATION}/libexec/configure-${joboption_lrms}-env.sh" ] ; then
    echo "${ARC_LOCATION}/libexec/configure-${joboption_lrms}-env.sh not found." 1>&2
    exit 1
fi
source ${ARC_LOCATION}/libexec/configure-${joboption_lrms}-env.sh

arg_file=$1
##############################################################
# Source the argument file.
##############################################################
if [ -z "$arg_file" ] ; then
   echo "Arguments file should be specified" 1>&2
   exit 1
fi
if [ ! -f $arg_file ] ; then
   echo "Missing arguments file" 1>&2
   exit 1
fi

. $arg_file

