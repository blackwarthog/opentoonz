#pragma once

#ifndef MODIFIERASSISTANTS_INCLUDED
#define MODIFIERASSISTANTS_INCLUDED

// TnzTools includes
#include <tools/assistant.h>
#include <tools/inputmanager.h>

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
//    TModifierAssistants definition
//*****************************************************************************************

class DVAPI TModifierAssistants: public TInputModifier {
public:
  class DVAPI Modifier: public TTrackModifier {
  public:
    bool initialized;
    TInputSavePoint::Holder savePoint;
    TGuidelineList guidelines;

    Modifier(TTrackHandler &handler);
    TTrackPoint calcPoint(double originalIndex) override;
  };

private:
  bool scanAssistants(
    const TPointD *positions,
    int positionsCount,
    TGuidelineList *outGuidelines,
    bool draw ) const;

public:
  const bool drawOnly;
  const double sensitiveLength;

  explicit TModifierAssistants(bool drawOnly = false);

  void modifyTrack(
    const TTrack &track,
    const TInputSavePoint::Holder &savePoint,
    TTrackList &outTracks ) override;

  TRectD calcDrawBounds(const TTrackList &tracks, const THoverList &hovers) override;

  void drawTrack(const TTrack &track) override;
  void draw(const TTrackList &tracks, const THoverList &hovers) override;
};


#endif
