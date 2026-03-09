#pragma once

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/instance.h>

#include "engine/laren_state.h"
#include "core/transliterator.h"
#include "core/emoji_map.h"
#include "dict/dictionary.h"

#include <memory>
#include <unordered_map>
#include <string>

namespace laren::engine {

class LarenEngine : public fcitx::InputMethodEngineV2 {
public:
    LarenEngine(fcitx::Instance* instance);

    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent& keyEvent) override;

    void activate(const fcitx::InputMethodEntry& entry,
                  fcitx::InputContextEvent& event) override;

    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent& event) override;

    void reset(const fcitx::InputMethodEntry& entry,
               fcitx::InputContextEvent& event) override;

    const core::Transliterator& transliterator() const { return transliterator_; }
    const core::EmojiMap& emojiMap() const { return emoji_map_; }
    fcitx::Instance* instance() const { return instance_; }

    auto* stateFactory() { return &factory_; }

    // History: remembers user's preferred candidate for each input
    std::optional<std::u32string> getHistory(const std::string& input) const;
    void setHistory(const std::string& input, const std::u32string& arabic);

private:
    fcitx::Instance* instance_;
    fcitx::FactoryFor<LarenState> factory_;
    std::shared_ptr<dict::Dictionary> dictionary_;
    core::Transliterator transliterator_;
    core::EmojiMap emoji_map_;

    void loadDictionary();
    void loadEmoji();
    void loadHistory();
    void saveHistory();

    std::unordered_map<std::string, std::u32string> history_;
    std::string history_path_;
};

class LarenEngineFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new LarenEngine(manager->instance());
    }
};

} // namespace laren::engine
