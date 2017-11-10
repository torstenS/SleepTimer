This directory contains some scripts and config files that demonstrate the
usage of the SleepTimer on linux side.

The requirement is to regularly backup a remote NAS via rsync to a local attached 
USB drive. Rotating backubs are maintained by rsnapshot.
The rsnapshot.conf is given as an example and should be placed in /etc.
The rsnapshot.omv provides the backup paths for all available rsync folders of the NAS.
Regular execution is triggered by the rsnapshot entry in /etc/cron/daily.
Because the linux box will not run permanently anachron will be used to trigger
missed cron entries.
The run-rsnapshot script will execute outstanding rsnapshots, monitor success or failure and
arm the SleppTimer at the end, just before initiating a shutdown.
The shutdown script will be registered as the shell for the shutdown user.
This way, pressing the button on the SleepTimer 
  - which will send shutdown to the console
  - which will logon as the shutdown user
  - which will execute the shutdown script
will arm the SleepTimer for the next job before actually shutdown. 
