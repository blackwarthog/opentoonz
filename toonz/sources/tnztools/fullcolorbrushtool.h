#pragma once

#ifndef FULLCOLORBRUSHTOOL_H
#define FULLCOLORBRUSHTOOL_H

#include <ctime>

#include "brushtool.h"
#include "mypainttoonzbrush.h"
#include "toonz/mypaintbrushstyle.h"
#include <QElapsedTimer>

//==============================================================

//  Forward declarations

class TTileSetFullColor;
class TTileSaverFullColor;
class MyPaintToonzBrush;
class FullColorBrushToolNotifier;
namespace mypaint { class Brush; }

//==============================================================

//************************************************************************
//    FullColor Brush Tool declaration
//************************************************************************

class FullColorBrushTool final : public TTool, public RasterController {
  Q_DECLARE_TR_FUNCTIONS(FullColorBrushTool)
public:
  class TrackHandler: public TTrackToolHandler {
  public:
    MyPaintToonzBrush brush;

    TrackHandler(
      const TRaster32P &ras,
      RasterController &controller,
      const mypaint::Brush &brush
    ):
      brush(ras, controller, brush) { }
  };

private:
  void updateCurrentStyle();
  void applyClassicToonzBrushSettings(mypaint::Brush &mypaintBrush);
  void applyToonzBrushSettings(mypaint::Brush &mypaintBrush);

public:
  FullColorBrushTool(std::string name);

  ToolType getToolType() const override
    { return LevelWriteTool; }
  ToolModifiers getToolModifiers() const override
    { return ModifierTangents | ModifierAssistants | ModifierCustom | ModifierSegmentation; }
  bool isAssistantsEnabled() const override
    { return true; }
  bool isCustomModifiersEnabled() const override
    { return true; }

  ToolOptionsBox *createOptionsBox() override;

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;

  bool preLeftButtonDown() override;
  void mouseMove(const TPointD &position, const TInputState &state) override;
  void paintBegin() override;
  void paintTrackPoint(const TTrackPoint &point, const TTrack &track, bool firstTrack);
  void paintEnd() override;

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;

  void onImageChanged() override;
  void setWorkAndBackupImages();
  void updateWorkAndBackupRasters(const TRect &rect);

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();

  void onCanvasSizeChanged();
  void onColorStyleChanged();

  TMyPaintBrushStyle* getBrushStyle();

protected:
  TPropertyGroup m_prop;

  TIntPairProperty    m_thickness;
  TBoolProperty       m_pressure;
  TDoublePairProperty m_opacity;
  TDoubleProperty     m_hardness;
  TDoubleProperty     m_modifierSize;
  TDoubleProperty     m_modifierOpacity;
  TBoolProperty       m_modifierEraser;
  TBoolProperty       m_modifierLockAlpha;
  TEnumProperty       m_preset;

  TPixel32 m_currentColor;
  bool m_enabledPressure;
  int m_minCursorThick, m_maxCursorThick;

  TPointD m_mousePos,  //!< Current mouse position, in world coordinates.
          m_brushPos;  //!< World position the brush will be painted at.

  TTrackPoint m_trackPoint;

  TRasterP m_backUpRas;
  TRaster32P m_workRaster;

  TRect m_strokeRect, m_strokeSegmentRect, m_lastRect;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance
  FullColorBrushToolNotifier *m_notifier;

  bool m_presetsLoaded;
  bool m_firstTime;
  bool m_started;
};

//------------------------------------------------------------

class FullColorBrushToolNotifier final : public QObject {
  Q_OBJECT

  FullColorBrushTool *m_tool;

public:
  FullColorBrushToolNotifier(FullColorBrushTool *tool);

protected slots:
  void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // FULLCOLORBRUSHTOOL_H
