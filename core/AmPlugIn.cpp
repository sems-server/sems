/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#include "AmPlugIn.h"
#include "AmConfig.h"
#include "AmApi.h"
#include "AmUtils.h"
//#include "AmSdp.h"
#include "AmSipDispatcher.h"
//#include "AmServer.h"

#include "amci/amci.h"
#include "amci/codecs.h"
#include "log.h"

#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <set>
#include <vector>
#include <algorithm>
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
  8000, // telephone-event has always SR 8000 
  8000,
  -1,
  CODEC_TELEPHONE_EVENT,
  -1 
};

AmPlugIn* AmPlugIn::_instance=0;

AmPlugIn::AmPlugIn()
  : dynamic_pl(DYNAMIC_PAYLOAD_TYPE_START)
    //ctrlIface(NULL)
{
}


static void delete_plugin_factory(std::pair<string, AmPluginFactory*> pf)
{
  DBG("decreasing reference to plug-in factory: %s\n", pf.first.c_str());
  dec_ref(pf.second);

}

AmPlugIn::~AmPlugIn()
{
  std::for_each(module_objects.begin(), module_objects.end(), delete_plugin_factory);
  std::for_each(name2seh.begin(), name2seh.end(), delete_plugin_factory);
  std::for_each(name2base.begin(), name2base.end(), delete_plugin_factory);
  std::for_each(name2di.begin(), name2di.end(), delete_plugin_factory);
  std::for_each(name2logfac.begin(), name2logfac.end(), delete_plugin_factory);

  // if _DEBUG is set do not unload shared libs to allow better debugging
#ifndef _DEBUG
  for(vector<void*>::iterator it=dlls.begin();it!=dlls.end();++it)
    dlclose(*it);
#endif
}

void AmPlugIn::dispose()
{
  if (_instance) {
    delete _instance;
  }
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
  
  vector<AmPluginFactory*> loaded_plugins;

  if (!plugins.length()) {
    INFO("AmPlugIn: loading modules in directory '%s':\n", directory.c_str());

    DIR* dir = opendir(directory.c_str());
    if (!dir){
      ERROR("while opening plug-in directory (%s): %s\n",
	    directory.c_str(), strerror(errno));
      return -1;
    }
    
    vector<string> excluded_plugins = explode(AmConfig::ExcludePlugins, ";");
    set<string> excluded_plugins_s; 
    for (vector<string>::iterator it = excluded_plugins.begin(); 
	 it != excluded_plugins.end();it++)
      excluded_plugins_s.insert(*it);

    struct dirent* entry;

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
      if( (err = loadPlugIn(plugin_file, plugin_name, loaded_plugins)) < 0 ) {
        ERROR("while loading plug-in '%s'\n",plugin_file.c_str());
        return -1;
      }
    }
    
    closedir(dir);
  } 
  else {
    INFO("AmPlugIn: loading modules: '%s'\n", plugins.c_str());

    vector<string> plugins_list = explode(plugins, ";");
    for (vector<string>::iterator it = plugins_list.begin(); 
       it != plugins_list.end(); it++) {
      string plugin_file = *it;
      if (plugin_file == "sipctrl") {
	WARN("sipctrl is integrated into the core, loading sipctrl "
	     "module is not necessary any more\n");
	WARN("please update your configuration to not load sipctrl module\n");
	continue;
      }

      if(plugin_file.find(".so",plugin_file.length()-3) == string::npos )
        plugin_file+=".so";

      plugin_file = directory + "/"  + plugin_file;
      DBG("loading %s...\n",plugin_file.c_str());
      if( (err = loadPlugIn(plugin_file, plugin_file, loaded_plugins)) < 0 ) {
        ERROR("while loading plug-in '%s'\n",plugin_file.c_str());
        // be strict here: if plugin not loaded, stop!
        return err; 
      }
    }
  }

  DBG("AmPlugIn: modules loaded.\n");
  DBG("Initializing %zd plugins...\n", loaded_plugins.size());
  for (vector<AmPluginFactory*>::iterator it =
	 loaded_plugins.begin(); it != loaded_plugins.end(); it++) {
    int err = (*it)->onLoad();
    if(err)
      return err;
  }

  return 0;
}

void AmPlugIn::registerLoggingPlugins() {
  // init logging facilities
  for(std::map<std::string,AmLoggingFacility*>::iterator it = name2logfac.begin();
      it != name2logfac.end(); it++){
    // register for receiving logging messages
    register_log_hook(it->second);
  }  
}

void AmPlugIn::set_load_rtld_global(const string& plugin_name) {
  rtld_global_plugins.insert(plugin_name);
}

int AmPlugIn::loadPlugIn(const string& file, const string& plugin_name,
			 vector<AmPluginFactory*>& plugins)
{
  AmPluginFactory* plugin = NULL; // default: not loaded
  int dlopen_flags = RTLD_NOW;

  char* pname = strdup(plugin_name.c_str());
  char* bname = basename(pname);

  // dsm, ivr and py_sems need RTLD_GLOBAL
  if (!strcmp(bname, "dsm.so") || !strcmp(bname, "ivr.so") ||
      !strcmp(bname, "py_sems.so") || !strcmp(bname, "sbc.so") ||
      !strcmp(bname, "diameter_client.so") || !strcmp(bname, "registrar_client.so") ||
      !strcmp(bname, "uac_auth.so") || !strcmp(bname, "msg_storage.so")
      ) {
      dlopen_flags = RTLD_NOW | RTLD_GLOBAL;
      DBG("using RTLD_NOW | RTLD_GLOBAL to dlopen '%s'\n", file.c_str());
  }

  // possibly others
  for (std::set<string>::iterator it=rtld_global_plugins.begin();
       it!=rtld_global_plugins.end();it++) {
    if (!strcmp(bname, it->c_str())) {
      dlopen_flags = RTLD_NOW | RTLD_GLOBAL;
      DBG("using RTLD_NOW | RTLD_GLOBAL to dlopen '%s'\n", file.c_str());
      break;
    }
  }
  free(pname);

  void* h_dl = dlopen(file.c_str(),dlopen_flags);

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
    plugin = (AmPluginFactory*)fc();
    if(loadAppPlugIn(plugin))
      goto error;
    has_sym=true;
    if (NULL != plugin) plugins.push_back(plugin);
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_SESSION_EVENT_HANDLER_EXPORT_STR)) != NULL){
    plugin = (AmPluginFactory*)fc();
    if(loadSehPlugIn(plugin))
      goto error;
    has_sym=true;
    if (NULL != plugin) plugins.push_back(plugin);
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_PLUGIN_EXPORT_STR)) != NULL){
    plugin = (AmPluginFactory*)fc();
    if(loadBasePlugIn(plugin))
      goto error;
    has_sym=true;
    if (NULL != plugin) plugins.push_back(plugin);
  }
  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_PLUGIN_CLASS_EXPORT_STR)) != NULL){
    plugin = (AmPluginFactory*)fc();
    if(loadDiPlugIn(plugin))
      goto error;
    has_sym=true;
    if (NULL != plugin) plugins.push_back(plugin);
  }

  if((fc = (FactoryCreate)dlsym(h_dl,FACTORY_LOG_FACILITY_EXPORT_STR)) != NULL){
    plugin = (AmPluginFactory*)fc();
    if(loadLogFacPlugIn(plugin))
      goto error;
    has_sym=true;
    if (NULL != plugin) plugins.push_back(plugin);
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
  std::map<int,amci_codec_t*>::const_iterator it = codecs.find(id);
  if(it != codecs.end())
    return it->second;

  return 0;
}

amci_payload_t*  AmPlugIn::payload(int payload_id) const
{
  std::map<int,amci_payload_t*>::const_iterator it = payloads.find(payload_id);
  if(it != payloads.end())
    return it->second;

  return 0;
}

int AmPlugIn::getDynPayload(const string& name, int rate, int encoding_param) const {
  // find a dynamic payload by name/rate and encoding_param (channels, if > 0)
  for(std::map<int, amci_payload_t*>::const_iterator pl_it = payloads.begin();
      pl_it != payloads.end(); ++pl_it)
    if( (!strcasecmp(name.c_str(),pl_it->second->name)
	 && (rate == pl_it->second->advertised_sample_rate)) ) {
      if ((encoding_param > 0) && (pl_it->second->channels > 0) && 
	  (encoding_param != pl_it->second->channels))
	continue;
	  
      return pl_it->first;
    }
  // not found
  return -1;
}

/** return 0, or -1 in case of error. */
void AmPlugIn::getPayloads(vector<SdpPayload>& pl_vec) const
{
  for (std::map<int,int>::const_iterator it = payload_order.begin(); it != payload_order.end(); ++it) {
    std::map<int,amci_payload_t*>::const_iterator pl_it = payloads.find(it->second);
    if(pl_it != payloads.end()){
      // if channels==2 use that value; otherwise don't add channels param
      pl_vec.push_back(SdpPayload(pl_it->first, pl_it->second->name, pl_it->second->advertised_sample_rate, pl_it->second->channels==2?2:0));
    } else {
      ERROR("Payload %d (from the payload_order map) was not found in payloads map!\n", it->second);
    }
  }
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

int AmPlugIn::subtypeID(amci_inoutfmt_t* iofmt, const string& subtype_name) {
  if(!iofmt)
    return -1;

  amci_subtype_t* st = iofmt->subtypes;
  if(subtype_name.empty()) // default subtype wanted
    return st->type;

  for(;;st++){
    if(!st || st->type<0) break;
    if(st->name == subtype_name)
      return st->type;
  }
  return -1;
}

AmSessionFactory* AmPlugIn::getFactory4App(const string& app_name)
{
  AmSessionFactory* res = NULL;

  name2app_mut.lock();
  std::map<std::string,AmSessionFactory*>::iterator it = name2app.find(app_name);
  if(it != name2app.end()) 
    res = it->second;
  name2app_mut.unlock();

  return res;
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
    return -1;
  }

  name2app_mut.lock();

  if(name2app.find(sf->getName()) != name2app.end()){
    ERROR("application '%s' already loaded !\n",sf->getName().c_str());
    name2app_mut.unlock();
    return -1;
  }      

  name2app.insert(std::make_pair(sf->getName(),sf));
  DBG("application '%s' loaded.\n",sf->getName().c_str());

  inc_ref(sf);
  if(!module_objects.insert(std::make_pair(sf->getName(),sf)).second){
    // insertion failed
    dec_ref(sf);
  }
  name2app_mut.unlock();

  return 0;

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

  inc_ref(sf);
  name2seh.insert(std::make_pair(sf->getName(),sf));
  DBG("session component '%s' loaded.\n",sf->getName().c_str());

  return 0;

 error:
  return -1;
}

int AmPlugIn::loadBasePlugIn(AmPluginFactory* f)
{
  inc_ref(f);
  if(!name2base.insert(std::make_pair(f->getName(),f)).second){
    // insertion failed
    dec_ref(f);
  }
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
  }
  else {
    p->payload_id = dynamic_pl;
    dynamic_pl++;
  }

  payloads.insert(std::make_pair(p->payload_id,p));
  id = p->payload_id;

  for (i = 0; i < AmConfig::CodecOrder.size(); i++) {
      if (p->name == AmConfig::CodecOrder[i]) break;
  }
  if (i >= AmConfig::CodecOrder.size()) {
      payload_order.insert(std::make_pair(id + 100, id));
      DBG("payload '%s/%i' inserted with id %i and order %i\n",
	  p->name, p->sample_rate, id, id + 100);
  } else {
      payload_order.insert(std::make_pair(i, id));
      DBG("payload '%s/%i' inserted with id %i and order %i\n",
	  p->name, p->sample_rate, id, i);
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
  bool res;

  name2app_mut.lock();
  std::map<std::string,AmSessionFactory*>::iterator it = name2app.find(app_name);
  if(it != name2app.end()){
    WARN("Application '%s' has already been registered and cannot be "
	 "registered a second time\n",
	 app_name.c_str());
    res =  false;
  } else {
    name2app.insert(make_pair(app_name,f));
    res = true;
  }
  name2app_mut.unlock();

  return res;
}

// static alias to registerFactory4App
bool AmPlugIn::registerApplication(const string& app_name, AmSessionFactory* f) {
  bool res = instance()->registerFactory4App(app_name, f);
  if (res) {
    DBG("Application '%s' registered.\n", app_name.c_str());
  }
  return res;
}

AmSessionFactory* AmPlugIn::findSessionFactory(const AmSipRequest& req, string& app_name)
{
    string m_app_name;

    switch (AmConfig::AppSelect) {
	
    case AmConfig::App_RURIUSER:
      m_app_name = req.user; 
      break;
    case AmConfig::App_APPHDR: 
      m_app_name = getHeader(req.hdrs, APPNAME_HDR, true); 
      break;      
    case AmConfig::App_RURIPARAM: 
      m_app_name = get_header_param(req.r_uri, "app");
      break;
    case AmConfig::App_MAPPING:
      m_app_name = ""; // no match if not found
      run_regex_mapping(AmConfig::AppMapping, req.r_uri.c_str(), m_app_name);
      break;
    case AmConfig::App_SPECIFIED: 
      m_app_name = AmConfig::Application; 
      break;
    }
    
    if (m_app_name.empty()) {
      INFO("could not find any application matching configured criteria\n");
      return NULL;
    }
    
    AmSessionFactory* session_factory = getFactory4App(m_app_name);
    if(!session_factory) {
      ERROR("AmPlugIn::findSessionFactory: application '%s' not found !\n", m_app_name.c_str());
    }
    
    app_name = m_app_name;
    return session_factory;
}

#define REGISTER_STUFF(comp_name, map_name, param_name)			\
  if(instance()->map_name.find(param_name) != instance()->map_name.end()){	\
  ERROR(comp_name "'%s' already registered !\n", param_name.c_str());	\
  return false;								\
  }									\
  inc_ref(f);								\
  instance()->map_name.insert(std::make_pair(param_name,f));		\
  DBG(comp_name " '%s' registered.\n",param_name.c_str());		\
  return true;

bool AmPlugIn::registerSIPEventHandler(const string& seh_name,
				       AmSessionEventHandlerFactory* f) {
  REGISTER_STUFF("SIP Event handler", name2seh, seh_name);
}

bool AmPlugIn::registerDIInterface(const string& di_name, AmDynInvokeFactory* f) {
  REGISTER_STUFF("DI Interface", name2di, di_name);
}

bool AmPlugIn::registerLoggingFacility(const string& lf_name, AmLoggingFacility* f) {
  REGISTER_STUFF("Logging Facility", name2logfac, lf_name);
}

#undef REGISTER_STUFF
