mod_aws - Copyright (C) 2009 TelTech Systems Inc.

This module brings Amazon Web Services to DSM. 

It uses and depends on libaws++, see http://aws.28msec.com/.

mod_aws must be preloaded. See aws.conf for configuration 
(aws credentials, connection pool size etc).

Actions
-------
S3: bucket name from $aws.s3.bucket is used if not specified as 
action parameter.

aws.s3.put(string filename [, string keyname]) 
  put file into bucket, if specified under key keyname, 
  else keyname=filename

aws.s3.putArray(string filename_array [, string keyname_array])
  put files into bucket, if specified under key keyname, 
  else keyname=filename
  array is $filename_0, $filename_1, .. $filename_size

SQS: queue name from $aws.sqs.queue is used if not specified as 
action parameter.
 
aws.s3.createBucket([string bucket_name])
  create bucket. created bucket name in $aws.s3.bucket (should be same
  as bucket_name)

aws.sqs.createQueue(int visibilityTimeout [, string queue_name])
  create queue

aws.sqs.deleteQueue([string queue_name])
  delete queue

aws.sqs.sendMessage(string message [, string queue_name])
  send a message in queue. sets
    $aws.sqs.url
    $aws.sqs.md5


Error handling
--------------
On error, errno is set, and aws.ereason has more detailed 
description.

Errno codes:
#define DSM_ERRNO_AWS_CONN    "50"
#define DSM_ERRNO_AWS_PUT     "51"
#define DSM_ERRNO_AWS_CREATE  "52"
#define DSM_ERRNO_AWS_DELETE  "53"
#define DSM_ERRNO_AWS_SEND    "54"
