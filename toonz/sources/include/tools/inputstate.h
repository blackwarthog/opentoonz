#pragma once

#ifndef INPUTSTATE_INCLUDED
#define INPUTSTATE_INCLUDED

// TnzTools includes
#include <tools/tooltimer.h>
#include <tools/keyhistory.h>

// TnzCore includes
#include <tcommon.h>
#include <tsmartpointer.h>
#include <tgeometry.h>

// Qt includes
#include <Qt>

// std includes
#include <map>
#include <vector>


#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//====================================================

//  Forward declarations

typedef std::vector<TPointD> THoverList;

//===================================================================


//*****************************************************************************************
//    TKey definition
//*****************************************************************************************

class TKey {
public:
  bool generic;
  bool numPad;
  Qt::Key key;
  Qt::KeyboardModifier modifier;

  inline TKey():
    generic(),
    numPad(),
    key(),
    modifier()
    { }

  inline explicit TKey(Qt::Key key, bool numPad = false, bool generic = false):
    generic(generic),
    numPad(numPad),
    key(key),
    modifier()
    { }

  inline explicit TKey(Qt::KeyboardModifier modifier, bool generic = false):
    generic(generic),
    numPad(),
    key(),
    modifier(modifier)
    { }

  inline bool operator== (const TKey &other) const {
    if (isModifier())
      return modifier == other.modifier;
    if (generic || other.generic)
      return key == other.key;
    return key == other.key && numPad == other.numPad;
  }

  inline bool isModifier() const
    { return modifier != Qt::KeyboardModifier(); }

  inline bool isKey() const
    { return !isModifier(); }
};


//*****************************************************************************************
//    TInputState definition
//*****************************************************************************************

class TInputState {
public:
  typedef qint64 DeviceId;
  typedef long long TouchId;

  typedef TKey Key;
  typedef TKeyHistoryT<Key> KeyHistory;
  typedef KeyHistory::State KeyState;

  typedef Qt::MouseButton Button;
  typedef TKeyHistoryT<Button> ButtonHistory;
  typedef ButtonHistory::State ButtonState;
  typedef std::map<DeviceId, ButtonHistory::Pointer> ButtonHistoryMap;

private:
  TTimerTicks m_ticks;
  KeyHistory::Pointer m_keyHistory;
  mutable ButtonHistoryMap m_buttonHistories;

public:
  TInputState();
  ~TInputState();

  TTimerTicks ticks() const
    { return m_ticks; }
  void touch(TTimerTicks ticks);

  inline KeyHistory::Pointer keyHistory() const
    { return keyHistory(); }
  inline KeyState::Pointer keyState() const
    { return keyHistory()->current(); }

  inline void keyEvent(bool press, Key key, TTimerTicks ticks)
    { touch(ticks); keyHistory()->change(press, key, m_ticks); }
  inline void keyPress(Key key, TTimerTicks ticks)
    { keyEvent(true, key, ticks); }
  inline void keyRelease(Key key, TTimerTicks ticks)
    { keyEvent(false, key, ticks); }

  inline KeyState::Pointer keyFind(Key key) const
    { return keyState()->find(key); }
  inline bool isKeyPressed(Key key) const
    { return keyFind(key); }
  inline double howLongKeyPressed(Key key, TTimerTicks ticks, double timeOffset = 0.0)
    { return KeyState::Holder::howLongPressed(keyFind(key), ticks, timeOffset); }
  inline double howLongKeyPressed(Key key)
    { return howLongKeyPressed(key, m_ticks); }

  inline KeyState::Holder keyStateHolder(TTimerTicks ticks, double timeOffset = 0.0) const
    { return KeyState::Holder(keyState(), ticks, timeOffset); }
  inline KeyState::Holder keyStateHolder()
    { return keyStateHolder(m_ticks); }
  inline KeyHistory::Holder keyHistoryHolder(TTimerTicks ticks, double timeOffset = 0.0)
    { return KeyHistory::Holder(keyHistory(), ticks, timeOffset); }
  inline KeyHistory::Holder keyHistoryHolder()
    { return keyHistoryHolder(m_ticks); }

  ButtonHistory::Pointer buttonHistory(DeviceId deviceId) const;

  inline const ButtonHistoryMap buttonHistories() const
    { return m_buttonHistories; }
  inline ButtonState::Pointer buttonState(DeviceId deviceId) const
    { return buttonHistory(deviceId)->current(); }

  inline void buttonEvent(bool press, DeviceId deviceId, Button button, TTimerTicks ticks)
    { touch(ticks); buttonHistory(deviceId)->change(press, button, m_ticks); }
  inline void buttonPress(DeviceId deviceId, Button button, TTimerTicks ticks)
    { buttonEvent(true, deviceId, button, ticks); }
  inline void buttonRelease(DeviceId deviceId, Button button, TTimerTicks ticks)
    { buttonEvent(false, deviceId, button, ticks); }
  inline void buttonEvent(bool press, Button button, TTimerTicks ticks)
    { buttonEvent(press, 0, button, ticks); }
  inline void buttonPress(Button button, TTimerTicks ticks)
    { buttonEvent(true, button, ticks); }
  inline void buttonRelease(Button button, TTimerTicks ticks)
    { buttonEvent(false, button, ticks); }

  inline ButtonState::Pointer buttonFind(DeviceId deviceId, Button button)
    { return buttonState(deviceId)->find(button); }
  inline bool isButtonPressed(DeviceId deviceId, Button button)
    { return buttonFind(deviceId, button); }
  inline double howLongButtonPressed(DeviceId deviceId, Button button, TTimerTicks ticks, double timeOffset = 0.0)
    { return ButtonState::Holder::howLongPressed(buttonFind(deviceId, button), ticks, timeOffset); }
  inline double howLongButtonPressed(DeviceId deviceId, Button button)
    { return howLongButtonPressed(deviceId, button, m_ticks); }

  inline ButtonState::Pointer buttonFindDefault(Button button)
    { return buttonFind(DeviceId(), button); }
  inline bool isButtonPressedDefault(Button button)
    { return isButtonPressed(DeviceId(), button); }
  inline double howLongButtonPressedDefault(Button button, TTimerTicks ticks, double timeOffset = 0.0)
    { return howLongButtonPressed(DeviceId(), button, ticks, timeOffset); }
  inline double howLongButtonPressedDefault(Button button)
    { return howLongButtonPressedDefault(button, m_ticks); }

  ButtonState::Pointer buttonFindAny(Button button, DeviceId &outDevice);

  inline ButtonState::Pointer buttonFindAny(Button button)
    { DeviceId deviceId; return buttonFindAny(button, deviceId); }
  inline bool isButtonPressedAny(Button button)
    { return buttonFindAny(button); }
  inline double howLongButtonPressedAny(Button button, TTimerTicks ticks, double timeOffset = 0.0)
    { return ButtonState::Holder::howLongPressed(buttonFindAny(button), ticks, timeOffset); }
  inline double howLongButtonPressedAny(Button button)
    { return howLongButtonPressedAny(button, m_ticks); }

  inline ButtonState::Holder buttonStateHolder(DeviceId deviceId, TTimerTicks ticks, double timeOffset = 0.0)
    { return ButtonState::Holder(buttonState(deviceId), ticks, timeOffset); }
  inline ButtonState::Holder buttonStateHolder(DeviceId deviceId)
    { return buttonStateHolder(deviceId, m_ticks); }
  inline ButtonHistory::Holder buttonHistoryHolder(DeviceId deviceId, long ticks, double timeOffset = 0.0)
    { return ButtonHistory::Holder(buttonHistory(deviceId), ticks, timeOffset); }
  inline ButtonHistory::Holder buttonHistoryHolder(DeviceId deviceId)
    { return buttonHistoryHolder(deviceId, m_ticks); }
};


#endif
