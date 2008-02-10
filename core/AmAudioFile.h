/*
 * $Id: AmAudio.h 633 2008-01-28 18:17:36Z sayer $
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
  /**
   * Constructor for file based formats.
   * All information are taken from the file descriptor.
   * @param name The file format name (ex. "Wav").
   * @param fd A file descriptor filled by the a plug-in's open function.
   */
  AmAudioFileFormat(const string& name, const amci_file_desc_t* fd);

  /** @return Format name. */
  string        getName() { return name; }
  /** @return Format subtype. */
  int           getSubtypeId() { return subtype; }
  /** @return Subtype pointer. */
  amci_subtype_t*  getSubtype();

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
  AmAudioFileFormat* fileName2Fmt(const string& name);

  /** internal function for opening the file */
  int fpopen_int(const string& filename, OpenMode mode, FILE* n_fp);

public:
  AmSharedVar<bool> loop;

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

  /**
   * Rewind the file.
   */
  void rewind();

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
