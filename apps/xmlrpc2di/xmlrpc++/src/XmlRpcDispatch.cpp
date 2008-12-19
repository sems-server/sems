
#include "XmlRpcDispatch.h"
#include "XmlRpcSource.h"
#include "XmlRpcUtil.h"

#include <errno.h>
#include <math.h>
#include <sys/timeb.h>

#if defined(_WINDOWS)
# include <winsock2.h>

# define USE_FTIME
# if defined(_MSC_VER)
#  define timeb _timeb
#  define ftime _ftime
# endif
#else
# include <sys/time.h>
#endif  // _WINDOWS


using namespace XmlRpc;


XmlRpcDispatch::XmlRpcDispatch()
{
  _endTime = -1.0;
  _doClear = false;
  _inWork = false;
}


XmlRpcDispatch::~XmlRpcDispatch()
{
}

// Monitor this source for the specified events and call its event handler
// when the event occurs
void
XmlRpcDispatch::addSource(XmlRpcSource* source, unsigned mask)
{
  _sources.push_back(MonitoredSource(source, mask));
}

// Stop monitoring this source. Does not close the source.
void
XmlRpcDispatch::removeSource(XmlRpcSource* source)
{
  for (SourceList::iterator it=_sources.begin(); it!=_sources.end(); ++it)
    if (it->getSource() == source)
    {
      _sources.erase(it);
      break;
    }
}


// Modify the types of events to watch for on this source
void 
XmlRpcDispatch::setSourceEvents(XmlRpcSource* source, unsigned eventMask)
{
  for (SourceList::iterator it=_sources.begin(); it!=_sources.end(); ++it)
    if (it->getSource() == source)
    {
      it->getMask() = eventMask;
      break;
    }
}



// Watch current set of sources and process events
void
XmlRpcDispatch::work(double timeout)
{
  // Compute end time
  double timeNow = getTime();
  _endTime = (timeout < 0.0) ? -1.0 : (timeNow + timeout);
  _doClear = false;
  _inWork = true;

  // Only work while there is something to monitor
  while (_sources.size() > 0) {

    // Wait for and dispatch events
    if ( ! waitForAndProcessEvents(timeout))
    {
      _inWork = false;
      return;
    }


    // Check whether to clear all sources
    if (_doClear)
    {
      SourceList closeList = _sources;
      _sources.clear();
      for (SourceList::iterator it=closeList.begin(); it!=closeList.end(); ++it) {
        XmlRpcSource *src = it->getSource();
        src->close();
      }

      _doClear = false;
    }

    // Check whether end time has passed or exit has been called
    if (_endTime == 0.0)        // Exit
      break;
    else if (_endTime > 0.0)    // Check for timeout
    {
      double t = getTime();
      if (t > _endTime)
        break;

      // Decrement timeout by elapsed time
      timeout -= (t - timeNow);
      if (timeout < 0.0) 
        timeout = 0.0;    // Shouldn't happen but its fp math...
      timeNow = t;
    }
  }

  _inWork = false;
}



// Exit from work routine. Presumably this will be called from
// one of the source event handlers.
void
XmlRpcDispatch::exit()
{
  _endTime = 0.0;   // Return from work asap
}


// Clear all sources from the monitored sources list
void
XmlRpcDispatch::clear()
{
  if (_inWork)
    _doClear = true;  // Finish reporting current events before clearing
  else
  {
    SourceList closeList = _sources;
    _sources.clear();
    for (SourceList::iterator it=closeList.begin(); it!=closeList.end(); ++it)
      it->getSource()->close();
  }
}


// Time utility
double
XmlRpcDispatch::getTime()
{
#ifdef USE_FTIME
  struct timeb	tbuff;

  ftime(&tbuff);
  return ((double) tbuff.time + ((double)tbuff.millitm / 1000.0) +
	  ((double) tbuff.timezone * 60));
#else
  struct timeval	tv;
  struct timezone	tz;

  gettimeofday(&tv, &tz);
  return (tv.tv_sec + tv.tv_usec / 1000000.0);
#endif /* USE_FTIME */
}


// Wait for I/O on any source, timeout, or interrupt signal.
bool
XmlRpcDispatch::waitForAndProcessEvents(double timeout)
{
#if defined(_WINDOWS) && 0

  int nHandles = 0;
  SourceList::iterator it;
  for (it=_sources.begin(); it!=_sources.end(); ++it) {
    int fd = it->getSource()->getfd();
    int mask = 0;
    if (it->getMask() & ReadableEvent) mask = (FD_READ | FD_CLOSE | FD_ACCEPT);
    if (it->getMask() & WritableEvent) mask |= (FD_WRITE | FD_CLOSE);

#else   // Posix

  // Construct the sets of descriptors we are interested in
  fd_set inFd, outFd, excFd;
  FD_ZERO(&inFd);
  FD_ZERO(&outFd);
  FD_ZERO(&excFd);

  int maxFd = -1;
  SourceList::iterator it;
  for (it=_sources.begin(); it!=_sources.end(); ++it) {
    int fd = it->getSource()->getfd();
    if (it->getMask() & ReadableEvent) FD_SET(fd, &inFd);
    if (it->getMask() & WritableEvent) FD_SET(fd, &outFd);
    if (it->getMask() & Exception)     FD_SET(fd, &excFd);
    if (it->getMask() && fd > maxFd)   maxFd = fd;
  }

  // Check for events
  int nEvents;
  if (_endTime < 0.0)
    nEvents = select(maxFd+1, &inFd, &outFd, &excFd, NULL);
  else 
  {
    struct timeval tv;
    tv.tv_sec = (int)floor(timeout);
    tv.tv_usec = ((int)floor(1000000.0 * (timeout-floor(timeout)))) % 1000000;
    nEvents = select(maxFd+1, &inFd, &outFd, &excFd, &tv);
  }

  if (nEvents < 0 && errno != EINTR)
  {
    XmlRpcUtil::error("Error in XmlRpcDispatch::work: error in select (%d).", nEvents);
    return false;
  }

  // Process events
  for (it=_sources.begin(); it != _sources.end(); )
  {
    SourceList::iterator thisIt = it++;
    XmlRpcSource* src = thisIt->getSource();
    int fd = src->getfd();

    if (fd <= maxFd) {
      // handleEvent is called once per event type signalled
      unsigned newMask = 0;
      int nset = 0;
      if (FD_ISSET(fd, &inFd))
      {
        newMask |= src->handleEvent(ReadableEvent);
        ++nset;
      }
      if (FD_ISSET(fd, &outFd))
      {
        newMask |= src->handleEvent(WritableEvent);
        ++nset;
      }
      if (FD_ISSET(fd, &excFd))
      {
        newMask |= src->handleEvent(Exception);
        ++nset;
      }

      // Some event occurred
      if (nset)
      {
        if (newMask)
          thisIt->getMask() = newMask;
        else       // Stop monitoring this one
        {
          _sources.erase(thisIt);
          if ( ! src->getKeepOpen())
            src->close();
        }
      }
    }
  }
#endif

  return true;
}
