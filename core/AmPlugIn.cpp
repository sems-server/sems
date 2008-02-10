/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AmPlugIn.h"
#include "AmConfig.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmSdp.h"
#include "AmSipDispatcher.h"
#include "AmServer.h"

#include "amci/amci.h"
#include "amci/codecs.h"
#include "log.h"

#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

#include <set>
using std::set;

static unsigned int pcm16_bytes2samples(long h_codec, unsigned int num_bytes)
{
  return num_bytes / 2;
}

static unsigned int pcm16_samples2bytes(long h_codec, unsigned int num_samples)
{
  return num_samples * 2;
}

static unsigned int tevent_bytes2samples(long h_codec, unsigned int num_bytes)
{
  return num_bytes;
}

static unsigned int tevent_samples2bytes(long h_codec, unsigned int num_samples)
{
  return num_samples;
}

amci_codec_t _codec_pcm16 = { 
  CODEC_PCM16,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  pcm16_bytes2samples,
  pcm16_samples2bytes
};

amci_codec_t _codec_tevent = { 
  CODEC_TELEPHONE_EVENT,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  tevent_bytes2samples,
  tevent_samples2bytes
};

amci_payload_t _payload_tevent = { 
  -1,
  "telephone-event",
  8000,
  -1,
  CODEC_TELEPHONE_EVENT,
  -1 
};

AmPlugIn* AmPlugIn::_instance=0;

AmPlugIn::AmPlugIn()
  : dynamic_pl(DYNAMIC_PAYLOAD_TYPE_START),   
    ctrlIface(NULL)
{
}

AmPlugIn::~AmPlugIn()
{
  for(vector<void*>::iterator it=dlls.begin();it!=dlls.end();++it)
    dlclose(*it);
}

AmPlugIn* AmPlugIn::instance()
{
  if(!_instance)
    _instance = new AmPlugIn();

  return _instance;
}

void AmPlugIn::init() {
  vector<string> excluded_payloads_v = 
    explode(AmConfig::ExcludePayloads, ";");
  for (vector<string>::iterator it = 
	 excluded_payloads_v.begin(); 
       it != excluded_payloads_v.end();it++)
    excluded_payloads.insert(*it);

  DBG("adding built-in codecs...\n");
  addCodec(&_codec_pcm16);
  addCodec(&_codec_tevent);
  addPayload(&_payload_tevent);
}


int AmPlugIn::load(const string& directory, const string& plugins)
{
  int err=0;
  struct dirent* entry;
  DIR* dir = opendir(directory.c_str());

  if(!dir){
    ERROR("plug-ins loader (%s): %s\n",directory.c_str(),strerror(errno));
    return -1;
  }


  if (!plugins.length()) {
    DBG("AmPlugIn: loading modules in directory '%s':\n", directory.c_str());

    vector<string> excluded_plugins = explode(AmConfig::ExcludePlugins, ";");
    set<string> excluded_plugins_s; 
    for (vector<string>::iterator it = excluded_plugins.begin(); 
	 it != excluded_plugins.end();it++)
      excluded_plugins_s.insert(*it);

    while( ((entry = readdir(dir)) != NULL) && (err == 0) ){
      string plugin_name = string(entry->d_name);

      if(plugin_name.find(".so",plugin_name.length()-3) == string::npos ){
        continue;
      }
      
      if (excluded_plugins_s.find(plugin_name.substr(0, plugin_name.length()-3)) 
	  != excluded_plugins_s.end()) {
	DBG("skipping excluded plugin %s\n", plugin_name.c_str());
	continue;
      }
      
      string plugin_file = directory + "/" + plugin_name;

      DBG("loading %s ...\n",plugin_file.c_str());
      if( (err = loadPlugIn(plugin_file)) < 0 ) {
        ERROR("while loading plug-in '%s'\n",plugin_file.c_str());
        return -1;
      }
    }
  } else {
    DBG("AmPlugIn: loading modules '%s':\n", plugins.c_str());

    vector<string> plugins_list = explode(plugins, ";");
    for (vector<string>::iterator it = plugins_list.begin(); 
       it != plugins_list.end(); it++) {
      string plugin_file = *it;
      if(plugin_file.find(".so",plugin_file.length()-3) == string::npos )
        plugin_file+=".so";

      plugin_file = directory + "/"  + plugin_file;
      DBG("loading %s...\n",plugin_file.c_str());
      if( (err = loadPlugIn(plugin_file)) < 0 ) {
        ERROR("while loading plug-in '%s'\n",plugin_file.c_str());
        // be strict here: if plugin not loaded, stop!
        return err; 
      }
    }
  }
  closedir(dir);

  DBG("AmPlugIn: modules loaded.\n");

  DBG("AmPlugIn: Initializing plugins...\n");

  // initialize base components
  for(std::map<std::string,AmPluginFactory*>::iterator it = name2base.begin();
      it != name2base.end(); it++){
    err = it->second->onLoad();
    if(err)
      return err;
  }

  // initialize session event handlers
  for(std::map<std::string,AmSessionEventHandlerFactory*>::iterator it = name2seh.begin();
      it != name2seh.end(); it++){
    err = it->second->onLoad();
    if(err)
      return err;
  }

  // initialize DI component plugins
  for(std::map<std::string,AmDynInvokeFactory*>::iterator it = name2di.begin();
      it != name2di.end(); it++){
    err = it->second->onLoad();
    if(err)
      return err;
  }

  // load SIPEventHandlers 
  for(std::map<std::string,AmSIPEventHandler*>::iterator it = name2sipeh.begin();
      it != name2sipeh.end(); it++){
    err = it->second->onLoad();
    if(err)
      return err;
    // register for receiving replys 
    //AmReplyHandler::get()->registerReplyHandler(it->second);
    AmSipDispatcher::instance()->registerReplyHandler(it->second);
  }

  // init logging facilities
  for(std::map<std::string,AmLoggingFacility*>::iterator it = name2logfac.begin();
      it != name2logfac.end(); it++){
    err = it->second->onLoad();
    if(err)
      return err;
    // register for receiving logging messages
    register_logging_fac(it->second);
  }

    
  std::map<std::string,AmSessionFactory*> apps(name2app);
  for(std::map<std::string,AmSessionFactory*>::iterator it = apps.begin();
      it != apps.end(); it++){

    err = it->second->onLoad();
    if(err)
      return err;
  }

  if (ctrlIface) {
    if ((err = ctrlIface->onLoad())) {
      ERROR("failed to initialize control interface.\n");
      return err;
    } else {
	AmServer::instance()->regIface(ctrlIface->instance());
    }
  }

  DBG("AmPlugIn: Initialized plugins.\n");

  return 0;
}

int AmPlugIn::loadPlugIn(const string& file)
{
  void* h_dl = dlopen(file.c_str(),RTLD_NOW | RTLD_GLOBAL);

  if(!h_dl){
    ERROR("AmPlugIn::loadPlugIn: %s: %s\n",file.c_str(),dlerror());
    return -1;
  }

  FactoryCreate fc = NULL;
  amci_exports_t* exports = (amci_exports_t*)dlsym(h_dl,"amci_exports");

  bool has_sym=false;
  if(exports){
    if(loadAudioPlugIn(exports))
      goto error;
    goto end;
  }

  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_SESSION_EXPORT_STR)) != NULL){
    if(loadAppPlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_SESSION_EVENT_HANDLER_EXPORT_STR)) != NULL){
    if(loadSehPlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_PLUGIN_EXPORT_STR)) != NULL){
    if(loadBasePlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_PLUGIN_CLASS_EXPORT_STR)) != NULL){
    if(loadDiPlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }

  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_SIP_EVENT_HANDLER_EXPORT_STR)) != NULL){
    if(loadSIPehPlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }

  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_LOG_FACILITY_EXPORT_STR)) != NULL){
    if(loadLogFacPlugIn((AmPluginFactory*)fc()))
      goto error;
    has_sym=true;
  }

  // try load a control plugin
  if ((fc = (FactoryCreate)dlsym(h_dl,FACTORY_CONTROL_INTERFACE_EXPORT_STR))) {
    if (loadCtrlFacPlugIn((AmPluginFactory*)fc()))
      goto error;
    assert(! has_sym);
    has_sym = true;
  }

  if(!has_sym){
    ERROR("Plugin type could not be detected (%s)(%s)\n",file.c_str(),dlerror());
    goto error;
  }

 end:
  dlls.push_back(h_dl);
  return 0;

 error:
  dlclose(h_dl);
  return -1;
}


amci_inoutfmt_t* AmPlugIn::fileFormat(const string& fmt_name, const string& ext)
{
  if(!fmt_name.empty()){

    std::map<std::string,amci_inoutfmt_t*>::iterator it = file_formats.find(fmt_name);
    if ((it != file_formats.end()) &&
	(ext.empty() || (ext == it->second->ext)))
      return it->second;
  }
  else if(!ext.empty()){
	
    std::map<std::string,amci_inoutfmt_t*>::iterator it = file_formats.begin();
    for(;it != file_formats.end();++it){
      if(ext == it->second->ext)
	return it->second;
    }
  }

  return 0;
}

amci_codec_t* AmPlugIn::codec(int id)
{
  std::map<int,amci_codec_t*>::iterator it = codecs.find(id);
  if(it != codecs.end())
    return it->second;

  return 0;
}

amci_payload_t*  AmPlugIn::payload(int payload_id)
{
  std::map<int,amci_payload_t*>::iterator it = payloads.find(payload_id);
  if(it != payloads.end())
    return it->second;

  return 0;
}

amci_subtype_t* AmPlugIn::subtype(amci_inoutfmt_t* iofmt, int subtype)
{
  if(!iofmt)
    return 0;
    
  amci_subtype_t* st = iofmt->subtypes;
  if(subtype<0) // default subtype wanted
    return st;

  for(;;st++){
    if(!st || st->type<0) break;
    if(st->type == subtype)
      return st;
  }

  return 0;
}

AmSessionFactory* AmPlugIn::getFactory4App(const string& app_name)
{
  std::map<std::string,AmSessionFactory*>::iterator it = name2app.find(app_name);
  if(it != name2app.end())
    return it->second;
  return 0;
}

AmSessionEventHandlerFactory* AmPlugIn::getFactory4Seh(const string& name)
{
  std::map<std::string,AmSessionEventHandlerFactory*>::iterator it = name2seh.find(name);
  if(it != name2seh.end())
    return it->second;
  return 0;
}

AmDynInvokeFactory* AmPlugIn::getFactory4Di(const string& name)
{
  std::map<std::string,AmDynInvokeFactory*>::iterator it = name2di.find(name);
  if(it != name2di.end())
    return it->second;
  return 0;
}

AmSIPEventHandler* AmPlugIn::getFactory4SIPeh(const string& name)
{
  std::map<std::string,AmSIPEventHandler*>::iterator it = name2sipeh.find(name);
  if(it != name2sipeh.end())
    return it->second;
  return 0;
}

AmLoggingFacility* AmPlugIn::getFactory4LogFaclty(const string& name)
{
  std::map<std::string,AmLoggingFacility*>::iterator it = name2logfac.find(name);
  if(it != name2logfac.end())
    return it->second;
  return 0;
}

int AmPlugIn::loadAudioPlugIn(amci_exports_t* exports)
{
  if(!exports){
    ERROR("audio plug-in doesn't contain any exports !\n");
    return -1;
  }

  if (exports->module_load) {
    if (exports->module_load() < 0) {
      ERROR("initializing audio plug-in!\n");
      return -1;
    }
  }

  for( amci_codec_t* c=exports->codecs; 
       c->id>=0; c++ ){

    if(addCodec(c))
      goto error;
  }

  for( amci_payload_t* p=exports->payloads; 
       p->name; p++ ){

    if(addPayload(p))
      goto error;
  }

  for(amci_inoutfmt_t* f = exports->file_formats; 
      f->name; f++ ){

    if(addFileFormat(f))
      goto error;
  }
    
  return 0;

 error:
  return -1;
}


int AmPlugIn::loadAppPlugIn(AmPluginFactory* f)
{
  AmSessionFactory* sf = dynamic_cast<AmSessionFactory*>(f);
  if(!sf){
    ERROR("invalid application plug-in!\n");
    goto error;
  }

  if(name2app.find(sf->getName()) != name2app.end()){
    ERROR("application '%s' already loaded !\n",sf->getName().c_str());
    goto error;
  }
      
  name2app.insert(std::make_pair(sf->getName(),sf));
  DBG("application '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadSehPlugIn(AmPluginFactory* f)
{
  AmSessionEventHandlerFactory* sf = dynamic_cast<AmSessionEventHandlerFactory*>(f);
  if(!sf){
    ERROR("invalid session component plug-in!\n");
    goto error;
  }

  if(name2seh.find(sf->getName()) != name2seh.end()){
    ERROR("session component '%s' already loaded !\n",sf->getName().c_str());
    goto error;
  }
      
  name2seh.insert(std::make_pair(sf->getName(),sf));
  DBG("session component '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadBasePlugIn(AmPluginFactory* f)
{
  name2base.insert(std::make_pair(f->getName(),f));
  return 0;
}

int AmPlugIn::loadDiPlugIn(AmPluginFactory* f)
{
  AmDynInvokeFactory* sf = dynamic_cast<AmDynInvokeFactory*>(f);
  if(!sf){
    ERROR("invalid component plug-in!\n");
    goto error;
  }

  if(name2di.find(sf->getName()) != name2di.end()){
    ERROR("component '%s' already loaded !\n",sf->getName().c_str());
    goto error;
  }
      
  name2di.insert(std::make_pair(sf->getName(),sf));
  DBG("component '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadSIPehPlugIn(AmPluginFactory* f)
{
  AmSIPEventHandler* sf = dynamic_cast<AmSIPEventHandler*>(f);
  if(!sf){
    ERROR("invalid SIP event handler plug-in!\n");
    goto error;
  }

  if(name2sipeh.find(sf->getName()) != name2sipeh.end()){
    ERROR("sip event handler component '%s' already loaded !\n",
	  sf->getName().c_str());
    goto error;
  }
      
  name2sipeh.insert(std::make_pair(sf->getName(),sf));
  DBG("sip event handler component '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadLogFacPlugIn(AmPluginFactory* f)
{
  AmLoggingFacility* sf = dynamic_cast<AmLoggingFacility*>(f);
  if(!sf){
    ERROR("invalid logging facility plug-in!\n");
    goto error;
  }

  if(name2logfac.find(sf->getName()) != name2logfac.end()){
    ERROR("logging facility '%s' already loaded !\n",
	  sf->getName().c_str());
    goto error;
  }
      
  name2logfac.insert(std::make_pair(sf->getName(),sf));
  DBG("logging facility component '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadCtrlFacPlugIn(AmPluginFactory* f)
{
  AmCtrlInterfaceFactory *_ctrlIface = dynamic_cast<AmCtrlInterfaceFactory *>(f);
  if (! _ctrlIface) {
    ERROR("invalid control interface plugin.\n");
    return -1;
  }
  if (ctrlIface) {
    ERROR("one control interface already loaded (`%s'): can not load a "
      "second one (`%s').\n", (ctrlIface->getName()).c_str(), 
      (_ctrlIface->getName()).c_str());
    return -1;
  }
  ctrlIface = _ctrlIface;//->instance();
  if (! ctrlIface) {
    ERROR("BUG: failed to retrieve a control interface instance.\n");
    return -1;
  }

  return 0;
}

int AmPlugIn::addCodec(amci_codec_t* c)
{
  if(codecs.find(c->id) != codecs.end()){
    ERROR("codec id (%i) already supported\n",c->id);
    return -1;
  }
  codecs.insert(std::make_pair(c->id,c));
  DBG("codec id %i inserted\n",c->id);
  return 0;
}

int AmPlugIn::addPayload(amci_payload_t* p)
{
  if (excluded_payloads.find(p->name) != 
      excluded_payloads.end()) {
    DBG("Not enabling excluded payload '%s'\n", 
	p->name);
    return 0;
  }

  amci_codec_t* c;
  unsigned int i, id;
  if( !(c = codec(p->codec_id)) ){
    ERROR("in payload '%s': codec id (%i) not supported\n",
	  p->name, p->codec_id);
    return -1;
  }
  if(p->payload_id != -1){
    if(payloads.find(p->payload_id) != payloads.end()){
      ERROR("payload id (%i) already supported\n",p->payload_id);
      return -1;
    }
    payloads.insert(std::make_pair(p->payload_id,p));
    id = p->payload_id;
  }
  else {
    payloads.insert(std::make_pair(dynamic_pl,p));
    id = dynamic_pl;
    dynamic_pl++;
  }

  for (i = 0; i < AmConfig::CodecOrder.size(); i++) {
      if (p->name == AmConfig::CodecOrder[i]) break;
  }
  if (i >= AmConfig::CodecOrder.size()) {
      payload_order.insert(std::make_pair(id + 100, id));
      DBG("payload '%s' inserted with id %i and order %i\n",
	  p->name, id, id + 100);
  } else {
      payload_order.insert(std::make_pair(i, id));
      DBG("payload '%s' inserted with id %i and order %i\n",
	  p->name, id, i);
  }

  return 0;
}

int AmPlugIn::addFileFormat(amci_inoutfmt_t* f)
{
  if(file_formats.find(f->name) != file_formats.end()){
    ERROR("file format '%s' already supported\n",f->name);
    return -1;
  }

  amci_subtype_t* st = f->subtypes;
  for(; st->type >= 0; st++ ){

    if( !codec(st->codec_id) ){
      ERROR("in '%s' subtype %i: codec id (%i) not supported\n",
	    f->name,st->type,st->codec_id);
      return -1;
    }

    if (st->sample_rate < 0) {
      ERROR("in '%s' subtype %i: rate must be specified!"
	    " (ubr no longer supported)\n", f->name,st->type);
      return -1;
    }
    if (st->channels < 0) {
      ERROR("in '%s' subtype %i: channels must be specified!"
	    "(unspecified channel count no longer supported)\n", f->name,st->type);
      return -1;
    }

  }
  DBG("file format %s inserted\n",f->name);
  file_formats.insert(std::make_pair(f->name,f));

  return 0;
}

bool AmPlugIn::registerFactory4App(const string& app_name, AmSessionFactory* f)
{
  std::map<std::string,AmSessionFactory*>::iterator it = name2app.find(app_name);
  if(it != name2app.end()){
    WARN("Application '%s' has already been registered and cannot be registered a second time\n",
	 app_name.c_str());
    return false;
  }
  
  name2app.insert(make_pair(app_name,f));
  return true;
}
