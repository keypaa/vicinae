#pragma once
#include "../abstract-audio-control.hpp"

class Win32AudioControl : public AbstractAudioControl {
public:
  Win32AudioControl();
  ~Win32AudioControl() override;

  QString id() const override;

  float getVolume() const override;
  std::optional<float> setVolume(float level) override;
  std::optional<float> adjustVolume(float delta) override;

  bool isMuted() const override;
  bool setMuted(bool muted) override;
  bool toggleMute() override;

  std::vector<AudioSink> listSinks() const override;
  bool setDefaultSink(const QString &sinkName) override;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
