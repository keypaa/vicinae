#pragma once
#include "abstract-snippet-server.hpp"
#include <set>
#include <string>
#include <windows.h>

class Win32SnippetServer : public AbstractSnippetServer {
  Q_OBJECT

public:
  using AbstractSnippetServer::AbstractSnippetServer;

  void registerSnippet(snippet_gen::CreateSnippetRequest payload) override;
  void unregisterSnippet(std::string_view keyword) override;
  void setKeymap(snippet_gen::LayoutInfo info) override;
  void resetContext() override;

  void injectExpand(const std::string &text, unsigned charsToDelete, unsigned prePasteDelayUs, bool terminal,
                    unsigned cursorLeftMoves) override;
  void injectUndo(unsigned backspaceCount, const std::string &trigger) override;
  void setKeyDelay(int us) override;
  bool supportsKeyInjection() const override { return true; }
  bool usesClipboard() const override { return true; }

  bool isRunning() const override { return true; }

private:
  void sendKeyPress(WORD vk) const;
  void sendKeyRelease(WORD vk) const;

  std::set<std::string> m_keywords;
  int m_keyDelayUs = 0;
};
