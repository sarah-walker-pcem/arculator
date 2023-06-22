Arculator Networking
====================

Arculator provides two methods of networking :

SLiRP (Windows & Linux)
~~~~~~~~~~~~~~~~~~~~~~~

This allows basic IP functionality (eg web browsing) with little fuss.

Network settings for SLiRP are fixed to :

IP address: 10.0.2.15
Netmask: 255.255.255.0
Gateway: 10.0.2.2
DNS: 10.0.2.3

PCAP (Windows)
~~~~~~~~~~~~~~

This allows for a more "complete" emulation, eg Access+ and AUN.

You will need a wired network connection. PCAP requires promiscuous mode which isn't generally supported by WiFi adapters.

To find out network settings, open a Command Prompt and run "ipconfig /all". Identify the appropriate Ethernet adapter and note the following settings :

IP address - You should make up an unused address on the same subnet as "IPv4 Address". Eg if ipconfig gives an address of 192.168.1.1, possible addresses would be 192.168.1.2, 192.168.1.3, 192.168.1.50 etc. You may want to configure your router to reserve the chosen address to prevent conflicts with anything else on the network.
Netmask - This is "Subnet Mask".
Gateway - This is "Default Gateway".
DNS - This is "DNS Server(s)". Only one is required, though multiple DNS servers are supported.


Setting up networking under RISC OS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The recommended option is to use Internet 5. You will need to install UniBoot.

With UniBoot installed, go to Apps and run !InetSetup. Click on Internet then "Enable TCP/IP Protocol Suite".

Click on Interfaces, then Configure... for the chosen network card. Set "Obtain IP address" to "Manually". Enter the IP Address and Netmask. Click Set and close the Interfaces window.

Click on Routing. Enter the Gateway, then click Set.

Click on Host Names. Enter a Host Name and Local domain of your choice (these are not optional!). Set "Primary name server" (and Secondary and Tertiary if desired) to the DNS address(s). Click Set.

Click on Close to close the Internet configuration window, then Save to close Network configuration. RISC OS will then reboot.

To test the configuration, press F12 then type "ping <Gateway>". Note that when using SLiRP, the gateway address is the only address that will respond to ping.

