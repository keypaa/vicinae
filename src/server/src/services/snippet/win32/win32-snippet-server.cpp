#include "win32-snippet-server.hpp"
#include <thread>

void Win32SnippetServer::registerSnippet(snippet_gen::CreateSnippetRequest payload) {
  m_keywords.insert(std::move(payload.keyword));
}

void Win32SnippetServer::unregisterSnippet(std::string_view keyword) {
  m_keywords.erase(std::string(keyword));
}

void Win32SnippetServer::setKeymap(snippet_gen::LayoutInfo) {}

void Win32SnippetServer::resetContext() {}

void Win32SnippetServer::sendKeyPress(WORD vk) const {
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  SendInput(1, &input, sizeof(INPUT));
}

void Win32SnippetServer::sendKeyRelease(WORD vk) const {
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(INPUT));
}

void Win32SnippetServer::injectExpand(const std::string &, unsigned charsToDelete, unsigned prePasteDelayUs,
                                      bool, unsigned cursorLeftMoves) {
  if (prePasteDelayUs > 0) { std::this_thread::sleep_for(std::chrono::microseconds(prePasteDelayUs)); }

  for (unsigned i = 0; i < charsToDelete; ++i) {
    sendKeyPress(VK_BACK);
    sendKeyRelease(VK_BACK);
    if (m_keyDelayUs > 0) std::this_thread::sleep_for(std::chrono::microseconds(m_keyDelayUs));
  }

  sendKeyPress(VK_CONTROL);
  sendKeyPress('V');
  sendKeyRelease('V');
  sendKeyRelease(VK_CONTROL);
  if (m_keyDelayUs > 0) std::this_thread::sleep_for(std::chrono::microseconds(m_keyDelayUs));

  for (unsigned i = 0; i < cursorLeftMoves; ++i) {
    sendKeyPress(VK_LEFT);
    sendKeyRelease(VK_LEFT);
    if (m_keyDelayUs > 0) std::this_thread::sleep_for(std::chrono::microseconds(m_keyDelayUs));
  }
}

void Win32SnippetServer::injectUndo(unsigned backspaceCount, const std::string &) {
  for (unsigned i = 0; i < backspaceCount; ++i) {
    sendKeyPress(VK_BACK);
    sendKeyRelease(VK_BACK);
    if (m_keyDelayUs > 0) std::this_thread::sleep_for(std::chrono::microseconds(m_keyDelayUs));
  }
}

void Win32SnippetServer::setKeyDelay(int us) { m_keyDelayUs = us; }
