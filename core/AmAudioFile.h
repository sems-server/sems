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
/** @file AmAudioFile.h */
#ifndef _AmAudioFile_h_
#define _AmAudioFile_h_

#include "AmAudio.h"
#include "AmBufferedAudio.h"

/** \brief \ref AmAudioFormat for file */
class AmAudioFileFormat: public AmAudioFormat
{
  /** == "" if not yet initialized. */
  string          name;
    
  /** == -1 if not yet initialized. */
  int             subtype;

  /** ==  0 if not yet initialized. */
  amci_subtype_t* p_subtype;

protected:
  int getCodecId();

public:
  /**
   * Constructor for file based formats.
   * All information are taken from the plug-in description.
   * @param name The file format name (ex. "Wav").
   * @param subtype Subtype for the file format (see amci.h).
   */
  AmAudioFileFormat(const string& name, int subtype = -1);

  virtual ~AmAudioFileFormat()  { }

  /** @return Format name. */
  string        getName() { return name; }
  /** @return Format subtype. */
  int           getSubtypeId() { return subtype; }
  /** @return Subtype pointer. */
  virtual amci_subtype_t*  getSubtype();

  void setSubtypeId(int subtype_id);
};

/**
 * \brief AmAudio implementation for file access
 */
class AmAudioFile: public AmBufferedAudio
{
public:
  /** Open mode. */
  enum OpenMode { Read=1, Write=2 };

protected:
  /** Pointer to the file opened as last. */
  FILE* fp;
  long begin;

  /** Format of that file. @see fp, open(). */
  amci_inoutfmt_t* iofmt;
  /** Open mode. */
  int open_mode;

  /** Size of datas having been read/written until now. */
  int data_size;

  bool on_close_done;
  bool close_on_exit;

  /** @see AmAudio::read */
  int read(unsigned int user_ts, unsigned int size);

  /** @see AmAudio::write */
  int write(unsigned int user_ts, unsigned int size);

  /** @return a file format from file name. (ex: '1234.wav') */
  virtual AmAudioFileFormat* fileName2Fmt(const string& name, const string& subtype);

  /** @return subtype ID and trim filename if subtype embedded */
  string getSubtype(string& filename);

  /** internal function for opening the file */
  int fpopen_int(const string& filename, OpenMode mode, FILE* n_fp, const string& subtype);

public:
  AmSharedVar<bool> loop;
  AmSharedVar<bool> autorewind;

  AmAudioFile();
  ~AmAudioFile();

  /**
   * Opens a file.
   * <ul>
   * <li>In read mode: sets input format.
   * <li>In write mode: <ol>
   *                    <li>needs output format set. 
   *                    <li>If file name already exists, 
   *                        the file will be overwritten.
   *                    </ol>
   * </ul>
   * @param filename Name of the file.
   * @param mode Open mode.
   * @return 0 if everything's OK
   * @see OpenMode
   */
  int open(const string& filename, OpenMode mode, 
	   bool is_tmp=false);

  int fpopen(const string& filename, OpenMode mode, FILE* n_fp);

  /** Rewind the file to beginning. */
  void rewind();

  /** Rewind the file some milliseconds. */
  void rewind(unsigned int msec);

  /** skip forward some milliseconds. */
  void forward(unsigned int msec); 

  /** Closes the file. */
  void close();

  /** Executes the handler's on_close. */
  void on_close();

  /** be carefull with this one ;-) */ 
  FILE* getfp() { return fp; }

  OpenMode getMode() { return (OpenMode)open_mode; }

  /** Gets data size in the current file */
  int getDataSize() { return data_size; }

  /** Gets length of the current file in ms */
  int getLength();

  /**
   * @return MIME type corresponding to the audio file.
   */
  string getMimeType();

  void setCloseOnDestroy(bool cod){
    close_on_exit = cod;
  }
};

#endif

// Local Variables:
// mode:C++
// End:
