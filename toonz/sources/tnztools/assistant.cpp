
#include <tools/assistant.h>

#include <tgl.h>
#include <tproperty.h>

#include <limits>


//************************************************************************
//    TGuideline implementation
//************************************************************************

void
TGuideline::drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize, bool active) const {
  double colorBlack[4] = { 0.0, 0.0, 0.0, 0.5 };
  double colorWhite[4] = { 1.0, 1.0, 1.0, 0.5 };
  if (!active) colorBlack[3] = (colorWhite[3] *= 0.5);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 0.5);
  TPointD d = p1 - p0;
  double k = norm2(d);
  if (k > TConsts::epsilon*TConsts::epsilon) {
    k = 0.5*pixelSize/sqrt(k);
    d = TPointD(-k*d.y, k*d.x);
    glColor4dv(colorWhite);
    tglDrawSegment(p0 - d, p1 - d);
    glColor4dv(colorBlack);
    tglDrawSegment(p0 + d, p1 + d);
  }
  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

double
TGuideline::calcTrackWeight(const TTrack &track, const TAffine &toScreen, bool &outLongEnough) const {
  outLongEnough = false;
  if (track.size() < 2)
    return std::numeric_limits<double>::infinity();

  const double snapLenght = 20.0;
  const double snapScale = 1.0;
  const double maxLength = 2.0*snapLenght*snapScale;

  double sumWeight = 0.0;
  double sumLength = 0.0;
  double sumDeviation = 0.0;

  TPointD prev = toScreen*track[0].position;
  for(int i = 0; i < track.size(); ++i) {
    const TTrackPoint &tp = track[i];
    TPointD p = toScreen*tp.position;
    double length = tdistance(p, prev);
    sumLength += length;

    double midStepLength = sumLength - 0.5*length;
    if (midStepLength > TConsts::epsilon) {
      double weight = length*logNormalDistribuitionUnscaled(midStepLength, snapLenght, snapScale);
      sumWeight += weight;

      TTrackPoint ntp = transformPoint(tp);
      double deviation = tdistance(toScreen*ntp.position, p);
      sumDeviation += weight*deviation;
    }
    prev = p;

    if (sumLength >= maxLength)
      { outLongEnough = true; break; }
  }
  return sumWeight > TConsts::epsilon
       ? sumDeviation/sumWeight
       : std::numeric_limits<double>::infinity();
}

//---------------------------------------------------------------------------------------------------

TGuidelineP
TGuideline::findBest(const TGuidelineList &guidelines, const TTrack &track, const TAffine &toScreen, bool &outLongEnough) {
  outLongEnough = true;
  double bestWeight = 0.0;
  TGuidelineP best;
  for(TGuidelineList::const_iterator i = guidelines.begin(); i != guidelines.end(); ++i) {
    double weight = (*i)->calcTrackWeight(track, toScreen, outLongEnough);
    if (!best || weight < bestWeight)
      { bestWeight = weight; best = *i; }
  }
  return best;
}


//************************************************************************
//    TAssistantPoint implementation
//************************************************************************

TAssistantPoint::TAssistantPoint(
  Type type,
  const TPointD &position
):
  type(type),
  position(position),
  radius(10.0),
  selected() { }

//---------------------------------------------------------------------------------------------------

TAssistantPoint::TAssistantPoint(
  Type type,
  const TPointD &position,
  double radius
):
  type(type),
  position(position),
  radius(radius),
  selected() { }


//************************************************************************
//    TAssistantType implementation
//************************************************************************

TMetaObjectHandler*
TAssistantType::createHandler(TMetaObject &obj) const
  { return createAssistant(obj); }


//************************************************************************
//    TAssistant implementation
//************************************************************************

TAssistant::TAssistant(TMetaObject &object):
  TMetaObjectHandler(object),
  m_idEnabled("enabled"),
  m_idPoints("points"),
  m_idX("x"),
  m_idY("y"),
  m_idMagnetism("magnetism")
{
  addProperty( new TBoolProperty(m_idEnabled.str(), getEnabled()) );
  addProperty( new TDoubleProperty(m_idMagnetism.str(), 0.0, 1.0, getMagnetism()) );
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::addProperty(TProperty *p)
  { m_properties.add(p); }

//---------------------------------------------------------------------------------------------------

void
TAssistant::setTranslation(const TStringId &name, const QString &localName) const
  { m_properties.getProperty(name)->setQStringName( localName ); }

//---------------------------------------------------------------------------------------------------

void
TAssistant::updateTranslation() const {
  setTranslation(m_idEnabled, tr("Enabled"));
  setTranslation(m_idMagnetism, tr("Magnetism"));
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onSetDefaults() {
  setEnabled(true);
  setMagnetism(1.0);
}

//---------------------------------------------------------------------------------------------------

const TPointD&
TAssistant::blank() {
  static TPointD point;
  return point;
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::fixPoints()
  { onFixPoints(); }

//---------------------------------------------------------------------------------------------------

void
TAssistant::movePoint(int index, const TPointD &position)
  { if (index >= 0 && index < (int)m_points.size()) onMovePoint(index, position); }

//---------------------------------------------------------------------------------------------------

void
TAssistant::setPointSelection(int index, bool selected)  const {
  if (index >= 0 && index < pointsCount())
    m_points[index].selected = selected;
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onDataChanged(const TVariant &value) {
  const TVariant& pointsData = data()[m_idPoints];
  TVariantPathEntry entry;

  if (&value == &data() || &value == &pointsData)
    onAllDataChanged();
  else
  if (pointsData.getChildPathEntry(value, entry) && entry.isIndex()) {
    const TVariant& pointData = pointsData[entry];
    TPointD position = TPointD(
      pointData[m_idX].getDouble(),
      pointData[m_idY].getDouble() );
    movePoint(entry.index(), position);
  } else
  if (data().getChildPathEntry(value, entry) && entry.isField()) {
    updateProperty(entry.field(), data()[entry.field()]);
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onAllDataChanged() {
  const TVariant& pointsData = data()[m_idPoints];
  for(int i = 0; i < pointsCount(); ++i) {
    const TVariant& pointData = pointsData[i];
    m_points[i].position = TPointD(
      pointData[m_idX].getDouble(),
      pointData[m_idY].getDouble() );
  }
  fixPoints();
  updateProperties();
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onFixPoints()
  { }

//---------------------------------------------------------------------------------------------------

void
TAssistant::onMovePoint(int index, const TPointD &position)
  { m_points[index].position = position; }

//---------------------------------------------------------------------------------------------------

void
TAssistant::onFixData() {
  TVariant& pointsData = data()[m_idPoints];
  for(int i = 0; i < pointsCount(); ++i) {
    TVariant& pointData = pointsData[i];
    pointData[m_idX].setDouble( m_points[i].position.x );
    pointData[m_idY].setDouble( m_points[i].position.y );
  }
  setMagnetism( std::max(0.0, std::min(1.0, getMagnetism())) );
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::updateProperties() {
  const TVariantMap &map = data().getMap();
  for(TVariantMap::const_iterator i = map.begin(); i != map.end(); ++i)
    if (i->first != m_idPoints)
      updateProperty(i->first, i->second);
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::updateProperty(const TStringId &name, const TVariant &value) {
  TProperty *property = m_properties.getProperty(name);
  if (!property)
    return;

  if (TBoolProperty *boolProperty = dynamic_cast<TBoolProperty*>(property)) {
    boolProperty->setValue( value.getBool() );
  } else
  if (TDoubleProperty *doubleProperty = dynamic_cast<TDoubleProperty*>(property)) {
    doubleProperty->setValue( value.getDouble() );
  } else
  if (TStringProperty *stringProperty = dynamic_cast<TStringProperty*>(property)) {
    stringProperty->setValue( to_wstring(value.getString()) );
  } else
  if (TEnumProperty *enumProperty = dynamic_cast<TEnumProperty*>(property)) {
    enumProperty->setValue( to_wstring(value.getString()) );
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onPropertyChanged(const TStringId &name) {
  TProperty *property = m_properties.getProperty(name);
  if (!property)
    return;

  if (name == m_idPoints)
    return;

  if (TBoolProperty *boolProperty = dynamic_cast<TBoolProperty*>(property)) {
    data()[name].setBool( boolProperty->getValue() );
  } else
  if (TDoubleProperty *doubleProperty = dynamic_cast<TDoubleProperty*>(property)) {
    data()[name].setDouble( doubleProperty->getValue() );
  } else
  if (TStringProperty *stringProperty = dynamic_cast<TStringProperty*>(property)) {
    data()[name].setString( to_string(stringProperty->getValue()) );
  } else
  if (TEnumProperty *enumProperty = dynamic_cast<TEnumProperty*>(property)) {
    data()[name].setString( to_string(enumProperty->getValue()) );
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize) const {
  double colorBlack[4] = { 0.0, 0.0, 0.0, 0.5 };
  double colorWhite[4] = { 1.0, 1.0, 1.0, 0.5 };

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 0.5);
  TPointD d = p1 - p0;
  double k = norm2(d);
  if (k > TConsts::epsilon*TConsts::epsilon) {
    k = 0.5*pixelSize/sqrt(k);
    d = TPointD(-k*d.y, k*d.x);
    glColor4dv(colorWhite);
    tglDrawSegment(p0 - d, p1 - d);
    glColor4dv(colorBlack);
    tglDrawSegment(p0 + d, p1 + d);
  }
  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::drawPoint(const TAssistantPoint &point, double pixelSize) const {
  double radius = point.radius;
  double crossSize = 1.2*radius;

  double colorBlack[4] = { 0.0, 0.0, 0.0, 0.5 };
  double colorGray[4]  = { 0.5, 0.5, 0.5, 0.5 };
  double colorWhite[4] = { 1.0, 1.0, 1.0, 0.5 };
  double width = 0.5;

  if (point.selected) {
    colorBlack[2] = 1.0;
    colorGray[2] = 1.0;
    width = 2.0;
  }

  glPushAttrib(GL_ALL_ATTRIB_BITS);

  // fill
  tglEnableBlending();
  if (point.type == TAssistantPoint::CircleFill) {
    glColor4dv(colorGray);
    tglDrawDisk(point.position, radius*pixelSize);
  }

  TPointD crossDx(pixelSize*crossSize, 0.0);
  TPointD crossDy(0.0, pixelSize*crossSize);

  // back line
  tglEnableLineSmooth(true, 2.0*std::max(1.0, width));
  glColor4dv(colorWhite);
  if (point.type == TAssistantPoint::CircleCross) {
    tglDrawSegment(point.position - crossDx, point.position + crossDx);
    tglDrawSegment(point.position - crossDy, point.position + crossDy);
  }
  tglDrawCircle(point.position, radius*pixelSize);

  // front line
  glLineWidth(width);
  glColor4dv(colorBlack);
  if (point.type == TAssistantPoint::CircleCross) {
    tglDrawSegment(point.position - crossDx, point.position + crossDx);
    tglDrawSegment(point.position - crossDy, point.position + crossDy);
  }
  tglDrawCircle(point.position, radius*pixelSize);

  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::getGuidelines(const TPointD &position, const TAffine &toTool, TGuidelineList &outGuidelines) const
  { }

//---------------------------------------------------------------------------------------------------

void
TAssistant::draw(TToolViewer *viewer) const
  { }

//---------------------------------------------------------------------------------------------------

void
TAssistant::drawEdit(TToolViewer *viewer) const {
  // paint all points
  draw(viewer);
  double pixelSize = sqrt(tglGetPixelSize2());
  for(int i = 0; i < pointsCount(); ++i)
    drawPoint(m_points[i], pixelSize);
}

//---------------------------------------------------------------------------------------------------
