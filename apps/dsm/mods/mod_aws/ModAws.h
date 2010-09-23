/*
 * Copyright (C) 2009 TelTech Systems
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
#ifndef _MOD_AWS_H
#define _MOD_AWS_H
#include "DSMModule.h"
#include "DSMSession.h"

#include <libaws/aws.h>
using namespace aws;

#define DSM_ERRNO_AWS_CONN    "aws_conn"
#define DSM_ERRNO_AWS_PUT     "aws_put"
#define DSM_ERRNO_AWS_CREATE  "aws_create"
#define DSM_ERRNO_AWS_DELETE  "aws_delete"
#define DSM_ERRNO_AWS_SEND    "aws_send"

#define MOD_CLS_NAME SCAwsModule

DECLARE_MODULE_BEGIN(MOD_CLS_NAME);
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
