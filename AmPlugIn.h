/*
 * $Id: AmPlugIn.h,v 1.9.2.2 2005/04/20 11:56:28 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#ifndef _AmPlugIn_h_
#define _AmPlugIn_h_

#include <string>
#include <map>
#include <vector>
using std::string;
using std::map;
using std::vector;

class AmStateFactory;
struct amci_exports_t;
struct amci_codec_t;
struct amci_payload_t;
struct amci_inoutfmt_t;
struct amci_subtype_t;

/**
 * Container for loaded Plug-ins.
 */
class AmPlugIn
{
 public:
    enum PlugInType {
      Audio,
      App
    };

private:
    static AmPlugIn* _instance;

    vector<void*> dlls;

    map<int,amci_codec_t*>       codecs;
    map<int,amci_payload_t*>     payloads;
    map<string,amci_inoutfmt_t*> file_formats;
    map<string,AmStateFactory*>  name2app;

    int dynamic_pl; // range: 96->127, see RFC 1890
    
    AmPlugIn();
    ~AmPlugIn();

    /** @return -1 if failed, else 0. */
    int loadPlugIn(const string& file, PlugInType type);
    int loadAudioPlugIn(amci_exports_t* exports);
    int loadAppPlugIn(AmStateFactory* cb);

    int addCodec(amci_codec_t* c);
    int addPayload(amci_payload_t* p);
    int addFileFormat(amci_inoutfmt_t* f);

 public:

    static AmPlugIn* instance();

    /** 
     * Loads all plug-ins from the directory given as parameter. 
     * @return -1 if failed, else 0.
     */
    int load(const string& directory, PlugInType type);

    /** 
     * Payload lookup function.
     * @param payload_id Payload ID.
     * @return NULL if failed .
     */
    amci_payload_t*  payload(int payload_id);
    /** @return the suported payloads. */
    const map<int,amci_payload_t*>& getPayloads() { return payloads; }
    /** 
     * File format lookup according to the 
     * format name and/or file extension.
     * @param fmt_name Format name.
     * @param ext File extension.
     * @return NULL if failed.
     */
    amci_inoutfmt_t* fileFormat(const string& fmt_name, const string& ext = "");
    /** 
     * File format's subtype lookup function.
     * @param iofmt The file format.
     * @param subtype Subtype ID (see plug-in declaration for values).
     * @return NULL if failed.
     */
    amci_subtype_t*  subtype(amci_inoutfmt_t* iofmt, int subtype);
    /** 
     * Codec lookup function.
     * @param id Codec ID (see amci/codecs.h).
     * @return NULL if failed.
     */
    amci_codec_t*    codec(int id);
    /**
     * Application lookup function
     * @param app_name application name
     * @return NULL if failed (-> application not found).
     */
    AmStateFactory* getFactory4App(const string& app_name);
};

#endif

// Local Variables:
// mode:C++
// End:

