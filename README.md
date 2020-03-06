# PeaceMaker Threat Detection
PeaceMaker Threat Detection is a kernel-mode utility designed to detect a variety of methods commonly used in advanced forms of malware. Compared to a stereotypical anti-virus that may detect via hashes or patterns, PeaceMaker targets the techniques malware commonly uses in order to catch them in the act. Furthermore, PeaceMaker is designed to provide an incredible amount of detail when a malicious technique is detected, allowing for effective containment and response.

## Motivation
PeaceMaker was designed primarily as a weapon to detect custom malware in virtualized environments. Specifically, this project was started in pursuit of preparing for the [Information Security Talent Search](https://www.ists.io/) blue/red team competition hosted by RIT's Security Club, [RITSEC](https://www.ritsec.club/). The competition's red team is primarily industry security professionals, which is why I decided my own defense platform would be useful. In a project like this, I can make sacrifices to factors such as performance that widely-employed AV/EDR companies can't make, allowing me to make decisions I couldn't get away with in a real product.

## Features
- View what code started a process (stack trace).
- View what code loaded an image into a process (stack trace).
- Detect unmapped (hidden) code via Stack Walking common operations such as:
	- Process Creation
	- Image Load
	- Thread Creation
- Detect remote thread creation.
- Detect parent process ID spoofing.
- Detect threat creation on unmapped (hidden) code.
- Block basic tamper operations on the GUI Client.
- Block filesystem/registry write, delete, or execute operations that violate a user-specified filter.
- Detect filesystem/registry write, delete, or execute operations that violate a user-specified filter.
	- Logs the source process and stack of the operation.
- Filter for known false positives.

## Notable properties
- Heavily commented code.
- All detection routines are in the kernel driver.
- Designed to detect user-mode malware.
- Tested using Driver Verifier standard configuration.
- Tested by putting it on my "daily driver" laptop and monitoring for issues (none occurred).

## Shortcomings
- Inefficient time and space complexity.
	- Performs useful, but expensive forensics that slow down common operations.
	- Often allocates more memory than needed, doesn't utilize space optimization techniques such as compression.
- Weak operation filtering mechanism.
	- For example, filters that prevent deletion of a file or registry key can be bypassed.
	- You can only filter on the target of an operation (i.e file/key name).
- Weak tamper protection.
	- Only protects against process termination of the GUI, nothing else.
- Incomplete GUI.

## Screenshots
![Alerts Tab](https://i.imgur.com/5L7fcsu.png)
![Processes Tab](https://i.imgur.com/v6NtDXH.png)
![Filters Tab](https://i.imgur.com/PL2clda.png)
![Process Information](https://i.imgur.com/Jefp8ac.png)
