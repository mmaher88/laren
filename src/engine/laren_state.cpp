#include "engine/laren_state.h"
#include "engine/laren_engine.h"
#include "util/unicode.h"

#include <fcitx/inputpanel.h>
#include <fcitx/candidatelist.h>
#include <fcitx-utils/key.h>

#include <fstream>
static void debugLog(const std::string& msg) {
    std::ofstream f("/tmp/laren-debug.log", std::ios::app);
    f << msg << std::endl;
}

namespace laren::engine {

// Candidate that commits Arabic text
class LarenCandidateWord : public fcitx::CandidateWord {
public:
    LarenCandidateWord(LarenState* state, const std::string& text,
                       size_t index, bool is_raw = false, bool is_history = false)
        : state_(state), index_(index), is_raw_(is_raw), is_history_(is_history) {
        setText(fcitx::Text(text));
    }

    void select(fcitx::InputContext* /*ic*/) const override {
        if (is_raw_) {
            state_->commitRaw();
        } else if (is_history_) {
            state_->commitHistory();
        } else {
            state_->commitCandidate(index_);
        }
    }

private:
    LarenState* state_;
    size_t index_;
    bool is_raw_;
    bool is_history_;
};

// Non-selectable separator
class LarenSeparator : public fcitx::CandidateWord {
public:
    LarenSeparator() {
        setText(fcitx::Text("──────────"));
    }
    void select(fcitx::InputContext* /*ic*/) const override {}
};

// Candidate list with continuous numbering across pages
class LarenCandidateList : public fcitx::CommonCandidateList {
public:
    const fcitx::Text& label(int idx) const override {
        int page = currentPage();
        int global = page * pageSize() + idx + 1; // 1-based
        auto it = label_cache_.find(global);
        if (it == label_cache_.end()) {
            it = label_cache_.emplace(global, fcitx::Text(std::to_string(global) + ". ")).first;
        }
        return it->second;
    }

private:
    mutable std::unordered_map<int, fcitx::Text> label_cache_;
};

LarenState::LarenState(LarenEngine* engine, fcitx::InputContext* ic)
    : engine_(engine), ic_(ic) {}

bool LarenState::processKey(fcitx::KeyEvent& event) {
    auto key = event.key();

    // Only handle key press, not release
    if (event.isRelease()) {
        return false;
    }

    // Handle letter, digit, and apostrophe input
    if (key.isSimple()) {
        auto ch = key.sym();

        bool is_valid_input = false;

        // Letters a-z, A-Z
        if ((ch >= FcitxKey_a && ch <= FcitxKey_z) ||
            (ch >= FcitxKey_A && ch <= FcitxKey_Z)) {
            is_valid_input = true;
        }
        // Digits 2-9 (used in Arabizi)
        else if (ch >= FcitxKey_2 && ch <= FcitxKey_9) {
            is_valid_input = true;
        }
        // Apostrophe (used for modified letters like 3', 7')
        else if (ch == FcitxKey_apostrophe) {
            is_valid_input = true;
        }

        if (is_valid_input) {
            buffer_ += static_cast<char>(ch);
            cursor_ = 0;
            updateCandidates();
            updateUI();
            return true;
        }
    }

    // Only process special keys if we have content in the buffer
    if (buffer_.empty()) {
        return false;
    }

    debugLog("key sym=0x" + std::to_string(key.sym()) + " buffer=" + buffer_);

    // Total visible items in the list
    int total_items = totalListItems();

    // Arrow Down: move selection down (skip separator, auto-advance page)
    if (key.sym() == FcitxKey_Down) {
        do {
            cursor_ = (cursor_ + 1) % total_items;
        } while (isSeparatorIndex(cursor_));
        updateUI();
        // Auto-advance page if cursor moved past current page
        syncPageToCursor();
        return true;
    }

    // Arrow Up: move selection up (skip separator, auto-advance page)
    if (key.sym() == FcitxKey_Up) {
        do {
            cursor_ = (cursor_ - 1 + total_items) % total_items;
        } while (isSeparatorIndex(cursor_));
        updateUI();
        syncPageToCursor();
        return true;
    }

    // Left/Right arrows: consume to prevent stray input
    if (key.sym() == FcitxKey_Left || key.sym() == FcitxKey_Right) {
        return true;
    }

    // Page Down / Page Up: navigate candidate pages
    if (key.sym() == FcitxKey_Page_Down || key.sym() == FcitxKey_Next) {
        auto cl = ic_->inputPanel().candidateList();
        if (cl) {
            auto* pageable = cl->toPageable();
            if (pageable && pageable->hasNext()) {
                pageable->next();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
        }
        return true;
    }

    if (key.sym() == FcitxKey_Page_Up || key.sym() == FcitxKey_Prior) {
        auto cl = ic_->inputPanel().candidateList();
        if (cl) {
            auto* pageable = cl->toPageable();
            if (pageable && pageable->hasPrev()) {
                pageable->prev();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
        }
        return true;
    }

    // Space or Enter: commit currently selected candidate
    if (key.sym() == FcitxKey_space || key.sym() == FcitxKey_Return) {
        commitSelected();
        return true;
    }

    // Number keys 1-9: select specific candidate on current page
    if (key.sym() >= FcitxKey_1 && key.sym() <= FcitxKey_9) {
        size_t idx = key.sym() - FcitxKey_1;
        commitByDisplayIndex(idx);
        return true;
    }


    // Backspace: remove last character
    if (key.sym() == FcitxKey_BackSpace) {
        buffer_.pop_back();
        if (buffer_.empty()) {
            reset();
        } else {
            cursor_ = 0;
            updateCandidates();
            updateUI();
        }
        return true;
    }

    // Escape: clear buffer
    if (key.sym() == FcitxKey_Escape) {
        reset();
        return true;
    }

    return false;
}

void LarenState::updateCandidates() {
    cached_candidates_ = engine_->transliterator().transliterate(buffer_);
    history_candidate_ = engine_->getHistory(buffer_);
    has_history_ = history_candidate_.has_value();
    debugLog("updateCandidates: buffer=" + buffer_ + " has_history=" + std::to_string(has_history_) +
             " candidates=" + std::to_string(cached_candidates_.size()));

    // Remove history candidate from regular list to avoid duplicate
    if (has_history_) {
        auto it = std::remove_if(cached_candidates_.begin(), cached_candidates_.end(),
            [this](const core::Candidate& c) {
                return c.arabic == *history_candidate_;
            });
        cached_candidates_.erase(it, cached_candidates_.end());
    }
}

int LarenState::totalListItems() const {
    int count = static_cast<int>(cached_candidates_.size()) + 1; // +1 for raw
    if (has_history_) {
        count += 2; // history item + separator
    }
    return count;
}

bool LarenState::isSeparatorIndex(int idx) const {
    return has_history_ && idx == 1; // separator is always at index 1 when history exists
}

void LarenState::updateUI() {
    auto& panel = ic_->inputPanel();
    panel.reset();

    if (buffer_.empty()) {
        ic_->updatePreedit();
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }

    // Show the Arabizi input as preedit (underlined in the text field)
    fcitx::Text preedit;
    preedit.append(buffer_, fcitx::TextFormatFlag::Underline);
    if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        panel.setClientPreedit(preedit);
    } else {
        panel.setPreedit(preedit);
    }

    // Show the Arabizi input as header above the candidate list
    panel.setAuxUp(fcitx::Text(buffer_));

    // Build vertical candidate list
    auto candidateList = std::make_unique<LarenCandidateList>();
    candidateList->setPageSize(9);
    candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);

    // History candidate first (if any)
    if (has_history_) {
        auto utf8 = util::utf32_to_utf8(*history_candidate_);
        candidateList->append<LarenCandidateWord>(this, utf8, 0, false, true);

        // Separator
        candidateList->append<LarenSeparator>();
    }

    // Regular Arabic candidates
    for (size_t i = 0; i < cached_candidates_.size(); ++i) {
        auto utf8 = util::utf32_to_utf8(cached_candidates_[i].arabic);
        candidateList->append<LarenCandidateWord>(this, utf8, i);
    }

    // Raw Arabizi as last fallback
    candidateList->append<LarenCandidateWord>(this, buffer_, 0, true);

    candidateList->setSelectionKey(fcitx::Key::keyListFromString(
        "1 2 3 4 5 6 7 8 9"));

    // Highlight the currently selected candidate
    candidateList->setGlobalCursorIndex(cursor_);

    panel.setCandidateList(std::move(candidateList));

    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LarenState::syncPageToCursor() {
    auto cl = ic_->inputPanel().candidateList();
    if (!cl) return;
    auto* pageable = cl->toPageable();
    if (!pageable) return;

    // Figure out which page the cursor should be on
    int page_size = cl->size(); // items on current page
    if (page_size <= 0) return;

    int target_page = cursor_ / page_size;
    int current_page = pageable->currentPage();

    while (current_page < target_page && pageable->hasNext()) {
        pageable->next();
        current_page++;
    }
    while (current_page > target_page && pageable->hasPrev()) {
        pageable->prev();
        current_page--;
    }

    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LarenState::commitSelected() {
    debugLog("commitSelected: cursor=" + std::to_string(cursor_) + " has_history=" + std::to_string(has_history_));
    if (isSeparatorIndex(cursor_)) {
        return; // can't select separator
    }

    if (has_history_ && cursor_ == 0) {
        commitHistory();
        return;
    }

    // Map cursor position to candidate index
    int offset = has_history_ ? 2 : 0; // skip history + separator
    int adjusted = cursor_ - offset;
    int raw_index = static_cast<int>(cached_candidates_.size());

    debugLog("commitSelected: offset=" + std::to_string(offset) + " adjusted=" + std::to_string(adjusted) + " raw_index=" + std::to_string(raw_index));

    if (adjusted >= raw_index || cached_candidates_.empty()) {
        commitRaw();
    } else if (adjusted >= 0) {
        commitCandidate(static_cast<size_t>(adjusted));
    }
}

void LarenState::commitByDisplayIndex(size_t display_idx) {
    // Map 1-9 key press (display_idx 0-8) to actual list item
    int list_idx = static_cast<int>(display_idx);

    if (has_history_) {
        if (list_idx == 0) {
            commitHistory();
            return;
        }
        if (list_idx == 1) {
            return; // separator
        }
        // Adjust for history + separator
        size_t candidate_idx = list_idx - 2;
        if (candidate_idx < cached_candidates_.size()) {
            commitCandidate(candidate_idx);
        } else if (candidate_idx == cached_candidates_.size()) {
            commitRaw();
        }
    } else {
        if (display_idx < cached_candidates_.size()) {
            commitCandidate(display_idx);
        } else if (display_idx == cached_candidates_.size()) {
            commitRaw();
        }
    }
}

void LarenState::commitCandidate(size_t index) {
    if (index < cached_candidates_.size()) {
        auto& arabic = cached_candidates_[index].arabic;
        auto utf8 = util::utf32_to_utf8(arabic);
        debugLog("commitCandidate: index=" + std::to_string(index) + " text=" + utf8 + " buffer=" + buffer_);
        engine_->setHistory(buffer_, arabic);
        commitText(utf8);
    } else {
        debugLog("commitCandidate: index=" + std::to_string(index) + " OUT OF RANGE (size=" + std::to_string(cached_candidates_.size()) + ")");
    }
}

void LarenState::commitHistory() {
    if (history_candidate_) {
        auto utf8 = util::utf32_to_utf8(*history_candidate_);
        // No need to update history — it's already the preferred choice
        commitText(utf8);
    }
}

void LarenState::commitRaw() {
    commitText(buffer_);
}

void LarenState::commitText(const std::string& text) {
    ic_->commitString(text + " ");
    buffer_.clear();
    cached_candidates_.clear();
    history_candidate_.reset();
    has_history_ = false;
    cursor_ = 0;
    auto& panel = ic_->inputPanel();
    panel.reset();
    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LarenState::reset() {
    buffer_.clear();
    cached_candidates_.clear();
    history_candidate_.reset();
    has_history_ = false;
    cursor_ = 0;
    auto& panel = ic_->inputPanel();
    panel.reset();
    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

} // namespace laren::engine
