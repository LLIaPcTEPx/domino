#include <Geode/Geode.hpp>
#include <Geode/ui/SceneEvent.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/layers_scenes_transitions_nodes/CCLayer.h>
#include <Geode/cocos/layers_scenes_transitions_nodes/CCScene.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/binding/LevelManagerDelegate.hpp>

#include <algorithm>
#include <cctype>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    struct Query {
        std::set<int> versions;
        std::set<int> stars;
        int coinMode = -1; // -1 = any, 0 = no coins, 1 = has coins
        bool filterVersion = false;
        bool filterStars = false;
    };

    std::string trim(std::string value) {
        auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::vector<std::string> split(std::string const& text, char delimiter) {
        std::vector<std::string> parts;
        std::stringstream stream(text);
        std::string part;
        while (std::getline(stream, part, delimiter)) parts.push_back(part);
        return parts;
    }

    bool parseStarList(std::string const& text, std::set<int>& out) {
        for (auto const& part : split(text, '-')) {
            try {
                size_t consumed = 0;
                int value = std::stoi(part, &consumed);
                if (consumed != part.size() || value > 14) return false;
                // A value below one means "don't apply a star/rating filter".
                if (value >= 1) out.insert(value);
            } catch (...) { return false; }
        }
        return true;
    }

    bool parseVersionList(std::string const& text, std::set<int>& out) {
        for (auto const& part : split(text, '-')) {
            auto dot = part.find('.');
            if (dot == std::string::npos || dot == 0 || dot + 1 >= part.size() || part.find('.', dot + 1) != std::string::npos) return false;
            try {
                size_t majorEnd = 0, minorEnd = 0;
                int major = std::stoi(part.substr(0, dot), &majorEnd);
                int minor = std::stoi(part.substr(dot + 1), &minorEnd);
                if (majorEnd != dot || minorEnd != part.size() - dot - 1 || major < 0 || minor < 0 || minor > 9) return false;
                // The game stores this editor-version value as a compact integer (2.2 = 22).
                out.insert(major * 10 + minor);
            } catch (...) { return false; }
        }
        return !out.empty();
    }

    bool parseQuery(std::vector<std::string> const& args, Query& query, bool official, std::string& error) {
        if (official) {
            if (args.size() > 1) {
                error = "Usage: randomofficial (stars)";
                return false;
            }
            if (!args.empty() && args[0] != "*") {
                if (!parseStarList(args[0], query.stars)) {
                    error = "Stars must be *, 0 (no star filter), or a dash-separated list up to 14.";
                    return false;
                }
                query.filterStars = !query.stars.empty();
            }
            return true;
        }

        if (args.size() > 3) {
            error = "Usage: random (version) {coins} (stars)";
            return false;
        }
        if (!args.empty() && args[0] != "*") {
            if (!parseVersionList(args[0], query.versions)) {
                error = "Version must look like 2.2 or a dash-separated list; use * for any.";
                return false;
            }
            query.filterVersion = true;
        }
        if (args.size() >= 2 && args[1] != "*") {
            auto coin = args[1];
            if (coin.size() < 3 || coin.front() != '{' || coin.back() != '}') {
                error = "Coins must be written as {true}, {false}, or *.";
                return false;
            }
            auto value = coin.substr(1, coin.size() - 2);
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (value == "true") query.coinMode = 1;
            else if (value == "false") query.coinMode = 0;
            else {
                error = "Coins must be {true}, {false}, or *.";
                return false;
            }
        }
        if (args.size() >= 3 && args[2] != "*") {
            if (!parseStarList(args[2], query.stars)) {
                error = "Stars must be *, 0 (no star filter), or a dash-separated list up to 14.";
                return false;
            }
            query.filterStars = !query.stars.empty();
        }
        return true;
    }

    int demonRank(GJGameLevel* level) {
        switch (level->m_demonDifficulty) {
            case 3: return 10; // Easy
            case 4: return 11; // Medium
            case 0: return 12; // Hard
            case 5: return 13; // Insane
            case 6: return 14; // Extreme
            default: return 10;
        }
    }

    bool matches(GJGameLevel* level, Query const& query) {
        if (!level) return false;
        if (query.coinMode >= 0 && ((level->m_coins > 0) != (query.coinMode == 1))) return false;
        if (query.filterVersion && !query.versions.contains(level->m_gameVersion)) return false;
        if (query.filterStars) {
            if (query.stars.empty()) return false;
            int value = level->m_demon.value() ? demonRank(level) : level->m_stars.value();
            if (!query.stars.contains(value)) return false;
        }
        return !level->m_levelString.empty();
    }

    int randomIndex(int upperBound) {
        static std::mt19937 engine(std::random_device{}());
        return std::uniform_int_distribution<int>(0, upperBound - 1)(engine);
    }

    std::vector<std::string> tokenize(std::string const& command) {
        std::istringstream input(command);
        std::vector<std::string> result;
        std::string word;
        while (input >> word) result.push_back(word);
        return result;
    }

    class ConsoleLayer final : public cocos2d::CCLayerColor, public LevelManagerDelegate {
    public:
        inline static bool s_consoleVisible = true;

        static ConsoleLayer* create() {
            auto ret = new ConsoleLayer();
            if (ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        bool init() override {
            auto size = CCDirector::sharedDirector()->getWinSize();
            if (!CCLayerColor::initWithColor({9, 14, 24, 225})) return false;
            setContentSize(size);
            setKeypadEnabled(true);

            auto panel = cocos2d::CCLayerColor::create({15, 25, 39, 245}, std::min(size.width - 50.f, 560.f), 205.f);
            panel->setPosition((size.width - panel->getContentSize().width) / 2.f,
                               (size.height - panel->getContentSize().height) / 2.f);
            addChild(panel);

            auto title = cocos2d::CCLabelBMFont::create("RANDOM CONSOLE  |  F5: hide", "bigFont.fnt");
            title->setScale(.62f);
            title->setAnchorPoint({0.f, .5f});
            title->setPosition(18.f, 178.f);
            panel->addChild(title);

            m_output = cocos2d::CCLabelBMFont::create("Commands: help, random, randomofficial", "bigFont.fnt");
            m_output->setAnchorPoint({0.f, 1.f});
            m_output->setScale(.42f);
            m_output->setPosition(18.f, 145.f);
            panel->addChild(m_output);

            m_input = TextInput::create(std::min(size.width - 180.f, 390.f), "Type a command...", "bigFont.fnt");
            m_input->setPosition(18.f + m_input->getContentWidth() / 2.f, 43.f);
            m_input->setCommonFilter(CommonFilter::Text);
            panel->addChild(m_input);

            auto button = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Run", 70, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f),
                this, menu_selector(ConsoleLayer::onRun)
            );
            auto menu = cocos2d::CCMenu::create();
            menu->addChild(button);
            menu->setPosition(m_input->getPositionX() + m_input->getContentWidth() / 2.f + 43.f, 43.f);
            panel->addChild(menu);

            addEventListener(KeyboardInputEvent(), [this](KeyboardInputData& data) {
                if (data.key == KEY_F5 && data.action == KeyboardInputData::Action::Press) {
                    this->setVisible(!this->isVisible());
                    s_consoleVisible = this->isVisible();
                    if (!s_consoleVisible && m_input) m_input->detachWithIME();
                    return geode::ListenerResult::Stop;
                }
                if (data.key == KEY_Enter && data.action == KeyboardInputData::Action::Press && this->isVisible()) {
                    this->runCommand();
                    return geode::ListenerResult::Stop;
                }
                return geode::ListenerResult::Propagate;
            });

            m_previousDelegate = GameLevelManager::sharedState()->m_levelManagerDelegate;
            showMessage("Ready. Type help for commands.");
            return true;
        }

        ~ConsoleLayer() override {
            auto manager = GameLevelManager::sharedState();
            if (manager && manager->m_levelManagerDelegate == this) manager->m_levelManagerDelegate = m_previousDelegate;
        }

        void loadLevelsFinished(cocos2d::CCArray* levels, char const*, int) override {
            if (!m_waiting) return;
            m_waiting = false;
            auto manager = GameLevelManager::sharedState();
            if (manager->m_levelManagerDelegate == this) manager->m_levelManagerDelegate = m_previousDelegate;
            std::vector<GJGameLevel*> candidates;
            if (levels) {
                for (unsigned i = 0; i < levels->count(); ++i) {
                    auto level = typeinfo_cast<GJGameLevel*>(levels->objectAtIndex(i));
                    if (matches(level, m_query)) candidates.push_back(level);
                }
            }
            if (candidates.empty()) {
                if (++m_attempt < 6) requestOnlinePage();
                else showMessage("No matching levels found. Try fewer filters or run the command again.");
                return;
            }
            openLevel(candidates[static_cast<size_t>(randomIndex(static_cast<int>(candidates.size())))]);
        }

        void loadLevelsFailed(char const*, int) override {
            if (!m_waiting) return;
            m_waiting = false;
            auto manager = GameLevelManager::sharedState();
            if (manager->m_levelManagerDelegate == this) manager->m_levelManagerDelegate = m_previousDelegate;
            if (++m_attempt < 6) requestOnlinePage();
            else showMessage("Level search failed. Check your internet connection and try again.");
        }

    private:
        void showMessage(std::string const& message) {
            m_output->setString(message.c_str());
        }

        void onRun(cocos2d::CCObject*) { runCommand(); }

        void runCommand() {
            if (!m_input) return;
            auto raw = trim(m_input->getString());
            if (raw.empty()) return;
            auto words = tokenize(raw);
            m_input->setString("");
            std::transform(words[0].begin(), words[0].end(), words[0].begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (words[0] == "help") {
                showMessage("help: show commands\nrandom (version) {coins} (stars)\nrandomofficial (stars)\nUse * for any; lists use dashes.");
                return;
            }
            bool official = words[0] == "randomofficial";
            if (!official && words[0] != "random") {
                showMessage("Unknown command. Type help to see all commands.");
                return;
            }
            std::vector<std::string> args(words.begin() + 1, words.end());
            Query query;
            std::string error;
            if (!parseQuery(args, query, official, error)) {
                showMessage(error);
                return;
            }
            m_query = query;
            m_attempt = 0;
            if (official) runOfficial();
            else {
                showMessage("Searching a random level...");
                requestOnlinePage();
            }
        }

        void runOfficial() {
            std::vector<GJGameLevel*> levels;
            auto manager = GameLevelManager::sharedState();
            for (int id = 1; id <= 22; ++id) {
                if (auto level = manager->getMainLevel(id, false); matches(level, m_query)) levels.push_back(level);
            }
            // The embedded main-level table contains The Challenge and the Tower worlds too;
            // discover their IDs rather than relying on undocumented numeric constants.
            auto local = LocalLevelManager::sharedState();
            if (local) {
                for (auto const& [id, encoded] : local->m_mainLevels) {
                    (void)encoded;
                    auto level = manager->getMainLevel(id, false);
                    if (!level) continue;
                    level->m_levelType = GJLevelType::Main;
                    auto name = level->m_levelName;
                    bool bonusOfficial = name == "The Challenge" || name == "The Tower" || name == "The Sewers" ||
                                         name == "The Cellar" || name == "The Secret Hollow";
                    if ((bonusOfficial || (id >= 1 && id <= 22)) && matches(level, m_query) &&
                        std::find(levels.begin(), levels.end(), level) == levels.end()) levels.push_back(level);
                }
            }
            if (levels.empty()) {
                showMessage("No matching official levels available (Tower levels are unrated; use * or no stars).");
                return;
            }
            openLevel(levels[static_cast<size_t>(randomIndex(static_cast<int>(levels.size())))]);
        }

        void requestOnlinePage() {
            auto manager = GameLevelManager::sharedState();
            auto object = GJSearchObject::create(SearchType::Recent);
            if (!object) {
                showMessage("Could not create a level search.");
                return;
            }
            object->m_page = randomIndex(100);
            m_previousDelegate = manager->m_levelManagerDelegate;
            manager->m_levelManagerDelegate = this;
            m_waiting = true;
            manager->getOnlineLevels(object);
        }

        void openLevel(GJGameLevel* level) {
            if (!level) return;
            showMessage(("Opening: " + level->m_levelName).c_str());
            if (m_input) m_input->detachWithIME();
            s_consoleVisible = false;
            level->retain();
            auto scene = PlayLayer::scene(level, false);
            if (scene) CCDirector::sharedDirector()->replaceScene(scene);
            level->release();
        }

        TextInput* m_input = nullptr;
        cocos2d::CCLabelBMFont* m_output = nullptr;
        Query m_query;
        LevelManagerDelegate* m_previousDelegate = nullptr;
        bool m_waiting = false;
        int m_attempt = 0;
    };
}

namespace {
    geode::comm::ListenerHandle g_sceneListener;
}

$on_mod(Loaded) {
    g_sceneListener = SceneEvent().listen([](cocos2d::CCScene* scene) {
        if (!scene || scene->getChildByID("domino-random-console")) return false;
        if (auto console = ConsoleLayer::create()) {
            console->setID("domino-random-console");
            console->setVisible(ConsoleLayer::s_consoleVisible);
            scene->addChild(console, 9999);
        }
        return false;
    });
}
