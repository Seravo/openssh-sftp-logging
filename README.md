A command wrapper to log sftp sessions into wtmp on GNU/Linux.

# USAGE

Change the following line in sshd_config from:

    Subsystem sftp /usr/lib/openssh/sftp-server

to

    Subsystem sftp sftpwrapper -c SSH_CLIENT -- /usr/lib/openssh/sftp-server

# BUGS AND LIMITATIONS

* Hardcoded executable paths for sudo and wtmplogger.
  These should be made configurable.

* Hardcoded parent process checking in wtmplogger.
  These should be made configurable.

* Only supports Linux /proc file system to get process information.
  Support for other platforms should be provided.

# AUTHOR

Copyright 2020 Seravo Oy.

You can mail feedback and improvements to Heikki Orsila <heikki.orsila@iki.fi>
