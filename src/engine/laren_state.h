#pragma once

#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/candidatelist.h>
#include "core/candidate.h"
#include "core/emoji_map.h"
#include <string>
#include <vector>

namespace laren::engine {

class LarenEngine;

class LarenState : public fcitx::InputContextProperty {
public:
    LarenState(LarenEngine* engine, fcitx::InputContext* ic);

    bool processKey(fcitx::KeyEvent& event);
    void reset();

    // Public for LarenCandidateWord access
    void commitCandidate(size_t index);
    void commitRaw();
    void commitEmoji(size_t index);

private:
    LarenEngine* engine_;
    fcitx::InputContext* ic_;
    std::string buffer_;
    std::vector<laren::core::Candidate> cached_candidates_;
    int cursor_ = 0;  // Currently highlighted candidate index

    // Emoji mode
    bool emoji_mode_ = false;
    std::vector<const core::EmojiEntry*> emoji_candidates_;

    void updateCandidates();
    void updateUI();
    void updateEmojiUI();
    void commitSelected();
    void commitByDisplayIndex(size_t display_idx);
    void commitText(const std::string& text);
    int totalListItems() const;
    void syncPageToCursor();
};

} // namespace laren::engine
