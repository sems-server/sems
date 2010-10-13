/*
 * Copyright (C) 2009 TelTech Systems Inc.
 * 
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ModAws.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "AmPlaylist.h"
#include "AmConfigReader.h"

#include <fstream>

SC_EXPORT(MOD_CLS_NAME);

ConnectionPool<S3ConnectionPtr>* SCAwsModule::s3ConnectionPool = NULL;
ConnectionPool<SQSConnectionPtr>* SCAwsModule::sqsConnectionPool = NULL;

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("aws.s3.put", SCS3PutFileAction);
  DEF_CMD("aws.s3.putArray", SCS3PutMultiFileAction);
  DEF_CMD("aws.s3.createBucket", SCS3CreateBucketAction);

  DEF_CMD("aws.sqs.createQueue", SCSQSCreateQueueAction);
  DEF_CMD("aws.sqs.deleteQueue", SCSQSDeleteQueueAction);
  DEF_CMD("aws.sqs.sendMessage", SCSQSSendMessageAction);
  DEF_CMD("aws.sqs.receiveMessage", SCSQSReceiveMessageAction);
  DEF_CMD("aws.sqs.deleteMessage", SCSQSDeleteMessageAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

int SCAwsModule::preload() {
   AmConfigReader cfg;
   if(cfg.loadFile(AmConfig::ModConfigPath + string("aws.conf")))
     return -1;

   string aws_access_key = cfg.getParameter("aws_access_key");
   string aws_secret_access_key = cfg.getParameter("aws_secret_access_key");
   if (aws_access_key.empty() || aws_secret_access_key.empty()) {
     ERROR("aws_access_key / aws_secret_access_key must be configured in aws.conf!\n");
     return -1;
   }

   unsigned int initial_connections_s3 = cfg.getParameterInt("s3_initial_connections", 0);  
   try {
     s3ConnectionPool = new ConnectionPool<S3ConnectionPtr>(initial_connections_s3,
							    aws_access_key, aws_secret_access_key);
   } catch (const AWSInitializationException& e) {
     ERROR("initializing AWS: '%s'\n", e.what());
     return -1;
   } catch (const AWSAccessKeyIdMissingException& e) {
     ERROR("missing AWS key\n");
     return -1;
   } catch (const AWSSecretAccessKeyMissingException& e) {
     ERROR("missing AWS secret key\n");
     return -1;
   } catch (const AWSConnectionException& e) {
     ERROR("connection failed:'%s'\n", e.what());
     return -1;
   } catch (const AWSException& e) {
     ERROR("creating AWS connections: '%s'\n", e.what());
     return -1;
   }

   unsigned int initial_connections_sqs = cfg.getParameterInt("sqs_initial_connections", 0);  
   try {
     sqsConnectionPool = new ConnectionPool<SQSConnectionPtr>(initial_connections_sqs,
							      aws_access_key, aws_secret_access_key);
   } catch (const AWSInitializationException& e) {
     ERROR("initializing AWS: '%s'\n", e.what());
     return -1;
   } catch (const AWSAccessKeyIdMissingException& e) {
     ERROR("missing AWS key\n");
     return -1;
   } catch (const AWSSecretAccessKeyMissingException& e) {
     ERROR("missing AWS secret key\n");
     return -1;
   } catch (const AWSConnectionException& e) {
     ERROR("connection failed:'%s'\n", e.what());
     return -1;
   } catch (const AWSException& e) {
     ERROR("creating AWS connections: '%s'\n", e.what());
     return -1;
   }

   return 0;
}

#define CHECK_PRELOAD_S3				\
  if (NULL == SCAwsModule::s3ConnectionPool) {		\
    ERROR("mod_aws must be preloaded!\n");		\
    throw DSMException("aws", "cause", "need preload"); \
    return false;					\
  }							\
  

#define GET_CONNECTION_S3						\
  S3ConnectionPtr aS3 = SCAwsModule::s3ConnectionPool->getConnection(); \
  if (!aS3) {								\
    ERROR("getting S3 connection from connection pool\n");		\
    throw DSMException("aws", "cause", "get connection");		\
    return false;							\
  }									\

#define   FREE_CONNECTION_S3  SCAwsModule::s3ConnectionPool->release(aS3);

CONST_ACTION_2P(SCS3PutFileAction, ',', true);
EXEC_ACTION_START(SCS3PutFileAction) {
  CHECK_PRELOAD_S3;
  string filename = resolveVars(par1, sess, sc_sess, event_params);
  string keyname  = resolveVars(par2, sess, sc_sess, event_params);

  const string& bucket = sc_sess->var["aws.s3.bucket"];
  if (bucket.empty()) {
    ERROR("S3: trying to put in empty bucket!\n");
    throw DSMException("aws", "cause", "empty bucket");
    return false;
  }
    
  GET_CONNECTION_S3;
  try {
    std::ifstream lInStream(filename.c_str());
    if (!lInStream) {
      WARN("file not found or accessible: '%s'\n",filename.c_str());
      FREE_CONNECTION_S3;
      throw DSMException("aws", "cause", "file not found");
      return false;
    }
    PutResponsePtr lPut = aS3->put(bucket, keyname.length()==0?filename:keyname, 
				   lInStream, "application/octet-stream");

    sc_sess->SET_ERRNO(DSM_ERRNO_OK);
  } catch (const AWSException &e) {
    WARN("S3 put failed: '%s'\n", e.what());
    sc_sess->var["aws.ereason"] = e.what();
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_PUT);
  }
  FREE_CONNECTION_S3;

} EXEC_ACTION_END;


CONST_ACTION_2P(SCS3PutMultiFileAction, ',', true);
EXEC_ACTION_START(SCS3PutMultiFileAction) {
  CHECK_PRELOAD_S3;

  string files_array = resolveVars(par1, sess, sc_sess, event_params);
  string keys_array  = resolveVars(par2, sess, sc_sess, event_params);
  if (keys_array.empty())
    keys_array = files_array;

  unsigned int num_files = 0;
  if (str2i(sc_sess->var[files_array+"_size"], num_files)) {
    ERROR("determining size of '%s' array\n", files_array.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("determining size of '"+
			  files_array+"' array\n");
    return false;
  }

  // safety check
  unsigned int num_keys = 0;
  if (str2i(sc_sess->var[keys_array+"_size"], num_keys)) {
    ERROR("determining size of '%s' array\n", keys_array.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("determining size of '"+
			  keys_array+"' array\n");
    return false;
  }

  for (unsigned int i=0;i<num_files;i++) {
    if (sc_sess->var[files_array+"_"+int2str(i)].empty()) {
      ERROR("missing file name $%s_%u\n", files_array.c_str(), i);
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("missing file name $"+
			    files_array+"_"+int2str(i)+"\n");
      return false;
    }
  }

  const string& bucket = sc_sess->var["aws.s3.bucket"];
  if (bucket.empty()) {
    ERROR("S3: trying to put with empty bucket name!\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("S3: trying to put with empty bucket name!\n");
    return false;
  }
    
  GET_CONNECTION_S3;

  for (unsigned int i=0;i<num_files;i++) {
    string filename = sc_sess->var[files_array+"_"+int2str(i)];
    string keyname = sc_sess->var[keys_array+"_"+int2str(i)];

    try {
      std::ifstream lInStream(filename.c_str());
      if (!lInStream) {
	WARN("file not found or accessible: '%s'\n",filename.c_str());
	sc_sess->SET_ERRNO(DSM_ERRNO_AWS_PUT);
	sc_sess->SET_STRERROR("file not found or accessible: '"+filename+"'\n");
	FREE_CONNECTION_S3;
	return false;
      }
      PutResponsePtr lPut = aS3->put(bucket, keyname.length()==0?filename:keyname, 
				     lInStream, "application/octet-stream");
      
      sc_sess->CLR_ERRNO;
    } catch (const AWSException &e) {
      WARN("S3 put failed: '%s'\n", e.what());
      sc_sess->var["aws.ereason"] = e.what();
      sc_sess->SET_ERRNO(DSM_ERRNO_AWS_PUT);
      sc_sess->SET_STRERROR(e.what());
    }
  }

  FREE_CONNECTION_S3;

} EXEC_ACTION_END;

EXEC_ACTION_START(SCS3CreateBucketAction) {

  CHECK_PRELOAD_S3;

  string bucket = resolveVars(arg, sess, sc_sess, event_params);
  if (bucket.empty())
    bucket = sc_sess->var["aws.s3.bucket"];
  if (bucket.empty()) {
    ERROR("S3: trying to create bucket with empty bucket name!\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("S3: trying to create bucket with empty bucket name!");
    return false;
  }

  GET_CONNECTION_S3;
  try {
    CreateBucketResponsePtr lRes = aS3->createBucket(bucket);
    bucket = lRes->getBucketName();
    DBG("created bucket with name '%s'\n", bucket.c_str());
    sc_sess->var["aws.s3.bucket"] = bucket;
    sc_sess->CLR_ERRNO;
  } catch (const AWSException &e) {
    sc_sess->var["aws.ereason"] = e.what();
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_CREATE);
    sc_sess->SET_STRERROR(e.what(););
  }
  FREE_CONNECTION_S3;

} EXEC_ACTION_END;

#define CHECK_PRELOAD_SQS				\
  if (NULL == SCAwsModule::sqsConnectionPool) {		\
    ERROR("mod_aws must be preloaded!\n");		\
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);		\
    sc_sess->SET_STRERROR("mod_aws must be preloaded");	\
    return false;					\
  }							\

#define GET_CONNECTION_SQS						\
  SQSConnectionPtr aSQS = SCAwsModule::sqsConnectionPool->getConnection(); \
  if (!aSQS) {								\
    ERROR("getting SQS connection from connection pool\n");		\
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_CONN);				\
    sc_sess->SET_STRERROR("getting SQS connection from connection pool"); \
    return false;							\
  }									\
  
#define FREE_CONNECTION_SQS  SCAwsModule::sqsConnectionPool->release(aSQS);

#define GET_QUEUE_ARG(paramname)					\
  string sqs_queue = resolveVars(paramname, sess, sc_sess, event_params); \
  if (sqs_queue.empty())						\
    sqs_queue = sc_sess->var["aws.sqs.queue"];				\
  if (sqs_queue.empty()) {						\
    ERROR("SQS: trying to operate on empty queue name!\n");		\
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);				\
    sc_sess->SET_STRERROR("SQS: trying to operate on empty queue name!"); \
    return false;							\
  }									\
  

CONST_ACTION_2P(SCSQSCreateQueueAction, ',', true);
EXEC_ACTION_START(SCSQSCreateQueueAction) {
  CHECK_PRELOAD_SQS;
  string aVisibilityTimeout_s = resolveVars(par1, sess, sc_sess, event_params);
  unsigned int aVisibilityTimeout = 0;
  if (str2i(aVisibilityTimeout_s, aVisibilityTimeout)) {
    ERROR("unable to determine aVisibilityTimeout '%s' for new queue\n",
	  aVisibilityTimeout_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unable to determine aVisibilityTimeout '"+
			  aVisibilityTimeout_s+"' for new queue\n");
    return false;
  }
  GET_QUEUE_ARG(par2);

  GET_CONNECTION_SQS;
  try {
    CreateQueueResponsePtr lRes = aSQS->createQueue (sqs_queue, aVisibilityTimeout);
    sc_sess->var["aws.sqs.queue"] = lRes->getQueueUrl();
    sc_sess->CLR_ERRNO;
  } catch (const AWSException &e) {
    WARN("SQS create failed: '%s'\n", e.what());
    sc_sess->var["aws.ereason"] = e.what();
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_CREATE);
    sc_sess->SET_STRERROR(e.what());
  }
  FREE_CONNECTION_SQS;

} EXEC_ACTION_END;

EXEC_ACTION_START(SCSQSDeleteQueueAction) {
  CHECK_PRELOAD_SQS;
  GET_QUEUE_ARG(arg);

  GET_CONNECTION_SQS;
  try {
    DeleteQueueResponsePtr lRes = aSQS->deleteQueue (sqs_queue);
    sc_sess->CLR_ERRNO;
  } catch (const AWSException &e) {
    WARN("SQS delete failed: '%s'\n", e.what());
    sc_sess->var["aws.ereason"] = e.what();
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_DELETE);
    sc_sess->SET_STRERROR(e.what());
  }
  FREE_CONNECTION_SQS;

} EXEC_ACTION_END;

CONST_ACTION_2P(SCSQSSendMessageAction, ',', true);
EXEC_ACTION_START(SCSQSSendMessageAction) {
  CHECK_PRELOAD_SQS;

  GET_QUEUE_ARG(par2);
  // todo: see whether it makes sense to not copy to optimize this
  string message = resolveVars(par1, sess, sc_sess, event_params);
  GET_CONNECTION_SQS;
  try {
    SendMessageResponsePtr lRes = aSQS->sendMessage (sqs_queue, message);
    DBG("successfully sent message to SQS\n");
    DBG("   url: [%s]\n", lRes->getMessageId().c_str());
    DBG("   md5: [%s]\n", lRes->getMD5OfMessageBody().c_str());
    sc_sess->var["aws.sqs.url"] = lRes->getMessageId();
    sc_sess->var["aws.sqs.md5"] = lRes->getMD5OfMessageBody();
  } catch (const AWSException &e) {
    WARN("SQS send failed: '%s'\n", e.what());
    sc_sess->var["aws.ereason"] = e.what();
    sc_sess->SET_ERRNO(DSM_ERRNO_AWS_SEND);
    sc_sess->SET_STRERROR(e.what());
  }
  FREE_CONNECTION_SQS;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCSQSReceiveMessageAction) {
  CHECK_PRELOAD_SQS;
  GET_CONNECTION_SQS;
//   try {
//     ReceiveMessageResponsePtr lReceiveMessages = aSQS->receiveMessage (aQueueName, aMaxNbMessages, aVisibilityTimeout);
//     lReceiveMessages->open();
//     ReceiveMessageResponse::Message lMessage;
//     std::cout << "received messages:" << std::endl;
//       int lNb = 0;
//       while (lReceiveMessages->next (lMessage)) {
// 	std::cout << "  Message Nb. " << lNb++ << std::endl;
// 	std::cout << "    message-id: [" << lMessage.message_id << "]" << std::endl;
// 	std::cout << "    message-handle: [" << lMessage.receipt_handle << "]"<< std::endl;
// 	std::cout << "    message-md5: [" << lMessage.message_md5 << "]" << std::endl;
// 	std::cout << "    message-size: [" << lMessage.message_size << "]" << std::endl;
// 	std::cout << "    message-content: [";
// 	std::cout.write (lMessage.message_body, lMessage.message_size);
// 	std::cout << "]" << std::endl;
//       }
//       lReceiveMessages->close();
//   } catch (const AWSException &e) {
//     std::cerr << e.what() << std::endl;
//     return false;
//   }
  ERROR("TODO\n");
  FREE_CONNECTION_SQS;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCSQSDeleteMessageAction) {
  CHECK_PRELOAD_SQS;
  GET_CONNECTION_SQS;
  ERROR("TODO\n");
  FREE_CONNECTION_SQS;
} EXEC_ACTION_END;
