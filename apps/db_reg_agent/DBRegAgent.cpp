/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "DBRegAgent.h"
#include "AmSession.h"
#include "AmEventDispatcher.h"

#include <unistd.h>
#include <stdlib.h>

EXPORT_MODULE_FACTORY(DBRegAgent);
DEFINE_MODULE_INSTANCE(DBRegAgent, MOD_NAME);

mysqlpp::Connection DBRegAgent::MainDBConnection(mysqlpp::use_exceptions);
mysqlpp::Connection DBRegAgent::ProcessorDBConnection(mysqlpp::use_exceptions);

string DBRegAgent::joined_query;
string DBRegAgent::registrations_table = "registrations";

double DBRegAgent::reregister_interval = 0.5;
double DBRegAgent::minimum_reregister_interval = -1;

bool DBRegAgent::enable_ratelimiting = false;
unsigned int DBRegAgent::ratelimit_rate = 0;
unsigned int DBRegAgent::ratelimit_per = 0;
bool DBRegAgent::ratelimit_slowstart = false;

bool DBRegAgent::delete_removed_registrations = true;
bool DBRegAgent::delete_failed_deregistrations = false;
bool DBRegAgent::save_contacts = true;
bool DBRegAgent::db_read_contact = false;
string DBRegAgent::contact_hostport;
string DBRegAgent::outbound_proxy;
bool DBRegAgent::save_auth_replies = false;

unsigned int DBRegAgent::error_retry_interval = 300;

static void _timer_cb(RegTimer* timer, long subscriber_id, int data2) {
  DBRegAgent::instance()->timer_cb(timer, subscriber_id, data2);
}

DBRegAgent::DBRegAgent(const string& _app_name)
  : AmDynInvokeFactory(_app_name),
    AmEventQueue(this),
    uac_auth_i(NULL)
{
}

DBRegAgent::~DBRegAgent() {
}

int DBRegAgent::onLoad()
{

  DBG("loading db_reg_agent....\n");

  AmDynInvokeFactory* uac_auth_f = AmPlugIn::instance()->getFactory4Di("uac_auth");
  if (uac_auth_f == NULL) {
    WARN("unable to get a uac_auth factory. "
	 "registrations will not be authenticated.\n");
    WARN("(do you want to load uac_auth module?)\n");
  } else {
    uac_auth_i = uac_auth_f->getInstance();
  }

  AmConfigReader cfg;
  if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1, MOD_NAME ".conf")))
    return -1;

  expires = cfg.getParameterInt("expires", 7200);
  DBG("requesting registration expires of %u seconds\n", expires);

  if (cfg.hasParameter("reregister_interval")) {
    reregister_interval = -1;
    reregister_interval = atof(cfg.getParameter("reregister_interval").c_str());
    if (reregister_interval <= 0 || reregister_interval > 1) {
      ERROR("configuration value 'reregister_interval' could not be read. "
	    "needs to be 0 .. 1.0 (recommended: 0.5)\n");
      return -1;
    }
  }

  if (cfg.hasParameter("minimum_reregister_interval")) {
    minimum_reregister_interval = -1;
    minimum_reregister_interval = atof(cfg.getParameter("minimum_reregister_interval").c_str());
    if (minimum_reregister_interval <= 0 || minimum_reregister_interval > 1) {
      ERROR("configuration value 'minimum_reregister_interval' could not be read. "
	    "needs to be 0 .. reregister_interval (recommended: 0.4)\n");
      return -1;
    }

    if (minimum_reregister_interval >= reregister_interval) {
      ERROR("configuration value 'minimum_reregister_interval' must be smaller "
	    "than reregister_interval (recommended: 0.4)\n");
      return -1;
    }
  }

  enable_ratelimiting = cfg.getParameter("enable_ratelimiting") == "yes";
  if (enable_ratelimiting) {
    if (!cfg.hasParameter("ratelimit_rate") || !cfg.hasParameter("ratelimit_per")) {
      ERROR("if ratelimiting is enabled, ratelimit_rate and ratelimit_per must be set\n");
      return -1;
    }
    ratelimit_rate = cfg.getParameterInt("ratelimit_rate", 0);
    ratelimit_per = cfg.getParameterInt("ratelimit_per", 0);
    if (!ratelimit_rate || !ratelimit_per) {
      ERROR("ratelimit_rate and ratelimit_per must be > 0\n");
      return -1;
    }
    ratelimit_slowstart = cfg.getParameter("ratelimit_slowstart") == "yes";

  }

  delete_removed_registrations =
    cfg.getParameter("delete_removed_registrations", "yes") == "yes";

  delete_failed_deregistrations =
    cfg.getParameter("delete_failed_deregistrations", "no") == "yes";

  save_contacts =
    cfg.getParameter("save_contacts", "yes") == "yes";

  db_read_contact =
    cfg.getParameter("db_read_contact", "no") == "yes";

  save_auth_replies =
    cfg.getParameter("save_auth_replies", "no") == "yes";

  contact_hostport = cfg.getParameter("contact_hostport");

  outbound_proxy = cfg.getParameter("outbound_proxy");

  error_retry_interval = cfg.getParameterInt("error_retry_interval", 300);
  if (!error_retry_interval) {
    WARN("disabled retry on errors!\n");
  }

  string mysql_server, mysql_user, mysql_passwd, mysql_db;

  mysql_server = cfg.getParameter("mysql_server", "localhost");

  mysql_user = cfg.getParameter("mysql_user");
  if (mysql_user.empty()) {
    ERROR(MOD_NAME ".conf parameter 'mysql_user' is missing.\n");
    return -1;
  }

  mysql_passwd = cfg.getParameter("mysql_passwd");
  if (mysql_passwd.empty()) {
    ERROR(MOD_NAME ".conf parameter 'mysql_passwd' is missing.\n");
    return -1;
  }

  mysql_db = cfg.getParameter("mysql_db", "sems");

  try {

    MainDBConnection.set_option(new mysqlpp::ReconnectOption(true));
    // matched instead of changed rows in result, so we know when to create DB entry
    MainDBConnection.set_option(new mysqlpp::FoundRowsOption(true));
    MainDBConnection.connect(mysql_db.c_str(), mysql_server.c_str(),
                      mysql_user.c_str(), mysql_passwd.c_str());
    if (!MainDBConnection) {
      ERROR("Database connection failed: %s\n", MainDBConnection.error());
      return -1;
    }

    ProcessorDBConnection.set_option(new mysqlpp::ReconnectOption(true));
    // matched instead of changed rows in result, so we know when to create DB entry
    ProcessorDBConnection.set_option(new mysqlpp::FoundRowsOption(true));
    ProcessorDBConnection.connect(mysql_db.c_str(), mysql_server.c_str(),
                      mysql_user.c_str(), mysql_passwd.c_str());
    if (!ProcessorDBConnection) {
      ERROR("Database connection failed: %s\n", ProcessorDBConnection.error());
      return -1;
    }

  } catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return -1;
  }

  // register us as SIP event receiver for MOD_NAME
  AmEventDispatcher::instance()->addEventQueue(MOD_NAME,this);

  if (!AmPlugIn::registerDIInterface(MOD_NAME, this)) {
    ERROR("registering "MOD_NAME" DI interface\n");
    return -1;
  }

  joined_query = cfg.getParameter("joined_query");
  if (joined_query.empty()) {
    // todo: name!
    ERROR("joined_query must be set\n");
    return -1;
  }

  if (cfg.hasParameter("registrations_table")) {
    registrations_table = cfg.getParameter("registrations_table");
  }
  DBG("using registrations table '%s'\n", registrations_table.c_str());

  if (!loadRegistrations()) {
    ERROR("loading registrations from DB\n");
    return -1;
  }

  DBG("starting registration timer thread...\n");
  registration_scheduler.start();

  // run_tests();

  start();

  return 0;
}

void DBRegAgent::onUnload() {
  DBG("closing main DB connection\n");
  MainDBConnection.disconnect();
  DBG("closing auxiliary DB connection\n");
  ProcessorDBConnection.disconnect();
}

bool DBRegAgent::loadRegistrations() {
  try {
    time_t now_time = time(NULL);

    mysqlpp::Query query = DBRegAgent::MainDBConnection.query();

    string query_string, table;

    query_string = joined_query;

    DBG("querying all registrations with : '%s'\n",
	query_string.c_str());

    query << query_string;
    mysqlpp::UseQueryResult res = query.use();
    
    // mysqlpp::Row::size_type row_count = res.num_rows();
    // DBG("got %zd subscriptions\n", row_count);

    while (mysqlpp::Row row = res.fetch_row()) {
      int status = 0; 
      long subscriber_id = row[COLNAME_SUBSCRIBER_ID];

      string contact_uri;
      if (db_read_contact && row[COLNAME_CONTACT] != mysqlpp::null) {
	contact_uri = (string) row[COLNAME_CONTACT];
      }

      if (row[COLNAME_STATUS] != mysqlpp::null)
	status = row[COLNAME_STATUS];
      else {
	DBG("registration status entry for id %ld does not exist, creating...\n",
	    subscriber_id);
	createDBRegistration(subscriber_id, ProcessorDBConnection);
      }

      DBG("got subscriber '%s@%s' status %i\n",
	  string(row[COLNAME_USER]).c_str(), string(row[COLNAME_REALM]).c_str(),
	  status);      

      switch (status) {
      case REG_STATUS_INACTIVE:
      case REG_STATUS_PENDING: // try again
      case REG_STATUS_FAILED:  // try again
	{
	  createRegistration(subscriber_id,
			     (string)row[COLNAME_USER],
			     (string)row[COLNAME_PASS],
			     (string)row[COLNAME_REALM],
			     contact_uri
			     );
	  scheduleRegistration(subscriber_id);
	}; break;

      case REG_STATUS_ACTIVE:
	{
	  createRegistration(subscriber_id,
			     (string)row[COLNAME_USER],
			     (string)row[COLNAME_PASS],
			     (string)row[COLNAME_REALM],
			     contact_uri
			     );

	  time_t dt_expiry = now_time;
	  if (row[COLNAME_EXPIRY] != mysqlpp::null) {
	    dt_expiry = (time_t)((mysqlpp::DateTime)row[COLNAME_EXPIRY]);
	  }

	  time_t dt_registration_ts = now_time;
	  if (row[COLNAME_REGISTRATION_TS] != mysqlpp::null) {
	    dt_registration_ts = (time_t)((mysqlpp::DateTime)row[COLNAME_REGISTRATION_TS]);
	  }

	  DBG("got expiry '%ld, registration_ts %ld, now %ld'\n",
	      dt_expiry, dt_registration_ts, now_time);

	  if (dt_registration_ts > now_time) {
	    WARN("needed to sanitize last_registration timestamp TS from the %ld (now %ld) - "
		 "DB host time mismatch?\n", dt_registration_ts, now_time);
	    dt_registration_ts = now_time;
	  }

	  // if expired add to pending registrations, else schedule re-regstration
	  if (dt_expiry <= now_time) {
	    DBG("scheduling imminent re-registration for subscriber %ld\n", subscriber_id);
	    scheduleRegistration(subscriber_id);
	  } else {
	    setRegistrationTimer(subscriber_id, dt_expiry, dt_registration_ts, now_time);
	  }
	  
	}; break;
      case REG_STATUS_REMOVED:
	{
	  DBG("ignoring removed registration %ld %s@%s", subscriber_id,
	      ((string)row[COLNAME_USER]).c_str(), ((string)row[COLNAME_REALM]).c_str());
	} break;

      case REG_STATUS_TO_BE_REMOVED:
	{
	  DBG("Scheduling Deregister of registration %ld %s@%s", subscriber_id,
	      ((string)row[COLNAME_USER]).c_str(), ((string)row[COLNAME_REALM]).c_str());
	  createRegistration(subscriber_id,
			     (string)row[COLNAME_USER],
			     (string)row[COLNAME_PASS],
			     (string)row[COLNAME_REALM],
			     contact_uri
			     );
	  scheduleDeregistration(subscriber_id);
	};
      }
    }


  } catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return false;
  }

  return true;
}

/** create registration in our list */
void DBRegAgent::createRegistration(long subscriber_id,
				    const string& user,
				    const string& pass,
				    const string& realm,
				    const string& contact) {

  string contact_uri = contact;
  if (contact_uri.empty() && !contact_hostport.empty()) {
    contact_uri = "sip:"+ user + "@" + contact_hostport;
  }

  string handle = AmSession::getNewId();
  SIPRegistrationInfo reg_info(realm, user,
			       user, // name
			       user, // auth_user
			       pass,
			       outbound_proxy, // proxy
			       contact_uri // contact
			       );

  registrations_mut.lock();
  try {
    if (registrations.find(subscriber_id) != registrations.end()) {
      registrations_mut.unlock();
      WARN("registration with ID %ld already exists, removing\n", subscriber_id);
      removeRegistration(subscriber_id);
      clearRegistrationTimer(subscriber_id);
      registrations_mut.lock();
    }

    AmSIPRegistration* reg = new AmSIPRegistration(handle, reg_info, "" /*MOD_NAME*/);
    reg->setExpiresInterval(expires);

    registrations[subscriber_id] = reg;
    registration_ltags[handle] = subscriber_id;

    if (NULL != uac_auth_i) {
      DBG("enabling UAC Auth for new registration.\n");
      
      // get a sessionEventHandler from uac_auth
      AmArg di_args,ret;
      AmArg a;
      a.setBorrowedPointer(reg);
      di_args.push(a);
      di_args.push(a);
      
      uac_auth_i->invoke("getHandler", di_args, ret);
      if (!ret.size()) {
	ERROR("Can not add auth handler to new registration!\n");
      } else {
	AmObject* p = ret.get(0).asObject();
	if (p != NULL) {
	  AmSessionEventHandler* h = dynamic_cast<AmSessionEventHandler*>(p);	
	  if (h != NULL)
	    reg->setSessionEventHandler(h);
	}
      }
    }
  } catch (const AmArg::OutOfBoundsException& e) {
    ERROR("OutOfBoundsException");
  } catch (const AmArg::TypeMismatchException& e) {
    ERROR("TypeMismatchException");
  } catch (...) {
    ERROR("unknown exception occured\n");
  }

  registrations_mut.unlock();

  // register us as SIP event receiver for this ltag
  AmEventDispatcher::instance()->addEventQueue(handle,this);

  DBG("created new registration with ID %ld and ltag '%s'\n",
      subscriber_id, handle.c_str());
}

void DBRegAgent::updateRegistration(long subscriber_id,
				    const string& user,
				    const string& pass,
				    const string& realm,
				    const string& contact) {

  registrations_mut.lock();
  map<long, AmSIPRegistration*>::iterator it=registrations.find(subscriber_id);
  if (it == registrations.end()) {
    registrations_mut.unlock();
    WARN("updateRegistration - registration %ld %s@%s unknown, creating\n",
	 subscriber_id, user.c_str(), realm.c_str());
    createRegistration(subscriber_id, user, pass, realm, contact);
    scheduleRegistration(subscriber_id);
    return;
  }

  bool need_reregister = it->second->getInfo().domain != realm
    || it->second->getInfo().user != user
    || it->second->getInfo().contact != contact;

  string old_realm = it->second->getInfo().domain;
  string old_user = it->second->getInfo().user;
  it->second->setRegistrationInfo(SIPRegistrationInfo(realm, user,
						      user, // name
						      user, // auth_user
						      pass,
						      outbound_proxy,   // proxy
						      contact)); // contact
  registrations_mut.unlock();
  if (need_reregister) {
    DBG("user/realm for registration %ld changed (%s@%s -> %s@%s). "
	"Triggering immediate re-registration\n",
	subscriber_id, old_user.c_str(), old_realm.c_str(), user.c_str(), realm.c_str());
    scheduleRegistration(subscriber_id);
  }
}

/** remove registration from our list */
void DBRegAgent::removeRegistration(long subscriber_id) {
  bool res = false;
  string handle;
  registrations_mut.lock();
  map<long, AmSIPRegistration*>::iterator it = registrations.find(subscriber_id);
  if (it != registrations.end()) {
    handle = it->second->getHandle();
    registration_ltags.erase(handle);
    delete it->second;
    registrations.erase(it);
    res = true;
  }
  registrations_mut.unlock();

  if (res) {
    // deregister us as SIP event receiver for this ltag
    AmEventDispatcher::instance()->delEventQueue(handle);

    DBG("removed registration with ID %ld\n", subscriber_id);
  } else {
    DBG("registration with ID %ld not found for removing\n", subscriber_id);
  }
}

/** schedule this registration to REGISTER (immediately) */
void DBRegAgent::scheduleRegistration(long subscriber_id) {
  if (enable_ratelimiting) {
    registration_processor.
      postEvent(new RegistrationActionEvent(RegistrationActionEvent::Register,
					    subscriber_id));
  } else {
    // use our own thread
    postEvent(new RegistrationActionEvent(RegistrationActionEvent::Register,
					  subscriber_id));
  }
  DBG("added to pending actions: REGISTER of %ld\n", subscriber_id);
}

/** schedule this registration to de-REGISTER (immediately) */
void DBRegAgent::scheduleDeregistration(long subscriber_id) {
  if (enable_ratelimiting) {
    registration_processor.
      postEvent(new RegistrationActionEvent(RegistrationActionEvent::Deregister,
					    subscriber_id));
  } else {
    // use our own thread
      postEvent(new RegistrationActionEvent(RegistrationActionEvent::Deregister,
					    subscriber_id));
  }
  DBG("added to pending actions: DEREGISTER of %ld\n", subscriber_id);
}

void DBRegAgent::process(AmEvent* ev) {

  if (ev->event_id == RegistrationActionEventID) {
    RegistrationActionEvent* reg_action_ev =
      dynamic_cast<RegistrationActionEvent*>(ev);
    if (reg_action_ev) {
      onRegistrationActionEvent(reg_action_ev);
      return;
    }
  }

  AmSipReplyEvent* sip_rep = dynamic_cast<AmSipReplyEvent*>(ev);
  if (sip_rep) {
      onSipReplyEvent(sip_rep);
    return;
  }

  if (ev->event_id == E_SYSTEM) {
    AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(ev);
    if(sys_ev){	
      DBG("Session received system Event\n");
      if (sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
	running = false;
	registration_scheduler._timer_thread_running = false;
      }
      return;
    }
  }

  ERROR("unknown event received!\n");
}

// uses ProcessorDBConnection
void DBRegAgent::onRegistrationActionEvent(RegistrationActionEvent* reg_action_ev) {
  switch (reg_action_ev->action) {
  case RegistrationActionEvent::Register:
    {
      DBG("REGISTER of registration %ld\n", reg_action_ev->subscriber_id);
      registrations_mut.lock();
      map<long, AmSIPRegistration*>::iterator it=
	registrations.find(reg_action_ev->subscriber_id);
      if (it==registrations.end()) {
	DBG("ignoring scheduled REGISTER of unknown registration %ld\n",
	    reg_action_ev->subscriber_id);
      } else {
	if (!it->second->doRegistration()) {
	  updateDBRegistration(ProcessorDBConnection,
			       reg_action_ev->subscriber_id,
			       480, ERR_REASON_UNABLE_TO_SEND_REQUEST,
			       true, REG_STATUS_FAILED);
	  if (error_retry_interval) {
	    // schedule register-refresh after error_retry_interval
	    setRegistrationTimer(reg_action_ev->subscriber_id, error_retry_interval,
				 RegistrationActionEvent::Register);
	  }
	}
      }
      registrations_mut.unlock();
    } break;
  case RegistrationActionEvent::Deregister:
    {
      DBG("De-REGISTER of registration %ld\n", reg_action_ev->subscriber_id);
      registrations_mut.lock();
      map<long, AmSIPRegistration*>::iterator it=
	registrations.find(reg_action_ev->subscriber_id);
      if (it==registrations.end()) {
	DBG("ignoring scheduled De-REGISTER of unknown registration %ld\n",
	    reg_action_ev->subscriber_id);
      } else {
	if (!it->second->doUnregister()) {
	  if (delete_removed_registrations && delete_failed_deregistrations) {
	    DBG("sending de-Register failed - deleting registration %ld "
		"(delete_failed_deregistrations=yes)\n", reg_action_ev->subscriber_id);
	    deleteDBRegistration(reg_action_ev->subscriber_id, ProcessorDBConnection);
	  } else {
	    DBG("failed sending de-register, updating DB with REG_STATUS_TO_BE_REMOVED "
		ERR_REASON_UNABLE_TO_SEND_REQUEST "for subscriber %ld\n",
		reg_action_ev->subscriber_id);
	    updateDBRegistration(ProcessorDBConnection,
				 reg_action_ev->subscriber_id,
				 480, ERR_REASON_UNABLE_TO_SEND_REQUEST,
				 true, REG_STATUS_TO_BE_REMOVED);
	    // don't re-try de-registrations if sending failed
	  // if (error_retry_interval) {
	  //   // schedule register-refresh after error_retry_interval
	  //   setRegistrationTimer(reg_action_ev->subscriber_id, error_retry_interval,
	  // 			 RegistrationActionEvent::Deregister);
	  // }
	  }
	}
      }
      registrations_mut.unlock();
    } break;
  }
}

void DBRegAgent::createDBRegistration(long subscriber_id, mysqlpp::Connection& conn) {
  string insert_query = "insert into "+registrations_table+
    " (subscriber_id) values ("+
    long2str(subscriber_id)+");";

  try {
    mysqlpp::Query query = conn.query();
    query << insert_query;

    mysqlpp::SimpleResult res = query.execute();
    if (!res) {
      WARN("creating registration in DB with query '%s' failed: '%s'\n",
	   insert_query.c_str(), res.info());
    }
  }  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return;
  }
}

void DBRegAgent::deleteDBRegistration(long subscriber_id, mysqlpp::Connection& conn) {
  string insert_query = "delete from "+registrations_table+
    " where subscriber_id=" +  long2str(subscriber_id)+";";

  try {
    mysqlpp::Query query = conn.query();
    query << insert_query;

    mysqlpp::SimpleResult res = query.execute();
    if (!res) {
      WARN("removing registration in DB with query '%s' failed: '%s'\n",
	   insert_query.c_str(), res.info());
    }
  }  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return;
  }
}

void DBRegAgent::updateDBRegistration(mysqlpp::Connection& db_connection,
				      long subscriber_id, int last_code,
				      const string& last_reason,
				      bool update_status, int status,
				      bool update_ts, unsigned int expiry,
				      bool update_contacts, const string& contacts) {
  try {

    mysqlpp::Query query = db_connection.query();

    query << "update "+registrations_table+" set last_code="+ int2str(last_code) +", ";
    query << "last_reason=";
    query << mysqlpp::quote << last_reason;

    if (update_status) {
      query <<  ", registration_status="+int2str(status);
    }

    if (update_ts) {
      query << ", last_registration=NOW(), "
	"expiry=TIMESTAMPADD(SECOND,"+int2str(expiry)+", NOW())";
    }

    if (update_contacts) {
      query << ", contacts=" << mysqlpp::quote << contacts;
    }

    query << " where " COLNAME_SUBSCRIBER_ID "="+long2str(subscriber_id) + ";";
    string query_str = query.str();
    DBG("updating registration in DB with query '%s'\n", query_str.c_str());

    mysqlpp::SimpleResult res = query.execute();
    if (!res) {
      WARN("updating registration in DB with query '%s' failed: '%s'\n",
	   query_str.c_str(), res.info());
    } else {
      if (!res.rows()) {
	// should not happen - DB entry is created on load or on createRegistration
	DBG("creating registration DB entry for subscriber %ld\n", subscriber_id);
	createDBRegistration(subscriber_id, db_connection);
	query.reset();
	query << query_str;

	mysqlpp::SimpleResult res = query.execute();
	if (!res || !res.rows()) {
	  WARN("updating registration in DB with query '%s' failed: '%s'\n",
	       query_str.c_str(), res.info());
	}
      }
    }

  }  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return;
  }

}

// uses MainDBConnection
void DBRegAgent::onSipReplyEvent(AmSipReplyEvent* ev) {
  if (!ev) return;

  DBG("received SIP reply event for '%s'\n", 
#ifdef HAS_OFFER_ANSWER
      ev->reply.from_tag.c_str()
#else
      ev->reply.local_tag.c_str()
#endif
      );
  registrations_mut.lock();

  string local_tag =
#ifdef HAS_OFFER_ANSWER
    ev->reply.from_tag;
#else
    ev->reply.local_tag;
#endif
    
  map<string, long>::iterator it=registration_ltags.find(local_tag);
  if (it!=registration_ltags.end()) {
    long subscriber_id = it->second;
    map<long, AmSIPRegistration*>::iterator r_it=registrations.find(subscriber_id);
    if (r_it != registrations.end()) {
      AmSIPRegistration* registration = r_it->second;
      if (!registration) {
	ERROR("Internal error: registration object missing\n");
	return;
      }
      unsigned int cseq_before = registration->getDlg()->cseq;

#ifdef HAS_OFFER_ANSWER
      registration->getDlg()->onRxReply(ev->reply);
#else
      registration->getDlg()->updateStatus(ev->reply);
#endif

      //update registrations set 
      bool update_status = false;
      int status = 0;
      bool update_ts = false;
      unsigned int expiry = 0;
      bool delete_status = false;
      bool auth_pending = false;

      if (ev->reply.code >= 300) {
	// REGISTER or de-REGISTER failed
	if ((ev->reply.code == 401 || ev->reply.code == 407) &&
	    // auth response codes
	    // processing reply triggered sending request: resent by auth
	    (cseq_before != registration->getDlg()->cseq)) {
	  DBG("received negative reply, but still in pending state (auth).\n");
	  auth_pending = true;
	} else {
	  if (!registration->getUnregistering()) {
	    // REGISTER failed - mark in DB
	    DBG("registration failed - mark in DB\n");
	    update_status = true;
	    status = REG_STATUS_FAILED;
	    if (error_retry_interval) {
	      // schedule register-refresh after error_retry_interval
	      setRegistrationTimer(subscriber_id, error_retry_interval,
				   RegistrationActionEvent::Register);
	    }
	  } else {
	    // de-REGISTER failed
	    if (delete_removed_registrations && delete_failed_deregistrations) {
	      DBG("de-Register failed - deleting registration %ld "
		  "(delete_failed_deregistrations=yes)\n", subscriber_id);
	      delete_status = true;
	    } else {
	      update_status = true;
	      status = REG_STATUS_TO_BE_REMOVED;
	    }
	  }
	}
      } else if (ev->reply.code >= 200) {
	// positive reply
	if (!registration->getUnregistering()) {
	  time_t now_time = time(0);
	  setRegistrationTimer(subscriber_id, registration->getExpiresTS(),
			       now_time, now_time);

	  update_status = true;
	  status = REG_STATUS_ACTIVE;

	  update_ts = true;
	  expiry = registration->getExpiresLeft();
	} else {
	  if (delete_removed_registrations) {
	    delete_status = true;
	  } else {
	    update_status = true;
	    status = REG_STATUS_REMOVED;
	  }
	}
      }

      // skip provisional replies & auth
      if (ev->reply.code >= 200 && !auth_pending) {
	// remove unregistered
	if (registration->getUnregistering()) {
	  registrations_mut.unlock();
	  removeRegistration(subscriber_id);
	  registrations_mut.lock();
	}
      }

      if (!delete_status) {
	if (auth_pending && !save_auth_replies) {
	  DBG("not updating DB with auth reply %u %s\n",
	      ev->reply.code, ev->reply.reason.c_str());
	} else {
	  DBG("update DB with reply %u %s\n", ev->reply.code, ev->reply.reason.c_str());
	  updateDBRegistration(MainDBConnection,
			       subscriber_id, ev->reply.code, ev->reply.reason,
			       update_status, status, update_ts, expiry,
			       save_contacts, ev->reply.contact);
	}
      } else {
	DBG("delete DB registration of subscriber %ld\n", subscriber_id);
	deleteDBRegistration(subscriber_id, MainDBConnection);
      }

    } else {
      ERROR("internal: inconsistent registration list\n");
    }
  } else {
    DBG("ignoring reply for unknown registration\n");
  }
  registrations_mut.unlock();
}

void DBRegAgent::run() {
  running = true;

  DBG("DBRegAgent thread: waiting 2 sec for server startup ...\n");
  sleep(2);
  
  mysqlpp::Connection::thread_start();

  if (enable_ratelimiting) {
    DBG("starting processor thread\n");
    registration_processor.start();
  }

  DBG("running DBRegAgent thread...\n");
  while (running) {
    processEvents();

    usleep(1000); // 1ms
  }

  DBG("DBRegAgent done, removing all registrations from Event Dispatcher...\n");
  registrations_mut.lock();
  for (map<string, long>::iterator it=registration_ltags.begin();
       it != registration_ltags.end(); it++) {
    AmEventDispatcher::instance()->delEventQueue(it->first);
  }
  registrations_mut.unlock();

  DBG("removing "MOD_NAME" registrations from Event Dispatcher...\n");
  AmEventDispatcher::instance()->delEventQueue(MOD_NAME);

  mysqlpp::Connection::thread_end();

  DBG("DBRegAgent thread stopped.\n");
}

void DBRegAgent::on_stop() {
  DBG("DBRegAgent on_stop()...\n");
  running = false;
}

void DBRegAgent::setRegistrationTimer(long subscriber_id, unsigned int timeout,
				      RegistrationActionEvent::RegAction reg_action) {
  DBG("setting Register timer for subscription %ld, timeout %u, reg_action %u\n",
      subscriber_id, timeout, reg_action);

  RegTimer* timer = NULL;
  map<long, RegTimer*>::iterator it=registration_timers.find(subscriber_id);
  if (it==registration_timers.end()) {
    DBG("timer object for subscription %ld not found\n", subscriber_id);
    timer = new RegTimer();
    timer->data1 = subscriber_id;
    timer->cb = _timer_cb;
    DBG("created timer object [%p] for subscription %ld\n", timer, subscriber_id);
  } else {
    timer = it->second;
    DBG("removing scheduled timer...\n");
    registration_scheduler.remove_timer(timer);
  }

  timer->data2 = reg_action;
  timer->expires = time(0) + timeout;

  DBG("placing timer for %ld in T-%u\n", subscriber_id, timeout);
  registration_scheduler.insert_timer(timer);

  registration_timers.insert(std::make_pair(subscriber_id, timer));

}

void DBRegAgent::setRegistrationTimer(long subscriber_id,
				      time_t expiry, time_t reg_start_ts,
				      time_t now_time) {
  DBG("setting re-Register timer for subscription %ld, expiry %ld, reg_start_t %ld\n",
      subscriber_id, expiry, reg_start_ts);

  RegTimer* timer = NULL;
  map<long, RegTimer*>::iterator it=registration_timers.find(subscriber_id);
  if (it==registration_timers.end()) {
    DBG("timer object for subscription %ld not found\n", subscriber_id);
    timer = new RegTimer();
    timer->data1 = subscriber_id;
    timer->cb = _timer_cb;
    DBG("created timer object [%p] for subscription %ld\n", timer, subscriber_id);
    registration_timers.insert(std::make_pair(subscriber_id, timer));
  } else {
    timer = it->second;
    DBG("removing scheduled timer...\n");
    registration_scheduler.remove_timer(timer);
  }

  timer->data2 = RegistrationActionEvent::Register;

  if (minimum_reregister_interval>0.0) {
    time_t t_expiry_max = reg_start_ts;
    time_t t_expiry_min = reg_start_ts;
    if (expiry > reg_start_ts)
      t_expiry_max+=(expiry - reg_start_ts) * reregister_interval;
    if (expiry > reg_start_ts)
      t_expiry_min+=(expiry - reg_start_ts) * minimum_reregister_interval;

    if (t_expiry_max < now_time) {
      // calculated interval completely in the past - immediate re-registration
      // by setting the timer to now
      t_expiry_max = now_time;
    }

    if (t_expiry_min > t_expiry_max)
      t_expiry_min = t_expiry_max;

    timer->expires = t_expiry_max;

    if (t_expiry_max == now_time) {
      // immediate re-registration
      DBG("calculated re-registration at TS <now> (%ld)"
	  "(reg_start_ts=%ld, reg_expiry=%ld, reregister_interval=%f, "
	  "minimum_reregister_interval=%f)\n",
	  t_expiry_max, reg_start_ts, expiry,
	  reregister_interval, minimum_reregister_interval);
      registration_scheduler.insert_timer(timer);
    } else {
      DBG("calculated re-registration at TS %ld .. %ld"
	  "(reg_start_ts=%ld, reg_expiry=%ld, reregister_interval=%f, "
	  "minimum_reregister_interval=%f)\n",
	  t_expiry_min, t_expiry_max, reg_start_ts, expiry,
	  reregister_interval, minimum_reregister_interval);
  
      registration_scheduler.insert_timer_leastloaded(timer, t_expiry_min, t_expiry_max);
    }
  } else {
    time_t t_expiry = reg_start_ts;
    if (expiry > reg_start_ts)
      t_expiry+=(expiry - reg_start_ts) * reregister_interval;

    if (t_expiry < now_time) {
      t_expiry = now_time;
      DBG("re-registering at TS <now> (%ld)\n", now_time);
    }

    DBG("calculated re-registration at TS %ld "
	"(reg_start_ts=%ld, reg_expiry=%ld, reregister_interval=%f)\n",
	t_expiry, reg_start_ts, expiry, reregister_interval);

    timer->expires = t_expiry;    
    registration_scheduler.insert_timer(timer);
  }
}

void DBRegAgent::clearRegistrationTimer(long subscriber_id) {
  DBG("removing timer for subscription %ld", subscriber_id);

  map<long, RegTimer*>::iterator it=registration_timers.find(subscriber_id);
  if (it==registration_timers.end()) {
    DBG("timer object for subscription %ld not found\n", subscriber_id);
      return;
  }
  DBG("removing timer [%p] from scheduler\n", it->second);
  registration_scheduler.remove_timer(it->second);

  DBG("deleting timer object [%p]\n", it->second);
  delete it->second;

  registration_timers.erase(it);
}

void DBRegAgent::removeRegistrationTimer(long subscriber_id) {
  DBG("removing timer object for subscription %ld", subscriber_id);

  map<long, RegTimer*>::iterator it=registration_timers.find(subscriber_id);
  if (it==registration_timers.end()) {
    DBG("timer object for subscription %ld not found\n", subscriber_id);
    return;
  }

  DBG("deleting timer object [%p]\n", it->second);
  delete it->second;

  registration_timers.erase(it);
}

void DBRegAgent::timer_cb(RegTimer* timer, long subscriber_id, int reg_action) {
  DBG("re-registration timer expired: subscriber %ld, timer=[%p], action %d\n",
      subscriber_id, timer, reg_action);

  registrations_mut.lock();
  removeRegistrationTimer(subscriber_id);
  registrations_mut.unlock();
  switch (reg_action) {
  case RegistrationActionEvent::Register:
    scheduleRegistration(subscriber_id); break;
  case RegistrationActionEvent::Deregister:
    scheduleDeregistration(subscriber_id); break;
  default: ERROR("internal: unknown reg_action %d for subscriber %ld timer event\n",
		 reg_action, subscriber_id);
  };
}


void DBRegAgent::DIcreateRegistration(int subscriber_id, const string& user, 
				      const string& pass, const string& realm,
				      const string& contact,
				      AmArg& ret) {
  DBG("DI method: createRegistration(%i, %s, %s, %s, %s)\n",
      subscriber_id, user.c_str(),
      pass.c_str(), realm.c_str(), contact.c_str());

  createRegistration(subscriber_id, user, pass, realm, contact);
  scheduleRegistration(subscriber_id);
  ret.push(200);
  ret.push("OK");
}

void DBRegAgent::DIupdateRegistration(int subscriber_id, const string& user, 
				      const string& pass, const string& realm,
				      const string& contact,
				      AmArg& ret) {
  DBG("DI method: updateRegistration(%i, %s, %s, %s)\n",
      subscriber_id, user.c_str(),
      pass.c_str(), realm.c_str());

  string contact_uri = contact;
  if (contact_uri.empty() && !contact_hostport.empty()) {
    contact_uri = "sip:"+ user + "@" + contact_hostport;
  }

  updateRegistration(subscriber_id, user, pass, realm, contact_uri);

  ret.push(200);
  ret.push("OK");
}

void DBRegAgent::DIremoveRegistration(int subscriber_id, AmArg& ret) {
  DBG("DI method: removeRegistration(%i)\n",
      subscriber_id);
  scheduleDeregistration(subscriber_id);

  registrations_mut.lock();
  clearRegistrationTimer(subscriber_id);
  registrations_mut.unlock();

  ret.push(200);
  ret.push("OK");
}

void DBRegAgent::DIrefreshRegistration(int subscriber_id, AmArg& ret) {
  DBG("DI method: refreshRegistration(%i)\n", subscriber_id);
  scheduleRegistration(subscriber_id);

  ret.push(200);
  ret.push("OK");
}

// ///////// DI API ///////////////////

void DBRegAgent::invoke(const string& method,
			const AmArg& args, AmArg& ret)
{
  if (method == "createRegistration"){
    args.assertArrayFmt("isss"); // subscriber_id, user, pass, realm
    string contact;
    if (args.size() > 4) {
      assertArgCStr(args.get(4));
      contact = args.get(4).asCStr();
    }
    DIcreateRegistration(args.get(0).asInt(), args.get(1).asCStr(), 
			 args.get(2).asCStr(),args.get(3).asCStr(),
			 contact, ret);
  } else if (method == "updateRegistration"){
    args.assertArrayFmt("isss"); // subscriber_id, user, pass, realm
    string contact;
    if (args.size() > 4) {
      assertArgCStr(args.get(4));
      contact = args.get(4).asCStr();
    }
    DIupdateRegistration(args.get(0).asInt(), args.get(1).asCStr(),
			 args.get(2).asCStr(),args.get(3).asCStr(),
			 contact, ret);
  } else if (method == "removeRegistration"){
    args.assertArrayFmt("i"); // subscriber_id
    DIremoveRegistration(args.get(0).asInt(), ret);
  } else if (method == "refreshRegistration"){
    args.assertArrayFmt("i"); // subscriber_id
    DIrefreshRegistration(args.get(0).asInt(), ret);
  }  else if(method == "_list"){
    ret.push(AmArg("createRegistration"));
    ret.push(AmArg("updateRegistration"));
    ret.push(AmArg("removeRegistration"));
    ret.push(AmArg("refreshRegistration"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}

// /////////////// processor thread /////////////////

DBRegAgentProcessorThread::DBRegAgentProcessorThread()
  : AmEventQueue(this), stopped(false) {
}

DBRegAgentProcessorThread::~DBRegAgentProcessorThread() {
}

void DBRegAgentProcessorThread::on_stop() {
}

void DBRegAgentProcessorThread::rateLimitWait() {
  DBG("applying rate limit %u initial requests per %us\n",
      DBRegAgent::ratelimit_rate, DBRegAgent::ratelimit_per);

  DBG("allowance before ratelimit: %f\n", allowance);

  struct timeval current;
  struct timeval time_passed;
  gettimeofday(&current, 0);
  timersub(&current, &last_check, &time_passed);
  memcpy(&last_check, &current, sizeof(struct timeval));
  double seconds_passed = (double)time_passed.tv_sec +
    (double)time_passed.tv_usec / 1000000.0;
  allowance += seconds_passed * 
    (double) DBRegAgent::ratelimit_rate / (double)DBRegAgent::ratelimit_per;

  if (allowance > (double)DBRegAgent::ratelimit_rate)
    allowance = (double)DBRegAgent::ratelimit_rate; // enough time passed, but limit to max
  if (allowance < 1.0) {
    useconds_t sleep_time = 1000000.0 * (1.0 - allowance) *
      ((double)DBRegAgent::ratelimit_per/(double)DBRegAgent::ratelimit_rate);
    DBG("not enough allowance (%f), sleeping %d useconds\n", allowance, sleep_time);
    usleep(sleep_time);
    allowance=0.0;
    gettimeofday(&last_check, 0);
  } else {
    allowance -= 1.0;
  }

  DBG("allowance left: %f\n", allowance);
}

void DBRegAgentProcessorThread::run() {
  DBG("DBRegAgentProcessorThread thread started\n");
  
  // register us as SIP event receiver for MOD_NAME_processor
  AmEventDispatcher::instance()->addEventQueue(MOD_NAME "_processor",this);

  mysqlpp::Connection::thread_start();

  // initialize ratelimit
  gettimeofday(&last_check, NULL);
  if (DBRegAgent::ratelimit_slowstart)
    allowance = 0.0;
  else
    allowance = DBRegAgent::ratelimit_rate;

  reg_agent = DBRegAgent::instance();
  while (!stopped) {
    waitForEvent();
    while (eventPending()) {
      rateLimitWait();
      processSingleEvent();
    }
  }

  mysqlpp::Connection::thread_end();

 DBG("DBRegAgentProcessorThread thread stopped\n"); 
}

void DBRegAgentProcessorThread::process(AmEvent* ev) {

  if (ev->event_id == E_SYSTEM) {
    AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(ev);
    if(sys_ev){	
      DBG("Session received system Event\n");
      if (sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
	DBG("stopping processor thread\n");
	stopped = true;
      }
      return;
    }
  }


  if (ev->event_id == RegistrationActionEventID) {
    RegistrationActionEvent* reg_action_ev =
      dynamic_cast<RegistrationActionEvent*>(ev);
    if (reg_action_ev) {
      reg_agent->onRegistrationActionEvent(reg_action_ev);
      return;
    }
  }

  ERROR("unknown event received!\n");
}
#if 0
void test_cb(RegTimer* tr, long data1, void* data2) {
  DBG("cb called: [%p], data %ld / [%p]\n", tr, data1, data2);
}

void DBRegAgent::run_tests() {

  registration_timer.start();

  struct timeval now;
  gettimeofday(&now, 0);

  RegTimer rt;
  rt.expires = now.tv_sec + 10; 
  rt.cb=test_cb;
  registration_scheduler.insert_timer(&rt);

  RegTimer rt2;
  rt2.expires = now.tv_sec + 5; 
  rt2.cb=test_cb;
  registration_scheduler.insert_timer(&rt2);

  RegTimer rt3;
  rt3.expires = now.tv_sec + 15; 
  rt3.cb=test_cb;
  registration_scheduler.insert_timer(&rt3);

  RegTimer rt4;
  rt4.expires = now.tv_sec - 1; 
  rt4.cb=test_cb;
  registration_scheduler.insert_timer(&rt4);

  RegTimer rt5;
  rt5.expires = now.tv_sec + 100000; 
  rt5.cb=test_cb;
  registration_scheduler.insert_timer(&rt5);

  RegTimer rt6;
  rt6.expires = now.tv_sec + 100; 
  rt6.cb=test_cb;
  registration_scheduler.insert_timer_leastloaded(&rt6, now.tv_sec+5, now.tv_sec+50);


  sleep(30);
  gettimeofday(&now, 0);

  RegTimer rt7;
  rt6.expires = now.tv_sec + 980; 
  rt6.cb=test_cb;
  registration_scheduler.insert_timer_leastloaded(&rt6, now.tv_sec+9980, now.tv_sec+9990);

   vector<RegTimer*> rts;

   for (int i=0;i<1000;i++) {
     RegTimer* t = new RegTimer();
     rts.push_back(t);
     t->expires = now.tv_sec + i;
     t->cb=test_cb;
     registration_scheduler.insert_timer_leastloaded(t, now.tv_sec, now.tv_sec+1000);
   }

  sleep(200);
}
#endif
