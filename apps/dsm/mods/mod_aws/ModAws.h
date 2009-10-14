/*
 * $Id$
 *
 * Copyright (C) 2009 TelTech Systems
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#ifndef _MOD_AWS_H
#define _MOD_AWS_H
#include "DSMModule.h"
#include "DSMSession.h"

#include <libaws/aws.h>
using namespace aws;

#define DSM_ERRNO_AWS_CONN    "50"
#define DSM_ERRNO_AWS_PUT     "51"
#define DSM_ERRNO_AWS_CREATE  "52"
#define DSM_ERRNO_AWS_DELETE  "53"
#define DSM_ERRNO_AWS_SEND    "54"

#define MOD_CLS_NAME SCAwsModule

DECLARE_MODULE_BEGIN(MOD_CLS_NAME);

/* class SCAwsModule  */
/* : public DSMModule { */

/*  public: */
/*   SCAwsModule(); */
/*   ~SCAwsModule(); */
  
/*   DSMAction* getAction(const string& from_str); */
/*   DSMCondition* getCondition(const string& from_str); */

  int preload();

  static ConnectionPool<S3ConnectionPtr>* s3ConnectionPool;
  static ConnectionPool<SQSConnectionPtr>* sqsConnectionPool;
DECLARE_MODULE_END;
/* }; */


DEF_ACTION_1P(SCS3CreateBucketAction);
DEF_ACTION_2P(SCS3PutFileAction);
DEF_ACTION_2P(SCS3PutMultiFileAction);

DEF_ACTION_2P(SCSQSCreateQueueAction);
DEF_ACTION_1P(SCSQSDeleteQueueAction);
DEF_ACTION_2P(SCSQSSendMessageAction);
DEF_ACTION_1P(SCSQSReceiveMessageAction);
DEF_ACTION_1P(SCSQSDeleteMessageAction);


/* DEF_ACTION_1P(SCMyConnectAction); */
/* DEF_SCCondition(MyHasResultCondition); */

#endif
