Linux process connector
=======================

Allows to monitor fork/exec/exit/uid change events.

Example:

		cnproc = require('cnproc');
		c = cnproc.createConnector();
		c.on('fork', function(ppid, ptgid, pid, tgid) {
			 console.log("fork: {" + "ppid: " + ppid + " ptgid: " + ptgid + "pid: " + pid + "ptgid: " + tgid);
			 c.close(); // close after first event
		});
		c.connect();

fork
----
Callback arguments are as follows: parent pid, parent tgid, child pid, child tgid.

exec
----
Callback arguments: pid, tgid.

exit
----
Callback arguments: pid, tgid, exit_code.

uid
---
Callback arguments: pid, tgid, old uid, new uid.
