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

// Candidate that commits an emoji
class EmojiCandidateWord : public fcitx::CandidateWord {
public:
    EmojiCandidateWord(LarenState* state, const std::string& emoji,
                       const std::string& shortcode, size_t index)
        : state_(state), index_(index) {
        setText(fcitx::Text(emoji + "  :" + shortcode + ":"));
    }

    void select(fcitx::InputContext* /*ic*/) const override {
        state_->commitEmoji(index_);
    }

private:
    LarenState* state_;
    size_t index_;
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

    // Colon toggles emoji mode
    if (key.isSimple() && key.sym() == FcitxKey_colon) {
        if (emoji_mode_) {
            // Second colon: if we have an exact match, commit it
            if (!emoji_candidates_.empty()) {
                commitEmoji(0);
                return true;
            }
            // No match — exit emoji mode, commit the raw text with colons
            std::string raw = ":" + buffer_;
            reset();
            ic_->commitString(raw + ":");
            return true;
        }
        // Enter emoji mode (only if buffer is empty — no mixing with Arabizi)
        if (buffer_.empty()) {
            emoji_mode_ = true;
            buffer_.clear();
            emoji_candidates_.clear();
            cursor_ = 0;
            updateEmojiUI();
            return true;
        }
        return false;
    }

    // === Emoji mode input handling ===
    if (emoji_mode_) {
        if (key.isSimple()) {
            auto ch = key.sym();
            // Accept letters, digits, underscore, hyphen, plus for shortcodes
            bool valid = (ch >= FcitxKey_a && ch <= FcitxKey_z) ||
                         (ch >= FcitxKey_A && ch <= FcitxKey_Z) ||
                         (ch >= FcitxKey_0 && ch <= FcitxKey_9) ||
                         ch == FcitxKey_underscore || ch == FcitxKey_minus ||
                         ch == FcitxKey_plus;
            if (valid) {
                buffer_ += static_cast<char>(tolower(static_cast<int>(ch)));
                cursor_ = 0;
                emoji_candidates_ = engine_->emojiMap().find_by_prefix(buffer_, 50);
                updateEmojiUI();
                return true;
            }
        }

        // Space or Enter: commit selected emoji
        if (key.sym() == FcitxKey_space || key.sym() == FcitxKey_Return) {
            if (!emoji_candidates_.empty()) {
                size_t idx = static_cast<size_t>(cursor_);
                if (idx < emoji_candidates_.size()) {
                    commitEmoji(idx);
                } else {
                    commitEmoji(0);
                }
            } else {
                // No matches — commit raw text
                std::string raw = ":" + buffer_;
                reset();
                ic_->commitString(raw + " ");
            }
            return true;
        }

        // Number keys 1-9: select specific emoji
        if (key.sym() >= FcitxKey_1 && key.sym() <= FcitxKey_9) {
            size_t idx = key.sym() - FcitxKey_1;
            if (idx < emoji_candidates_.size()) {
                commitEmoji(idx);
            }
            return true;
        }

        // Arrow navigation
        if (key.sym() == FcitxKey_Down && !emoji_candidates_.empty()) {
            cursor_ = (cursor_ + 1) % static_cast<int>(emoji_candidates_.size());
            updateEmojiUI();
            syncPageToCursor();
            return true;
        }
        if (key.sym() == FcitxKey_Up && !emoji_candidates_.empty()) {
            int n = static_cast<int>(emoji_candidates_.size());
            cursor_ = (cursor_ - 1 + n) % n;
            updateEmojiUI();
            syncPageToCursor();
            return true;
        }

        // Page navigation
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

        // Left/Right: consume
        if (key.sym() == FcitxKey_Left || key.sym() == FcitxKey_Right) {
            return true;
        }

        // Backspace
        if (key.sym() == FcitxKey_BackSpace) {
            if (buffer_.empty()) {
                reset(); // exit emoji mode
            } else {
                buffer_.pop_back();
                cursor_ = 0;
                emoji_candidates_ = engine_->emojiMap().find_by_prefix(buffer_, 50);
                updateEmojiUI();
            }
            return true;
        }

        // Escape: cancel emoji mode
        if (key.sym() == FcitxKey_Escape) {
            reset();
            return true;
        }

        return true; // consume all other keys in emoji mode
    }

    // === Normal Arabizi mode ===

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

    // Arabic punctuation pass-through (works with empty or non-empty buffer)
    if (key.sym() == FcitxKey_question) {
        if (!buffer_.empty()) {
            commitSelected(); // commit current candidate first
        }
        ic_->commitString("؟");
        return true;
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

void LarenState::commitEmoji(size_t index) {
    if (index < emoji_candidates_.size()) {
        auto& emoji = emoji_candidates_[index]->emoji;
        debugLog("commitEmoji: " + emoji);
        emoji_mode_ = false;
        emoji_candidates_.clear();
        buffer_.clear();
        cursor_ = 0;
        auto& panel = ic_->inputPanel();
        panel.reset();
        ic_->commitString(emoji);
        ic_->updatePreedit();
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
}

void LarenState::updateEmojiUI() {
    auto& panel = ic_->inputPanel();
    panel.reset();

    // Show ":shortcode" as preedit
    std::string preedit_str = ":" + buffer_;
    fcitx::Text preedit;
    preedit.append(preedit_str, fcitx::TextFormatFlag::Underline);
    if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        panel.setClientPreedit(preedit);
    } else {
        panel.setPreedit(preedit);
    }

    panel.setAuxUp(fcitx::Text(preedit_str));

    if (!emoji_candidates_.empty()) {
        auto candidateList = std::make_unique<LarenCandidateList>();
        candidateList->setPageSize(9);
        candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);

        for (size_t i = 0; i < emoji_candidates_.size(); ++i) {
            candidateList->append<EmojiCandidateWord>(
                this, emoji_candidates_[i]->emoji,
                emoji_candidates_[i]->shortcode, i);
        }

        candidateList->setSelectionKey(fcitx::Key::keyListFromString(
            "1 2 3 4 5 6 7 8 9"));
        candidateList->setGlobalCursorIndex(cursor_);
        panel.setCandidateList(std::move(candidateList));
    }

    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
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
    emoji_mode_ = false;
    emoji_candidates_.clear();
    cursor_ = 0;
    auto& panel = ic_->inputPanel();
    panel.reset();
    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

} // namespace laren::engine
