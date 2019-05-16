/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmMail.h"
#include "AmSmtpClient.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "log.h"
#include "AnswerMachine.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


AmMail::AmMail(const string& _from, const string& _subject,
	       const string& _to, const string& _body, 
	       const string& _header)
  : from(_from), subject(_subject), body(_body), to(_to),
    header(_header), clean_up(0), error_count(0)
{
}

AmMail::~AmMail()
{
  for(Attachements::iterator it = attachements.begin();
      it != attachements.end(); it++){

    fclose(it->fp);
  }
};

AmMailDeamon* AmMailDeamon::_instance=0;

AmMailDeamon* AmMailDeamon::instance()
{
  if(!_instance)
    _instance = new AmMailDeamon();
  return _instance;
}

void AmMailDeamon::on_stop()
{
}

int AmMailDeamon::sendQueued(AmMail* mail)
{
  if(mail->from.empty() || mail->to.empty()){
    ERROR("mail.from('%s') or mail.to('%s') is empty\n",mail->from.c_str(),mail->to.c_str());
    return -1;
  }

  //     FILE* tst_fp;
  //     for( Attachements::const_iterator att_it = mail->attachements.begin();
  // 	 att_it != mail->attachements.end(); ++att_it ){
	
  // 	if(!(tst_fp = fopen(att_it->fullname.c_str(),"r"))){
  // 	    ERROR("%s\n",strerror(errno));
  // 	    return -1;
  // 	}
  // 	else
  // 	    fclose(tst_fp);
  //     }

  event_fifo_mut.lock();
  event_fifo.push(mail);
  event_fifo_mut.unlock();
  _run_cond.set(true);
  return 0;
}

void AmMailDeamon::run()
{
  std::queue<AmMail*> n_event_fifo;
  while(1){

    _run_cond.wait_for();
    sleep(5);

    AmSmtpClient smtp;
    if (smtp.connect(AnswerMachineFactory::SmtpServerAddress,
		     AnswerMachineFactory::SmtpServerPort)) {
	    
      WARN("Mail deamon could not connect to SMTP server at <%s:%i>\n",
	   AnswerMachineFactory::SmtpServerAddress.c_str(),
	   AnswerMachineFactory::SmtpServerPort);
      continue;
    }

    event_fifo_mut.lock();
    DBG("Mail deamon starting its work\n");

    while(!event_fifo.empty()){

      AmMail* cur_mail = event_fifo.front();
      event_fifo.pop();

      event_fifo_mut.unlock();

      bool err = true;
      try{
	err = smtp.send(*cur_mail);
      }
      catch(...){}

      if(err && (cur_mail->error_count < 3)){
	n_event_fifo.push(cur_mail);
	cur_mail->error_count++;
      }
      else {
	// todo: save messages with errors somewhere? 
	if(cur_mail->clean_up)
	  (*(cur_mail->clean_up))(cur_mail);
	delete cur_mail;
      }
      event_fifo_mut.lock();
    }

    smtp.disconnect();
    smtp.close();

    if(n_event_fifo.empty()){
      _run_cond.set(false);
    } else {
      // requeue unsent mail
      while(!n_event_fifo.empty()) {
	event_fifo.push(n_event_fifo.front());
	n_event_fifo.pop();
      }
    }

    event_fifo_mut.unlock();

    DBG("Mail deamon finished\n");
  }
}







