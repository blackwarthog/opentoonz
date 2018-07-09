

#include <tools/inputmanager.h>

// TnzTools includes
#include <tools/tool.h>
#include <tools/toolhandle.h>

// TnzLib includes
#include <toonz/tapplication.h>
#include <toonz/tobjecthandle.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/dpiscale.h>

// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    static members
//*****************************************************************************************

static const bool debugInputManager = false;
TInputState::TouchId TInputManager::m_lastTouchId = 0;


//*****************************************************************************************
//    TInputModifier implementation
//*****************************************************************************************


void
TInputModifier::setManager(TInputManager *manager) {
  if (m_manager != manager)
    { m_manager = manager; onSetManager(); }
}


void
TInputModifier::modifyTrack(
  const TTrack &track,
  const TInputSavePoint::Holder &savePoint,
  TTrackList &outTracks )
{
  if (!track.handler) {
      track.handler = new TTrackHandler(track);
      track.handler->tracks.push_back(
        new TTrack(
          new TTrackModifier(*track.handler) ));
  }

  outTracks.insert(
    outTracks.end(),
    track.handler->tracks.begin(),
    track.handler->tracks.end() );
  if (!track.changed())
    return;

  int start = std::max(0, track.size() - track.pointsAdded);
  for(TTrackList::const_iterator ti = track.handler->tracks.begin(); ti != track.handler->tracks.end(); ++ti) {
    TTrack &subTrack = **ti;
    subTrack.truncate(start);
    for(int i = start; i < track.size(); ++i)
      subTrack.push_back( subTrack.modifier->calcPoint(i) );
  }
  track.resetChanges();
}


void
TInputModifier::modifyTracks(
    const TTrackList &tracks,
    const TInputSavePoint::Holder &savePoint,
    TTrackList &outTracks )
{
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    modifyTrack(**i, savePoint, outTracks);
}


void
TInputModifier::modifyHover(
  const TPointD &hover,
  THoverList &outHovers )
{
  outHovers.push_back(hover);
}


void
TInputModifier::modifyHovers(
  const THoverList &hovers,
  THoverList &outHovers )
{
  for(THoverList::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    modifyHover(*i, outHovers);
}


TRectD
TInputModifier::calcDrawBounds(const TTrackList &tracks, const THoverList &hovers) {
  TRectD bounds;
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    bounds += calcDrawBoundsTrack(**i);
  for(std::vector<TPointD>::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    bounds += calcDrawBoundsHover(*i);
  return bounds;
}


void
TInputModifier::drawTracks(const TTrackList &tracks) {
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    drawTrack(**i);
}


void
TInputModifier::drawHovers(const std::vector<TPointD> &hovers) {
  for(std::vector<TPointD>::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    drawHover(*i);
}


void
TInputModifier::draw(const TTrackList &tracks, const std::vector<TPointD> &hovers) {
  drawTracks(tracks);
  drawHovers(hovers);
}


//*****************************************************************************************
//    TInputManager implementation
//*****************************************************************************************


TInputManager::TInputManager():
  m_viewer(),
  m_tracks(1),
  m_hovers(1),
  m_started(),
  m_savePointsSent()
{
  // assign onToolSwitched
  assert(getApplication());
  assert(getApplication()->getCurrentTool());
  ToolHandle* handler = getApplication()->getCurrentTool();
  QObject::connect(handler, &ToolHandle::toolSwitched, this, &TInputManager::onToolSwitched);
}


void
TInputManager::paintRollbackTo(int saveIndex, TTrackList &subTracks) {
  if (saveIndex >= (int)m_savePoints.size())
    return;

  assert(isActive());
  TTool *tool = getTool();
  int level = saveIndex + 1;
  if (level <= m_savePointsSent) {
    if (level < m_savePointsSent)
      tool->paintPop(m_savePointsSent - level);
    tool->paintCancel();
    m_savePointsSent = level;
  }

  for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i) {
    TTrack &track = **i;
    if (TrackHandler *handler = dynamic_cast<TrackHandler*>(track.handler.getPointer())) {
      handler->saves.resize(level);
      int cnt = handler->saves[saveIndex];
      track.resetRemoved();
      track.pointsAdded = track.size() - cnt;
    }
  }
  for(int i = level; i < (int)m_savePoints.size(); ++i)
    m_savePoints[i].savePoint()->available = false;
  m_savePoints.resize(level);
}


void
TInputManager::paintApply(int count, TTrackList &subTracks) {
  if (count <= 0)
    return;

  assert(isActive());
  TTool *tool = getTool();
  int level = (int)m_savePoints.size() - count;
  bool resend = true;

  if (level < m_savePointsSent) {
    // apply
    int applied = tool->paintApply(m_savePointsSent - level);
    applied = std::max(0, std::min(m_savePointsSent - level, applied));
    m_savePointsSent -= applied;
    if (m_savePointsSent == level) resend = false;
  }

  if (level < m_savePointsSent) {
    // rollback
    tool->paintPop(m_savePointsSent - level);
    m_savePointsSent = level;
  }

  // remove keypoints
  for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i) {
    TTrack &track = **i;
    if (TrackHandler *handler = dynamic_cast<TrackHandler*>(track.handler.getPointer())) {
      if (resend) {
        track.resetRemoved();
        track.pointsAdded = track.size() - handler->saves[m_savePointsSent];
      }
      handler->saves.resize(level);
    }
  }
  for(int i = level; i < (int)m_savePoints.size(); ++i)
    m_savePoints[i].savePoint()->available = false;
  m_savePoints.resize(level);
}


void
TInputManager::paintTracks() {
  assert(isActive());
  TTool *tool = getTool();

  bool allFinished = true;
  for(TTrackList::const_iterator i = m_tracks.front().begin(); i != m_tracks.front().end(); ++i)
    if (!(*i)->finished())
      { allFinished = false; break; }

  while(true) {
    // run modifiers
    TInputSavePoint::Holder newSavePoint = TInputSavePoint::create(true);
    for(int i = 0; i < (int)m_modifiers.size(); ++i) {
      m_tracks[i+1].clear();
      m_modifiers[i]->modifyTracks(m_tracks[i], newSavePoint, m_tracks[i+1]);
    }
    TTrackList &subTracks = m_tracks.back();

    // is paint started?
    if (!m_started && !subTracks.empty()) {
      m_started = true;
      TTool::getApplication()->getCurrentTool()->setToolBusy(true);
      tool->paintBegin();
    }

    // create handlers
    for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
      if (!(*i)->handler)
        (*i)->handler = new TrackHandler(**i, (int)m_savePoints.size());

    if (!m_savePoints.empty()) {
      // rollback
      int rollbackIndex = (int)m_savePoints.size();
      for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i) {
        TTrack &track = **i;
        if (track.pointsRemoved > 0) {
          int count = track.size() - track.pointsAdded;
          if (TrackHandler *handler = dynamic_cast<TrackHandler*>(track.handler.getPointer()))
            while(rollbackIndex > 0 && (rollbackIndex >= (int)m_savePoints.size() || handler->saves[rollbackIndex] > count))
              --rollbackIndex;
        }
      }
      paintRollbackTo(rollbackIndex, subTracks);

      // apply
      int applyCount = 0;
      while(applyCount < (int)m_savePoints.size() && m_savePoints[(int)m_savePoints.size() - applyCount - 1].isFree())
        ++applyCount;
      paintApply(applyCount, subTracks);
    }

    // send to tool
    if (m_savePointsSent == (int)m_savePoints.size() && !subTracks.empty())
      tool->paintTracks(subTracks);
    for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
      (*i)->resetChanges();

    // is paint finished?
    newSavePoint.unlock();
    if (newSavePoint.isFree()) {
      newSavePoint.savePoint()->available = false;
      if (allFinished) {
        paintApply((int)m_savePoints.size(), subTracks);
        // send to tool final
        if (!subTracks.empty()) {
          tool->paintTracks(subTracks);
          for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
            (*i)->resetChanges();
        }
        for(std::vector<TTrackList>::iterator i = m_tracks.begin(); i != m_tracks.end(); ++i)
          i->clear();
        if (m_started) {
          tool->paintEnd();
          TTool::getApplication()->getCurrentTool()->setToolBusy(false);
          m_started = false;
        }
      }
      break;
    }

    // create save point
    if (tool->paintPush()) ++m_savePointsSent;
    m_savePoints.push_back(newSavePoint);
    for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
      if (TrackHandler *handler = dynamic_cast<TrackHandler*>((*i)->handler.getPointer()))
        handler->saves.push_back((*i)->size());
  }
}


int
TInputManager::trackCompare(
  const TTrack &track,
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId )
{
  if (track.deviceId < deviceId) return -1;
  if (deviceId < track.deviceId) return  1;
  if (track.touchId < touchId) return -1;
  if (touchId < track.touchId) return 1;
  return 0;
}


const TTrackP&
TInputManager::createTrack(
  int index,
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  TTimerTicks ticks,
  bool hasPressure,
  bool hasTilt )
{
  TTrackP track = new TTrack(
    deviceId,
    touchId,
    state.keyHistoryHolder(ticks),
    state.buttonHistoryHolder(deviceId, ticks),
    hasPressure,
    hasTilt );
  return *m_tracks.front().insert(m_tracks[0].begin() + index, track);
}


const TTrackP&
TInputManager::getTrack(
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  TTimerTicks ticks,
  bool hasPressure,
  bool hasTilt )
{
  TTrackList &origTracks = m_tracks.front();
  if (origTracks.empty())
    return createTrack(0, deviceId, touchId, ticks, hasPressure, hasTilt);
  int cmp;

  int a = 0;
  cmp = trackCompare(*origTracks[a], deviceId, touchId);
  if (cmp == 0) return origTracks[a];
  if (cmp < 0) return createTrack(a, deviceId, touchId, ticks, hasPressure, hasTilt);

  int b = (int)origTracks.size() - 1;
  cmp = trackCompare(*origTracks[b], deviceId, touchId);
  if (cmp == 0) return origTracks[b];
  if (cmp > 0) return createTrack(b+1, deviceId, touchId, ticks, hasPressure, hasTilt);

  // binary search: tracks[a] < tracks[c] < tracks[b]
  while(true) {
    int c = (a + b)/2;
    if (a == c) break;
    cmp = trackCompare(*origTracks[c], deviceId, touchId);
    if (cmp < 0) b = c; else
      if (cmp > 0) a = c; else
        return origTracks[c];
  }
  return createTrack(b, deviceId, touchId, ticks, hasPressure, hasTilt);
}


void
TInputManager::addTrackPoint(
  const TTrackP& track,
  const TPointD &position,
  double pressure,
  const TPointD &tilt,
  double time,
  bool final )
{
  track->push_back( TTrackPoint(
    position,
    pressure,
    tilt,
    (double)track->size(),
    time,
    0.0, // length will calculated inside of TTrack::push_back
    final ));
}


void
TInputManager::touchTracks(bool finish) {
  for(TTrackList::const_iterator i = m_tracks.front().begin(); i != m_tracks.front().end(); ++i) {
    if (!(*i)->finished() && (*i)->size() > 0) {
      const TTrackPoint &p = (*i)->back();
      addTrackPoint(*i, p.position, p.pressure, p.tilt, p.time, finish);
    }
  }
}


void
TInputManager::modifierActivate(const TInputModifierP &modifier) {
  modifier->setManager(this);
  modifier->activate();
}


void
TInputManager::modifierDeactivate(const TInputModifierP &modifier) {
  modifier->deactivate();
  modifier->setManager(NULL);
}


void
TInputManager::processTracks() {
  if (isActive()) {
    paintTracks();
    TRectD bounds = calcDrawBounds();
    if (!bounds.isEmpty())
      getViewer()->GLInvalidateRect(bounds);
  }
}


void
TInputManager::finishTracks() {
  if (isActive()) {
    touchTracks(true);
    processTracks();
  } else {
    reset();
  }
}


void
TInputManager::reset() {
  // forget about tool paint stack
  // assuime it was already reset by outside
  m_started = false;
  m_savePointsSent = 0;

  // reset save point
  for(int i = 0; i < (int)m_savePoints.size(); ++i)
    m_savePoints[i].savePoint()->available = false;
  m_savePoints.clear();

  // reset tracks
  for(int i = 0; i < (int)m_tracks.size(); ++i)
    m_tracks[i].clear();
}


void
TInputManager::setViewer(TToolViewer *viewer)
  { m_viewer = viewer; }


bool
TInputManager::isActive() const {
  TTool* tool = getTool();
  return getViewer() && tool && tool->isEnabled();
}


TApplication*
TInputManager::getApplication()
  { return TTool::getApplication(); }


TTool*
TInputManager::getTool() {
  if (TApplication *application = getApplication())
    if (ToolHandle *handle = application->getCurrentTool())
      return handle->getTool();
  return NULL;
}


void
TInputManager::onToolSwitched()
  { reset(); }


int
TInputManager::findModifier(const TInputModifierP &modifier) const {
  for(int i = 0; i < getModifiersCount(); ++i)
    if (getModifier(i) == modifier)
      return i;
  return -1;
}


void
TInputManager::insertModifier(int index, const TInputModifierP &modifier) {
  if (findModifier(modifier) >= 0) return;
  finishTracks();
  m_modifiers.insert(m_modifiers.begin() + index, modifier);
  m_tracks.insert(m_tracks.begin() + index + 1, TTrackList());
  m_hovers.insert(m_hovers.begin() + index + 1, THoverList());
  modifierActivate(modifier);
}


void
TInputManager::removeModifier(int index) {
  if (index >= 0 && index < getModifiersCount()) {
    finishTracks();
    modifierDeactivate(m_modifiers[index]);
    m_modifiers.erase(m_modifiers.begin() + index);
    m_tracks.erase(m_tracks.begin() + index + 1);
    m_hovers.erase(m_hovers.begin() + index + 1);
  }
}


void
TInputManager::clearModifiers() {
  while(getModifiersCount() > 0)
    removeModifier(getModifiersCount() - 1);
}


void TInputManager::updateDpiScale() const {
  if (TApplication *application = getApplication())
    if (TTool *tool = getTool())
      if (TXshLevelHandle *levelHandle = application->getCurrentLevel())
        if (TXshLevel *level = levelHandle->getLevel())
          if (TXshSimpleLevel *simpleLevel = level->getSimpleLevel())
            { m_dpiScale = getCurrentDpiScale(simpleLevel, tool->getCurrentFid()); return; }
  m_dpiScale = TPointD(1.0, 1.0);
}


TAffine
TInputManager::toolToWorld() const {
  if (!isActive()) return TAffine();

  TAffine matrix = getTool()->getMatrix();
  if (getTool()->getToolType() & TTool::LevelTool)
    if (TObjectHandle *objHandle = getApplication()->getCurrentObject())
      if (!objHandle->isSpline())
        matrix *= TScale(m_dpiScale.x, m_dpiScale.y);

  return matrix;
}


TAffine
TInputManager::worldToTool() const
  { return toolToWorld().inv(); }


TAffine
TInputManager::worldToScreen() const {
  return screenToWorld().inv();
}


TAffine
TInputManager::screenToWorld() const {
  if (TToolViewer *viewer = getViewer())
    return viewer->get3dViewMatrix().get2d();
  return TAffine();
}


void
TInputManager::trackEvent(
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  const TPointD &position,
  const double *pressure,
  const TPointD *tilt,
  bool final,
  TTimerTicks ticks )
{
  if (isActive() && getInputTracks().empty()) {
    TToolViewer *viewer = getTool()->getViewer();
    if (getTool()->preLeftButtonDown())
      getTool()->setViewer(viewer);
    updateDpiScale();
  }

  if (isActive()) {
    TTrackP track = getTrack(deviceId, touchId, ticks, (bool)pressure, (bool)tilt);
    if (!track->finished()) {
      double time = (double)(ticks - track->ticks())*TToolTimer::step - track->timeOffset();
      addTrackPoint(
        track,
        worldToTool() * position,
        pressure ? *pressure : 0.5,
        tilt ? *tilt : TPointD(),
        time,
        final );
    }
  }
}


bool
TInputManager::keyEvent(
  bool press,
  TInputState::Key key,
  TTimerTicks ticks,
  QKeyEvent *event )
{
  bool result = false;
  bool wasPressed = state.isKeyPressed(key);
  state.keyEvent(press, key, ticks);
  if (isActive()) {
    processTracks();
    result = getTool()->keyEvent(press, key, event, *this);
    if (wasPressed != press) {
      touchTracks();
      processTracks();
      //hoverEvent(getInputHovers());
    }
  }
  return result;
}


void
TInputManager::buttonEvent(
  bool press,
  TInputState::DeviceId deviceId,
  TInputState::Button button,
  TTimerTicks ticks )
{
  bool wasPressed = state.isButtonPressed(deviceId, button);
  state.buttonEvent(press, deviceId, button, ticks);
  if (isActive()) {
    processTracks();
    getTool()->buttonEvent(press, deviceId, button, *this);
    if (wasPressed != press) {
      touchTracks();
      processTracks();
      //hoverEvent(getInputHovers());
    }
  }
}


void
TInputManager::hoverEvent(const THoverList &hovers) {
  if (&m_hovers.front() != &hovers)
    m_hovers.front() = hovers;

  TAffine matrix = worldToTool();
  for(THoverList::iterator i = m_hovers.front().begin(); i != m_hovers.front().end(); ++i)
    *i = matrix * (*i);

  for(int i = 0; i < (int)m_modifiers.size(); ++i) {
    m_hovers[i+1].clear();
    m_modifiers[i]->modifyHovers(m_hovers[i], m_hovers[i+1]);
  }
  if (isActive()) {
    TRectD bounds = calcDrawBounds();
    if (!bounds.isEmpty())
      getViewer()->GLInvalidateRect(bounds);
    getTool()->hoverEvent(*this);
  }
}


void
TInputManager::doubleClickEvent() {
  if (isActive())
    getTool()->doubleClickEvent(*this);
}


void
TInputManager::textEvent(
  const std::wstring &preedit,
  const std::wstring &commit,
  int replacementStart,
  int replacementLen )
{
  if (isActive())
    getTool()->onInputText(preedit, commit, replacementStart, replacementLen);
}


void
TInputManager::enverEvent() {
  if (isActive())
    getTool()->onEnter();
}


void
TInputManager::leaveEvent() {
  if (isActive())
    getTool()->onLeave();
}


TRectD
TInputManager::calcDrawBounds() {
  if (debugInputManager)
    return TConsts::infiniteRectD;

  TRectD bounds;
  if (isActive()) {
    for(int i = 0; i < (int)m_modifiers.size(); ++i)
      bounds += m_modifiers[i]->calcDrawBounds(m_tracks[i], m_hovers[i]);

    if (m_savePointsSent < (int)m_savePoints.size()) {
      for(TTrackList::const_iterator ti = getOutputTracks().begin(); ti != getOutputTracks().end(); ++ti) {
        TTrack &track = **ti;
        if (TrackHandler *handler = dynamic_cast<TrackHandler*>(track.handler.getPointer())) {
          int start = handler->saves[m_savePointsSent] - 1;
          if (start < 0) start = 0;
          if (start + 1 < track.size())
            for(int i = start + 1; i < track.size(); ++i)
              bounds += boundingBox(track[i-1].position, track[i].position);
        }
      }
    }

    if (!bounds.isEmpty()) {
      bounds = toolToWorld()*bounds;
      if (!bounds.isEmpty())
        bounds.enlarge(4.0);
    }
  }
  return bounds;
}


void
TInputManager::draw() {
  if (!isActive()) return;
  getTool()->draw();
  TToolViewer *viewer = getViewer();

  // paint not sent sub-tracks
  if (debugInputManager || m_savePointsSent < (int)m_savePoints.size()) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    tglEnableBlending();
    tglEnableLineSmooth(true, 0.5);
    double pixelSize = sqrt(tglGetPixelSize2());
    double colorBlack[4] = { 0.0, 0.0, 0.0, 1.0 };
    double colorWhite[4] = { 1.0, 1.0, 1.0, 1.0 };
    for(TTrackList::const_iterator ti = getOutputTracks().begin(); ti != getOutputTracks().end(); ++ti) {
      TTrack &track = **ti;
      if (TrackHandler *handler = dynamic_cast<TrackHandler*>(track.handler.getPointer())) {
        int start = debugInputManager ? 0 : handler->saves[m_savePointsSent] - 1;
        if (start < 0) start = 0;
        if (start + 1 < track.size()) {
          int level = m_savePointsSent;
          colorBlack[3] = (colorWhite[3] = 0.8);
          double radius = 2.0;
          for(int i = start + 1; i < track.size(); ++i) {
            while(level < (int)handler->saves.size() && handler->saves[level] <= i)
              colorBlack[3] = (colorWhite[3] *= 0.8), ++level;

            const TPointD &a = track[i-1].position;
            const TPointD &b = track[i].position;
            TPointD d = b - a;

            double k = norm2(d);
            if (k > TConsts::epsilon*TConsts::epsilon) {
              k = 0.5*pixelSize/sqrt(k);
              d = TPointD(-k*d.y, k*d.x);
              glColor4dv(colorWhite);
              tglDrawSegment(a - d, b - d);
              glColor4dv(colorBlack);
              tglDrawSegment(a + d, b + d);
              radius = 2.0;
            } else {
              radius += 2.0;
            }

            if (debugInputManager) {
              glColor4d(0.0, 0.0, 0.0, 0.25);
              tglDrawCircle(b, radius*pixelSize);
            }
          }
        }
      }
    }
    glPopAttrib();
  }

  // paint modifiers
  for(int i = 0; i < (int)m_modifiers.size(); ++i)
    m_modifiers[i]->draw(m_tracks[i], m_hovers[i]);
}

TInputState::TouchId
TInputManager::genTouchId()
  { return ++m_lastTouchId; }
