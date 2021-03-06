#----------------------------------------------------------------
# load_exconfig.sh: Load Experiment.config and draw some basic
# conclusions
#
# Variables defined include:
# CFG_ERROR: Defined with a string defining a serious complaint
# Experiment: Must be defined in Experiment.config
# HomeDir: Must be defined in Experiment.config
# FltNode_Access: Either 'Local' or 'Remote'
# SERVICE: Either 'Installed' or 'AdHoc'
# TM_OPT: Options to access tm_bfr on the flight node
# CMD_OPT: Options to access cmd on the flight node
# VERSION: Read from the VERSION file or defaults to 1.0
# TMBINDIR: Where the experiment-specific binaries are located
# PATH is updated to include TMBINDIR
#
# Experiment and TMBINDIR are exported as required by virtually
# all the dasio-cognizent programs
#
# Determination of the SERVICE status is heuristic and based on
# circumstantial evidence. Anytime FlightNode is defined it is
# assumed the service is installed on the FlightNode, which is
# reasonable. It is entirely possible to imagine running a remote
# instrument via ssh similar to the way we would start up a local
# AdHoc instrument, but we would need a separate variable to mark
# the FlightNode as being AdHoc, since it may not be accessible
# when the decisions are being made.
#
# Similarly, when FlightNode is not defined, the presence of
# parent registered for this Experiment
# is deemed strong enough evidence that the service is installed.
#----------------------------------------------------------------
export PATH=:/bin:/usr/bin:/usr/local/bin
umask g+w

#----------------------------------------------------------------
# This is where we will decide what experiment we are
#----------------------------------------------------------------
cfile=Experiment.config
unset Experiment HomeDir FlightNode CFG_ERROR
unset FltNode_Access TM_OPT CMD_OPT
if [ -f "$cfile" -a -r "$cfile" ]; then
  . $cfile
  if [ -n "$Experiment" -a -n "$HomeDir" -a -d "$HomeDir" ]; then
    cd $HomeDir
  else
    CFG_ERROR="Experiment or HomeDir undefined or non-existant"
  fi
else
  CFG_ERROR="missing or unreadable $cfile: stopping"
fi

export Experiment

# Determine whether we are running on the FlightNode (local) or
# on a separate GSE (remote)
FltNode_Access=Local
if [ -n "$FlightNode" -a ! /home/flight/Experiment.config -ef $cfile ]; then
  FltNode_Access=Remote
fi

# echo "load_exconfig: FltNode_Access: [$FltNode_Access]"
# echo "load_exconfig: TM_OPT: [$TM_OPT]"

if [ $FltNode_Access = Remote ]; then
  TM_OPT="-t $FlightNode"
  CMD_OPT="-C $FlightNode"
  SSH_CMD="ssh -t flight@$FlightNode"
  SSH_TAR_CMD="ssh -T flight@$FlightNode"
  SERVICE=Installed
elif [ -e /var/run/monarch/$Experiment/parent -a \
       -e /var/run/monarch/$Experiment/parent.pid ] &&
     [ -e /proc/`cat /var/run/monarch/$Experiment/parent.pid` ]; then
  SERVICE=Installed
else
  SERVICE=AdHoc
fi

VERSION=1.0
[ -f VERSION ] && VERSION=`cat VERSION`
if [ -d bin/$VERSION/ ]; then
  TMBINDIR=$PWD/bin/$VERSION
  PATH=$TMBINDIR:$PATH
else
  TMBINDIR='.'
fi
export TMBINDIR

msgVdefault='-V'
LaunchVdefault='-V'
[ -z "$msgProgram" ] && msgProgram='load_exconfig.sh'
msgDebug=-1

function msgf {
  msgV=$msgVdefault
  if [ "$1" = '-V' ]; then
    msgV=-V
    shift
  fi
  code=$1
  excode=''
  case $code in
    0) prefix='';;
    1) prefix='[Warn] ';;
    2) prefix='[Error] ';;
    3) prefix='[Fatal] '; excode=1;;
    4) prefix='[Internal] '; excode=1;;
    -1) prefix=''; excode=0;;
    *) prefix='[Debug] ';;
  esac
  shift
  if [ $code -ge $msgDebug ]; then
    /usr/local/bin/msg $msgV -n $msgProgram "$prefix$*"
  fi
  [ -n "$excode" ] && exit $excode
}

function set_have {
  # name will be sanitized before getting here
  name=$1
  value=$2
  if [ -n "$name" ]; then
    eval have_$name=$value
  fi
}

function Launch {
  if [ -n "$launch_error" ]; then
    msgf $LaunchVdefault 2 "Skipping Launch $1 $2 due to earlier errors"
    return 1
  fi
  Debug=no
  if [ "$1" = "Debug" ]; then
    Debug=yes
    shift
  fi
  name=$1
  shift
  msgf $LaunchVdefault -2 "Launch: $*"
  shortname=''
  wname=''
  case $name in
    -*) :;;
    /*) wname=$name;;
    */*) wname=/var/run/monarch/$Experiment/$name;;
    *) wname=/var/run/monarch/$Experiment/$name
       shortname=$name;;
  esac
  set_have "$shortname" no
  # bfr is supposed to create this directory
  # mkdir -p /var/run/monarch/run/$Experiment
  
  # Check to make sure name does not already exist
  if [ -n "$wname" -a -e "$wname" ]; then
    # name already exists, so may not need to launch
    if [ -S $wname -a -f $wname.pid ]; then
      pid=`cat $wname.pid`
      if kill -0 $pid 2>/dev/null; then
        msgf $LaunchVdefault 2 "Launch of $1 Skipped: socket $name still active"
        launch_error=yes
        return 1
      else
        # echo socket $socket is abandoned
        rm -f $wname
        rm -f ${wname}.pid
      fi
    fi
  fi

  if [ $Debug = yes -a -n "$STY" ]; then
    sname=$*
    sname=${sname%% *}
    rm -f monarch_gdb_ready
    screen -t $sname gdb --args $*
    waitfor monarch_gdb_ready forever
    if [ -s monarch_gdb_ready ]; then
      msgf -V 2 "Aborting at Debug due to non-empty wait file"
      launch_error=yes
      rm -f monarch_gdb_ready
      return 1
    fi
    rm -f monarch_gdb_ready
    msgf $LaunchVdefault 0 "Debug: $*"
  elif { $* & }; then
    Launch_pid=$!
    msgf $LaunchVdefault 0 "Launch: $Launch_pid $*"
  else
    msgf $LaunchVdefault 2 "Launch Error: $*"
    launch_error=yes
    return 1
  fi
  if [ -n "$wname" ]; then
    waitfor $wname 20 || {
      msgf $LaunchVdefault 2 "Launch namewait failure: $*"
      launch_error=yes
      return 1
    }
  fi
  [ "$name" = memo ] && msgVdefault=''
  set_have "$shortname" yes
  return 0
}

