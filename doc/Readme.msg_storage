
This is an example module for file system based storage for 
messages. It uses the file atime vs. mtime to determine whether a 
messagee is new.

This module lacks delete-locking of an open message 
directory, which means that its possible to delete a message in
one call and on a parallel call not be able to listen to that 
deleted message any more (even if it was in the list before). This 
should be very rare usage though.

If you are looking for efficient, simple, application level 
replication of message files for all-active shared-nothing  
n+1 redundancy cluster voicemail and voicebox operation at 
low cost, don't hesitate to contact iptego GmbH at 
mailto:info@iptego.com or have a look at 
http://www.iptego.com/products/cnsp/ .
