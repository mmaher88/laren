#pragma once

#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/candidatelist.h>
#include "core/candidate.h"
#include <optional>
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
    void commitHistory();

private:
    LarenEngine* engine_;
    fcitx::InputContext* ic_;
    std::string buffer_;
    std::vector<laren::core::Candidate> cached_candidates_;
    std::optional<std::u32string> history_candidate_;
    bool has_history_ = false;
    int cursor_ = 0;  // Currently highlighted candidate index

    void updateCandidates();
    void updateUI();
    void commitSelected();
    void commitByDisplayIndex(size_t display_idx);
    void commitText(const std::string& text);
    int totalListItems() const;
    bool isSeparatorIndex(int idx) const;
    void syncPageToCursor();
};

} // namespace laren::engine
