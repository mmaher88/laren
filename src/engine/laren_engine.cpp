#include "engine/laren_engine.h"

// fcitx5 >= 5.1.11 renamed StandardPath -> StandardPaths
#if __has_include(<fcitx-utils/standardpaths.h>)
#include <fcitx-utils/standardpaths.h>
#define LAREN_HAS_STANDARDPATHS 1
#else
#include <fcitx-utils/standardpath.h>
#define LAREN_HAS_STANDARDPATHS 0
#endif

#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>

#include <filesystem>
#include <fstream>
#include "util/unicode.h"

static void debugLog(const std::string& msg) {
    std::ofstream f("/tmp/laren-debug.log", std::ios::app);
    f << msg << std::endl;
}

namespace laren::engine {

LarenEngine::LarenEngine(fcitx::Instance* instance)
    : instance_(instance),
      factory_([this](fcitx::InputContext& ic) -> LarenState* {
          return new LarenState(this, &ic);
      }) {

    loadDictionary();
    loadEmoji();
    loadHistory();

    instance->inputContextManager().registerProperty("larenState", &factory_);
    debugLog("LarenEngine constructed, dict size=" + std::to_string(dictionary_->size()));
}

void LarenEngine::loadDictionary() {
    dictionary_ = std::make_shared<dict::Dictionary>();

    std::string dict_path;

#if LAREN_HAS_STANDARDPATHS
    // fcitx5 >= 5.1.11: StandardPaths (plural) API
    auto& paths = fcitx::StandardPaths::global();
    auto path = paths.locate(fcitx::StandardPathsType::Data,
                             "laren/dictionary.tsv");
    if (path.empty()) {
        path = paths.locate(fcitx::StandardPathsType::PkgData,
                            "laren/dictionary.tsv");
    }
    dict_path = path.empty() ? "/usr/share/laren/dictionary.tsv" : path.string();

    auto user_path = paths.locate(fcitx::StandardPathsType::Data,
                                  "laren/user_dictionary.tsv");
    std::string user_dict_path = user_path.empty() ? "" : user_path.string();
#else
    // fcitx5 < 5.1.11: StandardPath (singular) API
    auto& paths = fcitx::StandardPath::global();
    dict_path = paths.locate(fcitx::StandardPath::Type::Data,
                             "laren/dictionary.tsv");
    if (dict_path.empty()) {
        dict_path = paths.locate(fcitx::StandardPath::Type::PkgData,
                                 "laren/dictionary.tsv");
    }
    if (dict_path.empty()) {
        dict_path = "/usr/share/laren/dictionary.tsv";
    }

    auto user_dict_path = paths.locate(fcitx::StandardPath::Type::Data,
                                       "laren/user_dictionary.tsv");
#endif

    debugLog("Dictionary path: " + dict_path);
    dictionary_->load_tsv(dict_path);

    if (!user_dict_path.empty()) {
        dictionary_->load_tsv(user_dict_path);
    }

    core::Transliterator::Config config;
    config.max_candidates = 50;
    config.max_expansions = 4096;
    config.use_dictionary = (dictionary_->size() > 0);
    transliterator_ = core::Transliterator(config);
    transliterator_.set_dictionary(dictionary_);
}

void LarenEngine::keyEvent(const fcitx::InputMethodEntry& /*entry*/,
                           fcitx::KeyEvent& keyEvent) {
    debugLog("keyEvent called, sym=0x" + std::to_string(keyEvent.key().sym()));
    auto* ic = keyEvent.inputContext();
    auto* state = ic->propertyFor(&factory_);

    if (state->processKey(keyEvent)) {
        keyEvent.filterAndAccept();
    }
}

void LarenEngine::activate(const fcitx::InputMethodEntry& /*entry*/,
                           fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    auto* state = ic->propertyFor(&factory_);
    state->reset();
}

void LarenEngine::deactivate(const fcitx::InputMethodEntry& /*entry*/,
                             fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    auto* state = ic->propertyFor(&factory_);
    state->reset();
    saveHistory();
}

void LarenEngine::reset(const fcitx::InputMethodEntry& /*entry*/,
                        fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    auto* state = ic->propertyFor(&factory_);
    state->reset();
}

void LarenEngine::loadEmoji() {
    std::string emoji_path;

#if LAREN_HAS_STANDARDPATHS
    auto& paths = fcitx::StandardPaths::global();
    auto path = paths.locate(fcitx::StandardPathsType::Data,
                             "laren/emoji.tsv");
    if (path.empty()) {
        path = paths.locate(fcitx::StandardPathsType::PkgData,
                            "laren/emoji.tsv");
    }
    emoji_path = path.empty() ? "/usr/share/laren/emoji.tsv" : path.string();
#else
    auto& paths = fcitx::StandardPath::global();
    emoji_path = paths.locate(fcitx::StandardPath::Type::Data,
                              "laren/emoji.tsv");
    if (emoji_path.empty()) {
        emoji_path = paths.locate(fcitx::StandardPath::Type::PkgData,
                                  "laren/emoji.tsv");
    }
    if (emoji_path.empty()) {
        emoji_path = "/usr/share/laren/emoji.tsv";
    }
#endif

    debugLog("Emoji path: " + emoji_path);
    emoji_map_.load_tsv(emoji_path);
    debugLog("Emoji loaded, entries=" + std::to_string(emoji_map_.size()));
}

void LarenEngine::loadHistory() {
    const char* home = std::getenv("HOME");
    if (!home) return;
    history_path_ = std::string(home) + "/.local/share/laren/history.tsv";
    history_.load(history_path_);
    debugLog("History loaded, keys=" + std::to_string(history_.size()));
}

void LarenEngine::saveHistory() {
    if (history_path_.empty() || !history_.dirty()) return;
    history_.save(history_path_);
}

} // namespace laren::engine

FCITX_ADDON_FACTORY(laren::engine::LarenEngineFactory);
