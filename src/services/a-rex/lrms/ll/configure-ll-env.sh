#
# set environment variables:
#   LL_BIN_PATH
#

##############################################################
# Reading configuration from $ARC_CONFIG
##############################################################

if [ -z "$pkgdatadir" ]; then echo 'pkgdatadir must be set' 1>&2; exit 1; fi

. "$pkgdatadir/config_parser_compat.sh" || exit $?

ARC_CONFIG=${ARC_CONFIG:-/etc/arc.conf}
config_parse_file $ARC_CONFIG 1>&2 || exit $?

config_import_section "common"
config_import_section "infosys"
config_import_section "grid-manager"

# performance logging: if perflogdir or perflogfile is set, logging is turned on. So only set them when enable_perflog_reporting is ON
unset perflogdir
unset perflogfile
enable_perflog=${CONFIG_enable_perflog_reporting:-no}
if [ "$CONFIG_enable_perflog_reporting" == "yes" ]; then
   perflogdir=${CONFIG_perflogdir:-/var/log/arc/perfdata}
   d=`date +%F`
   perflogfile="${perflogdir}/backends${d}.perflog"
fi

# Also read queue section
if [ ! -z "$joboption_queue" ]; then
  config_import_section "queue/$joboption_queue"
fi

# Path to ll commands
LL_BIN_PATH=$CONFIG_ll_bin_path
if [ ! -d ${LL_BIN_PATH} ] ; then
    echo "Could not set LL_BIN_PATH." 1>&2
    exit 1
fi

# Consumable resources
LL_CONSUMABLE_RESOURCES=${LL_CONSUMABLE_RESOURCES:-$CONFIG_ll_consumable_resources}

# Enable parallel single jobs 
LL_PARALLEL_SINGLE_JOBS=${LL_PARALLEL_SINGLE_JOBS:-$CONFIG_ll_parallel_single_jobs}

# Local scratch disk
RUNTIME_LOCAL_SCRATCH_DIR=${RUNTIME_LOCAL_SCRATCH_DIR:-$CONFIG_scratchdir}
export RUNTIME_LOCAL_SCRATCH_DIR

