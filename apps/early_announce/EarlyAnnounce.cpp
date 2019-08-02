/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2008 Juha Heinanen (USE_MYSQL parts)
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

#include "EarlyAnnounce.h"
#include "AmConfig.h"
#include "AmUtils.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "early_announce"

#ifdef USE_MYSQL
#define DEFAULT_AUDIO_TABLE "default_audio"
#define DOMAIN_AUDIO_TABLE "domain_audio"
#define USER_AUDIO_TABLE "user_audio"
#endif

EXPORT_SESSION_FACTORY(EarlyAnnounceFactory,MOD_NAME);

#ifdef USE_MYSQL
string EarlyAnnounceFactory::AnnounceApplication;
string EarlyAnnounceFactory::AnnounceMessage;
string EarlyAnnounceFactory::DefaultLanguage;
#else
string EarlyAnnounceFactory::AnnouncePath;
string EarlyAnnounceFactory::AnnounceFile;
#endif

EarlyAnnounceFactory::ContB2B EarlyAnnounceFactory::ContinueB2B = 
  EarlyAnnounceFactory::Never;

EarlyAnnounceFactory::EarlyAnnounceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

#ifdef USE_MYSQL

static sql::Driver *driver;
static sql::Connection *connection;

int get_announce_msg(string application, string message, string user,
		     string domain, string language, string *audio_file)
{
    string query_string;

    if (!user.empty()) {
	*audio_file = string("/tmp/") +  application + "_" + 
	    message + "_" + domain + "_" + user + ".wav";
	query_string = "select audio from " + string(USER_AUDIO_TABLE) +
	    " where application='" + application + "' and message='" +
	    message + "' and userid='" + user + "' and domain='" +
	    domain + "'";
    } else if (!domain.empty()) {
	*audio_file = string("/tmp/") +  application + "_" +
	    message + "_" + domain + "_" + language + ".wav";
	query_string = "select audio from " + string(DOMAIN_AUDIO_TABLE) +
	    " where application='" + application + "' and message='" +
	    message + "' and domain='" + domain + "' and language='" +
	    language + "'";
    } else {
	*audio_file = string("/tmp/") +  application  + "_" +
	    message + "_" + language + ".wav";
	query_string = "select audio from " + string(DEFAULT_AUDIO_TABLE) +
	    " where application='" + application + "' and message='" +
	    message + "' and language='" + language + "'";
    }

    try {
	    
      DBG("Query string <%s>\n", query_string.c_str());

      sql::Statement *stmt;
      sql::ResultSet *result;

      stmt = connection->createStatement();
      result = stmt->executeQuery(query_string);
      if (result->next()) {
	FILE *file;
	file = fopen((*audio_file).c_str(), "wb");
	string s = result->getString("audio");
	fwrite(s.data(), 1, s.length(), file);
	fclose(file);
	return 1;
      } else {
	*audio_file = "";
	return 1;
      }

      delete stmt;
      delete result;

    }

    catch (sql::SQLException &er) {
	ERROR("MySQL query error: %s\n", er.what());
	*audio_file = "";
	return 0;
    }
}

#endif

int EarlyAnnounceFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  if (cfg.hasParameter("continue_b2b")) { 
    if (cfg.getParameter("continue_b2b") == "yes") {
      ContinueB2B = Always;
      DBG("early_announce in b2bua mode.\n");
    }
    else if (cfg.getParameter("continue_b2b") == "app-param") {
      ContinueB2B = AppParam;
      DBG("early_announce in b2bua/final reply mode "
	  "(depends on app-param).\n");
    } else {
      DBG("early_announce sends final reply.\n");
    }
  }

#ifdef USE_MYSQL

  /* Get default audio from MySQL */

  string mysql_server, mysql_user, mysql_passwd, mysql_db, mysql_ca_cert;
  bool reconnect_state = true;
  sql::ConnectOptionsMap connection_properties;
  
  mysql_server = cfg.getParameter("mysql_server");
  if (mysql_server.empty()) {
    mysql_server = "localhost";
  }

  mysql_user = cfg.getParameter("mysql_user");
  if (mysql_user.empty()) {
    ERROR("early_announce.conf parameter 'mysql_user' is missing.\n");
    return -1;
  }

  mysql_passwd = cfg.getParameter("mysql_passwd");
  if (mysql_passwd.empty()) {
    ERROR("early_announce.conf parameter 'mysql_passwd' is missing.\n");
    return -1;
  }

  mysql_db = cfg.getParameter("mysql_db");
  if (mysql_db.empty()) {
    mysql_db = "sems";
  }

  mysql_ca_cert = cfg.getParameter("mysql_ca_cert");

  AnnounceApplication = cfg.getParameter("application");
  if (AnnounceApplication.empty()) {
    AnnounceApplication = MOD_NAME;
  }

  AnnounceMessage = cfg.getParameter("message");
  if (AnnounceMessage.empty()) {
    AnnounceMessage = "greeting_msg";
  }

  DefaultLanguage = cfg.getParameter("default_language");
  if (DefaultLanguage.empty()) {
    DefaultLanguage = "en";
  }

  try {

    connection_properties["hostName"] =
      sql::ConnectPropertyVal(mysql_server);
    connection_properties["userName"] =
      sql::ConnectPropertyVal(mysql_user);
    connection_properties["password"] =
      sql::ConnectPropertyVal(mysql_passwd);

    if (!mysql_ca_cert.empty()) {
      connection_properties["sslCa"] =
	sql::ConnectPropertyVal(sql::SQLString(mysql_ca_cert));
      connection_properties["sslCAPath"] =
	sql::ConnectPropertyVal(sql::SQLString(""));
      connection_properties["sslCipher"] =
	sql::ConnectPropertyVal(sql::SQLString("DHE-RSA-AES256-SHA"));
      connection_properties["sslEnforce"] =
	sql::ConnectPropertyVal(true);
    }

    driver = get_driver_instance();
    connection = driver->connect(connection_properties);
    connection->setClientOption("OPT_RECONNECT", &reconnect_state);
    connection->setSchema(mysql_db);

  }

  catch (sql::SQLException &er) {
    ERROR("MySQL connection error: %s\n", er.what());
    return -1;
  }

  string announce_file;
  if (!get_announce_msg(AnnounceApplication, AnnounceMessage, "", "",
			DefaultLanguage, &announce_file)) {
    return -1;
  }
  if (announce_file.empty()) {
    ERROR("default announce for " MOD_NAME " module does not exist.\n");
    return -1;
  }

#else 

  /* Get default audio from file system */

  AnnouncePath = cfg.getParameter("announce_path",ANNOUNCE_PATH);
  if( !AnnouncePath.empty() 
      && AnnouncePath[AnnouncePath.length()-1] != '/' )
    AnnouncePath += "/";

  AnnounceFile = cfg.getParameter("default_announce",ANNOUNCE_FILE);

  string announce_file = AnnouncePath + AnnounceFile;
  if(!file_exists(announce_file)){
    ERROR("default file for " MOD_NAME " module does not exist ('%s').\n",
	  announce_file.c_str());
    return -1;
  }

#endif

  return 0;
}


void EarlyAnnounceDialog::onInvite(const AmSipRequest& req) 
{
  AmMimeBody sdp_body;
  sdp_body.addPart(SIP_APPLICATION_SDP);

  if(dlg->reply(req,183,"Session Progress",
	       &sdp_body) != 0){
    throw AmSession::Exception(500,"could not reply");
  } else {
    invite_req = req;
  }
}


AmSession* EarlyAnnounceFactory::onInvite(const AmSipRequest& req, const string& app_name,
					  const map<string,string>& app_params)
{

#ifdef USE_MYSQL

    string iptel_app_param = getHeader(req.hdrs, PARAM_HDR, true);
    string language = get_header_keyvalue(iptel_app_param,"Language");
    string announce_file = "";

    if (language.empty()) language = DefaultLanguage;

    get_announce_msg(AnnounceApplication, AnnounceMessage, req.user,
		     req.domain, "", &announce_file);
    if (!announce_file.empty()) goto end;
    get_announce_msg(AnnounceApplication, AnnounceMessage, "", req.domain,
		     language, &announce_file);
    if (!announce_file.empty()) goto end;
    get_announce_msg(AnnounceApplication, AnnounceMessage, "", "", language,
		     &announce_file);

#else

  string announce_path = AnnouncePath;
  string announce_file = announce_path + req.domain 
    + "/" + req.user + ".wav";

  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    goto end;

  announce_file = announce_path + req.user + ".wav";
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    goto end;

  announce_file = AnnouncePath + AnnounceFile;

#endif

 end:
  return new EarlyAnnounceDialog(announce_file);
}

EarlyAnnounceDialog::EarlyAnnounceDialog(const string& filename)
  : filename(filename)
{
  set_sip_relay_only(false);
}

EarlyAnnounceDialog::~EarlyAnnounceDialog()
{
}

void EarlyAnnounceDialog::onEarlySessionStart()
{
  // we can drop all received packets
  // this disables DTMF detection as well
  setReceiving(false);

  DBG("EarlyAnnounceDialog::onEarlySessionStart\n");

  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("EarlyAnnounceDialog::onEarlySessionStart: Cannot open file");
    
  setOutput(&wav_file);

  AmB2BCallerSession::onEarlySessionStart();
}

void EarlyAnnounceDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  setStopped();
}

void EarlyAnnounceDialog::onCancel(const AmSipRequest& req)
{
  dlg->reply(invite_req,487,"Call terminated");
  AmB2BCallerSession::terminateOtherLeg();
  setStopped();
}

void EarlyAnnounceDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && 
     (audio_event->event_id == AmAudioEvent::cleared)) {
      DBG("AmAudioEvent::cleared\n");

      bool continue_b2b = false;
      if (EarlyAnnounceFactory::ContinueB2B == 
	  EarlyAnnounceFactory::Always) {
	continue_b2b = true;
      } else if (EarlyAnnounceFactory::ContinueB2B == 
		 EarlyAnnounceFactory::AppParam) {
	string iptel_app_param = getHeader(invite_req.hdrs, PARAM_HDR, true);
	if (iptel_app_param.length()) {
	  continue_b2b = get_header_keyvalue(iptel_app_param,"B2B")=="yes";
	} else {
	  continue_b2b = getHeader(invite_req.hdrs,"P-B2B", true)=="yes";
	}
      }
      DBG("determined: continue_b2b = %s\n", continue_b2b?"true":"false");

      if (!continue_b2b) {
	unsigned int code_i = 404;
	string reason = "Not Found";
	
	string iptel_app_param = getHeader(invite_req.hdrs, PARAM_HDR, true);
	if (iptel_app_param.length()) {
	  string code = get_header_keyvalue(iptel_app_param,"Final-Reply-Code");
	  if (code.length() && str2i(code, code_i)) {
	    ERROR("while parsing Final-Reply-Code parameter\n");
	  }
	  reason = get_header_keyvalue(iptel_app_param,"Final-Reply-Reason");
	  if (!reason.length())
	    reason = "Not Found";
	} else {
	  string code = getHeader(invite_req.hdrs,"P-Final-Reply-Code", true);
	  if (code.length() && str2i(code, code_i)) {
	    ERROR("while parsing P-Final-Reply-Code\n");
	  }
	  string h_reason =  getHeader(invite_req.hdrs,"P-Final-Reply-Reason", true);
	  if (h_reason.length()) {
	    INFO("Use of P-Final-Reply-Code/P-Final-Reply-Reason is deprecated. ");
	    INFO("Use '%s: Final-Reply-Code=<code>;"
		 "Final-Reply-Reason=<rs>' instead.\n",PARAM_HDR);
	    reason = h_reason;
	  }
	}

	DBG("Replying with code %d %s\n", code_i, reason.c_str());
	dlg->reply(invite_req, code_i, reason);
	
	setStopped();
      } else {
	set_sip_relay_only(true);
	recvd_req.insert(std::make_pair(invite_req.cseq,invite_req));
	
	relayEvent(new B2BSipRequestEvent(invite_req,true));
      }
	
      return;
    }

  AmB2BCallerSession::process(event);
}
