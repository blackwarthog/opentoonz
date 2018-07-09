#pragma once

#ifndef TOOLTIMER_INCLUDED
#define TOOLTIMER_INCLUDED

// TnzCore includes
#include <tcommon.h>

// Qt includes
#include <QElapsedTimer>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//===================================================================

//*****************************************************************************************
//    TToolTimer definition
//*****************************************************************************************

typedef qint64 TTimerTicks;

class DVAPI TToolTimer {
private:
  static TToolTimer instance;
  QElapsedTimer timer;
  TToolTimer();

public:
  static const TTimerTicks frequency;
  static const double step;
  static const double epsilon;

  static inline TTimerTicks ticks()
    { return instance.timer.nsecsElapsed(); }
};


#endif
