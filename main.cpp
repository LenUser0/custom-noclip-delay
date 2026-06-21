#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <sstream>
#include <iomanip>

using namespace geode::prelude;

// =====================================================================
//  GLOBAL REGISTRY VARIABLES (VOLATILE SESSION VARIABLES)
// =====================================================================
bool g_noclipEnabled = false;
bool g_delayEnabled = true;
bool g_autoSafeMode = true;
bool g_warnedOnce = false;

// Challenge Constraints
double g_accuracyLimit = 0.0;
int g_deathLimit = 0;

// Flash Indicator Configurations
bool g_tintEnabled = false;
float g_tintDuration = 0.15f;
ccColor3B g_tintColor = { 255, 100, 100 }; // Default soft crimson red

// Overlay Positions and Layout Anchors
bool g_showAccuracy = true;
bool g_showDeaths = true;
CCPoint g_overlayPos = { -1.0f, -1.0f };

// =====================================================================
//  LEVEL PERSISTENT STORAGE CONTROLLERS (SAVE SETTINGS PER LEVEL)
// =====================================================================
std::string getCurrentLevelID(PlayLayer* layer) {
    if (!layer || !layer->m_level) return "global";
    return std::to_string(layer->m_level->m_levelID.value());
}

double getTargetPercentForLevel(std::string levelID) {
    return Mod::get()->getSavedValue<double>("percent_" + levelID, 0.0);
}

void setTargetPercentForLevel(std::string levelID, double value) {
    Mod::get()->setSavedValue<double>("percent_" + levelID, value);
    Mod::get()->saveData();
}

// =====================================================================
//  VISUAL REVENUE SYSTEMS (TRANSLUCENT HUD ELEMENT GRAPHICS)
// =====================================================================
class NoclipOverlay : public CCNode {
public:
    CCLabelBMFont* m_statusLabel;
    CCLabelBMFont* m_accuracyLabel;
    CCLabelBMFont* m_deathsLabel;

    bool init() {
        if (!CCNode::init()) return false;

        // Status Label Design Configuration
        m_statusLabel = CCLabelBMFont::create("Noclip: OFF", "bigFont.fnt");
        m_statusLabel->setScale(0.4f);
        m_statusLabel->setOpacity(150); // Translucent rendering overlay
        m_statusLabel->setColor({ 255, 100, 100 });
        this->addChild(m_statusLabel);

        // Accuracy Counter Node Setup
        m_accuracyLabel = CCLabelBMFont::create("Accuracy: 100.00%", "bigFont.fnt");
        m_accuracyLabel->setScale(0.3f);
        m_accuracyLabel->setOpacity(150);
        m_accuracyLabel->setVisible(false);
        this->addChild(m_accuracyLabel);

        // Death Registry Metric Configuration
        m_deathsLabel = CCLabelBMFont::create("Deaths: 0", "bigFont.fnt");
        m_deathsLabel->setScale(0.3f);
        m_deathsLabel->setOpacity(150);
        m_deathsLabel->setVisible(false);
        this->addChild(m_deathsLabel);

        this->updatePositions();
        return true;
    }

    void updatePositions() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        if (g_overlayPos.x == -1.0f) {
            g_overlayPos = { winSize.width - 70, winSize.height - 20 };
        }
        m_statusLabel->setPosition(g_overlayPos);
        m_accuracyLabel->setPosition({ g_overlayPos.x, g_overlayPos.y - 12 });
        m_deathsLabel->setPosition({ g_overlayPos.x, g_overlayPos.y - 22 });
    }

    CREATE_FUNC(NoclipOverlay);
};

// Interface Element Interceptor for drag configuration positioning
class DragLabel : public CCMenuItem {
public:
    CCLabelBMFont* m_label;
    bool init() {
        if (!CCMenuItem::init()) return false;
        m_label = CCLabelBMFont::create("Drag Me to Position!", "bigFont.fnt");
        m_label->setScale(0.4f);
        m_label->setOpacity(180);
        this->addChild(m_label);
        this->setContentSize(m_label->getContentSize() * 0.4f);
        return true;
    }
    void activate() override {}
    CREATE_FUNC(DragLabel);
};

// =====================================================================
//  ADVANCED VALUATION SYSTEM (SECRET USER VALUATION POPUP)
// =====================================================================
class SafeModePopup : public Popup<> {
protected:
    CCMenuItemToggler* m_safeToggle;

    bool setup() override {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        this->setTitle("Advanced Settings");

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        // Strict Enforcement Matrix: Auto Safe Mode Toggle Node Configuration Only!
        m_safeToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(SafeModePopup::onToggleSafeMode), 0.7f);
        m_safeToggle->setPosition({winSize.width / 2 - 60, winSize.height / 2});
        m_safeToggle->toggle(g_autoSafeMode);
        menu->addChild(m_safeToggle);

        auto safeLabel = CCLabelBMFont::create("Auto Safe Mode", "bigFont.fnt");
        safeLabel->setScale(0.4f);
        safeLabel->setAnchorPoint({0, 0.5});
        safeLabel->setPosition({winSize.width / 2 - 40, winSize.height / 2});
        m_mainLayer->addChild(safeLabel);

        return true;
    }

    void onToggleSafeMode(CCObject* sender) {
        // Enforce the Two-Step Warning Lockout sequence on first disabling attempt
        if (g_autoSafeMode && !g_warnedOnce) {
            m_safeToggle->toggle(true); // Return button state to checked until user authorizes twice
            FLAlertLayer::create(
                "Warning",
                "Disabling Auto Safe Mode allows you to beat levels using noclip. This is considered <cr>cheating</c>!",
                "OK"
            )->show();
            g_warnedOnce = true;
            return;
        }
        g_autoSafeMode = !g_autoSafeMode;
    }

public:
    static SafeModePopup* create() {
        auto ret = new SafeModePopup();
        if (ret && ret->initAnchored(240, 120)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// =====================================================================
//  CORE INTERFACE SYSTEM (MAIN NOCLIP DELAY CONTROL PANEL MENU)
// =====================================================================
class NoclipSettingsPopup : public Popup<std::string> {
protected:
    TextInput* m_percentInput;
    TextInput* m_tintTimeInput;
    TextInput* m_accLimitInput;
    TextInput* m_deathLimitInput;

    CCSlider* m_tintSlider;
    DragLabel* m_dragLabel;
    std::string m_levelID;

    bool setup(std::string levelID) override {
        m_levelID = levelID;
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        this->setTitle("Noclip Delay Settings");

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        // Core State Activator
        auto noclipToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(NoclipSettingsPopup::onToggleNoclip), 0.6f);
        noclipToggle->setPosition({winSize.width / 2 - 100, winSize.height / 2 + 55});
        noclipToggle->toggle(g_noclipEnabled);
        menu->addChild(noclipToggle);

        auto noclipLabel = CCLabelBMFont::create("Enable Noclip", "bigFont.fnt");
        noclipLabel->setScale(0.35f); noclipLabel->setAnchorPoint({0, 0.5});
        noclipLabel->setPosition({winSize.width / 2 - 85, winSize.height / 2 + 55});
        m_mainLayer->addChild(noclipLabel);

        // Delay Parameter State Controller
        auto delayToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(NoclipSettingsPopup::onToggleDelay), 0.6f);
        delayToggle->setPosition({winSize.width / 2 + 10, winSize.height / 2 + 55});
        delayToggle->toggle(g_delayEnabled);
        menu->addChild(delayToggle);

        auto delayLabel = CCLabelBMFont::create("Use Delay %", "bigFont.fnt");
        delayLabel->setScale(0.35f); delayLabel->setAnchorPoint({0, 0.5});
        delayLabel->setPosition({winSize.width / 2 + 25, winSize.height / 2 + 55});
        m_mainLayer->addChild(delayLabel);

        // Floating Point Precision Input Level Tracking
        m_percentInput = TextInput::create(60.0f, "0.0", "bigFont.fnt");
        m_percentInput->setPosition({winSize.width / 2 - 60, winSize.height / 2 + 25});
        m_percentInput->setFilter("0123456789.");
        {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << getTargetPercentForLevel(m_levelID);
            m_percentInput->setString(ss.str());
        }
        m_mainLayer->addChild(m_percentInput);

        auto pctLbl = CCLabelBMFont::create("Level %", "bigFont.fnt");
        pctLbl->setScale(0.35f); pctLbl->setAnchorPoint({0, 0.5});
        pctLbl->setPosition({winSize.width / 2 - 25, winSize.height / 2 + 25});
        m_mainLayer->addChild(pctLbl);

        // Death Flash Visual Configuration Pipeline Setup
        auto tintToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(NoclipSettingsPopup::onToggleTint), 0.6f);
        tintToggle->setPosition({winSize.width / 2 - 100, winSize.height / 2 - 5});
        tintToggle->toggle(g_tintEnabled);
        menu->addChild(tintToggle);

        auto tintLbl = CCLabelBMFont::create("Death Tint Flash", "bigFont.fnt");
        tintLbl->setScale(0.35f); tintLbl->setAnchorPoint({0, 0.5});
        tintLbl->setPosition({winSize.width / 2 - 85, winSize.height / 2 - 5});
        m_mainLayer->addChild(tintLbl);

        // Slider and Text Integration Processing
        m_tintTimeInput = TextInput::create(50.0f, "0.15", "bigFont.fnt");
        m_tintTimeInput->setPosition({winSize.width / 2 + 65, winSize.height / 2 - 5});
        m_tintTimeInput->setFilter("0123456789.");
        {
            std::stringstream ss2;
            ss2 << g_tintDuration;
            m_tintTimeInput->setString(ss2.str());
        }
        m_mainLayer->addChild(m_tintTimeInput);

        // Challenge Threshold Configuration Text Inputs
        m_accLimitInput = TextInput::create(50.0f, "0.0", "bigFont.fnt");
        m_accLimitInput->setPosition({winSize.width / 2 - 60, winSize.height / 2 - 35});
        m_accLimitInput->setFilter("0123456789.");
        {
            std::stringstream ss3;
            ss3 << g_accuracyLimit;
            m_accLimitInput->setString(ss3.str());
        }
        m_mainLayer->addChild(m_accLimitInput);

        auto accLimLbl = CCLabelBMFont::create("Min Acc %", "bigFont.fnt");
        accLimLbl->setScale(0.35f); accLimLbl->setAnchorPoint({0, 0.5});
        accLimLbl->setPosition({winSize.width / 2 - 30, winSize.height / 2 - 35});
        m_mainLayer->addChild(accLimLbl);

        m_deathLimitInput = TextInput::create(50.0f, "0", "bigFont.fnt");
        m_deathLimitInput->setPosition({winSize.width / 2 + 45, winSize.height / 2 - 35});
        m_deathLimitInput->setFilter("0123456789");
        {
            std::stringstream ss4;
            ss4 << g_deathLimit;
            m_deathLimitInput->setString(ss4.str());
        }
        m_mainLayer->addChild(m_deathLimitInput);

        auto dthLimLbl = CCLabelBMFont::create("Max Deaths", "bigFont.fnt");
        dthLimLbl->setScale(0.35f); dthLimLbl->setAnchorPoint({0, 0.5});
        dthLimLbl->setPosition({winSize.width / 2 + 75, winSize.height / 2 - 35});
        m_mainLayer->addChild(dthLimLbl);

        // Interface HUD Positioning Workspace Configuration
        m_dragLabel = DragLabel::create();
        m_dragLabel->setPosition(g_overlayPos);
        menu->addChild(m_dragLabel);
        this->setTouchEnabled(true);

        // Advanced Validation Access Port (Strict Layout Alignment Requirements)
        auto invisibleSprite = CCSprite::create();
        invisibleSprite->setTextureRect(CCRect(0, 0, 25, 25));
        invisibleSprite->setOpacity(0);
        auto secretButton = CCMenuItemSpriteExtra::create(invisibleSprite, this, menu_selector(NoclipSettingsPopup::onSecretMenu));
        secretButton->setPosition({winSize.width / 2 + 120, winSize.height / 2 - 95});
        menu->addChild(secretButton);

        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto touchPoint = touch->getLocation();
        if (touchPoint.x < 30) touchPoint.x = 30;
        if (touchPoint.x > winSize.width - 30) touchPoint.x = winSize.width - 30;
        if (touchPoint.y < 30) touchPoint.y = 30;
        if (touchPoint.y > winSize.height - 30) touchPoint.y = winSize.height - 30;
        g_overlayPos = touchPoint;
        if (m_dragLabel) m_dragLabel->setPosition(g_overlayPos);
    }

    void onToggleNoclip(CCObject* sender) { g_noclipEnabled = !g_noclipEnabled; }
    void onToggleDelay(CCObject* sender) { g_delayEnabled = !g_delayEnabled; }
    void onToggleTint(CCObject* sender) { g_tintEnabled = !g_tintEnabled; }
    void onSecretMenu(CCObject* sender) { SafeModePopup::create()->show(); }

    void onClose(CCObject* sender) override {
        try {
            setTargetPercentForLevel(m_levelID, std::stod(m_percentInput->getString()));
            g_tintDuration = std::stof(m_tintTimeInput->getString());
            g_accuracyLimit = std::stod(m_accLimitInput->getString());
            g_deathLimit = std::stoi(m_deathLimitInput->getString());
        } catch(...) {}
        Popup::onClose(sender);
    }

public:
    static NoclipSettingsPopup* create(std::string levelID) {
        auto ret = new NoclipSettingsPopup();
        if (ret && ret->initAnchored(270, 230, levelID)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// Hooking engine into the game's default pause layout layer
class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto rightMenu = this->getChildByID("right-button-menu");
        if (rightMenu) {
            auto spikeSprite = CCSprite::createWithSpriteFrameName("spikes_01_001.png");
            spikeSprite->setScale(0.5f);
            auto myButton = CCMenuItemSpriteExtra::create(spikeSprite, this, menu_selector(MyPauseLayer::onCustomNoclipMenu));
            myButton->setID("noclip-delay-button");
            rightMenu->addChild(myButton);
            rightMenu->updateLayout();
        }
    }

    void onCustomNoclipMenu(CCObject* sender) {
        auto playLayer = PlayLayer::get();
        NoclipSettingsPopup::create(playLayer ? getCurrentLevelID(playLayer) : "global")->show();
    }
};

// =====================================================================
// PLAY EXECUTION SYSTEMS (REAL-TIME LOGIC & HIT DETECTOR INJECTION)
// =====================================================================
class $modify(MyPlayLayer, PlayLayer) {
    bool m_hasCheated = false;
    NoclipOverlay* m_overlay = nullptr;
    CCLayerColor* m_flashLayer = nullptr;
    int m_totalTicks = 0;
    int m_deathTicks = 0;
    int m_totalDeathsCounter = 0;

    bool init(GJGameLevel* level, bool useSubsequent, bool useSubsequent2) {
        if (!PlayLayer::init(level, useSubsequent, useSubsequent2)) return false;
        m_overlay = NoclipOverlay::create();
        this->addChild(m_overlay, 999);

        // Visual Interceptor Framework Setup
        m_flashLayer = CCLayerColor::create(ccc4(g_tintColor.r, g_tintColor.g, g_tintColor.b, 0));
        this->addChild(m_flashLayer, 998);
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (!m_overlay) return;
        m_overlay->updatePositions();

        if (!g_noclipEnabled) {
            m_overlay->m_statusLabel->setString("Noclip: OFF");
            m_overlay->m_statusLabel->setColor({ 255, 100, 100 });
            m_overlay->m_accuracyLabel->setVisible(false);
            m_overlay->m_deathsLabel->setVisible(false);
            return;
        }

        // Analytical Mathematics Logic Processing Execution Track
        double currentX = m_player1->m_position.x;
        double endX = m_levelEndAnimationXPos;
        double currentPercent = (endX > 0) ? (currentX / endX) * 100.0 : 0.0;
        double targetPercent = getTargetPercentForLevel(getCurrentLevelID(this));
        bool isPastTarget = (currentPercent >= targetPercent) || !g_delayEnabled;

        if (isPastTarget) {
            m_overlay->m_statusLabel->setString("Noclip: ACTIVE!");
            m_overlay->m_statusLabel->setColor({ 100, 255, 100 });
            m_totalTicks++;
            double accuracy = (m_totalTicks > 0) ?
                ((double)(m_totalTicks - m_deathTicks) / m_totalTicks) * 100.0 : 100.0;

            // Enforce Accuracy Constraint Failure
            if (g_accuracyLimit > 0.0 && accuracy < g_accuracyLimit) {
                g_noclipEnabled = false; // System override shutdown execution
            }

            if (g_showAccuracy) {
                m_overlay->m_accuracyLabel->setVisible(true);
                std::stringstream ss;
                ss << "Accuracy: " << std::fixed << std::setprecision(2) << accuracy << "%";
                m_overlay->m_accuracyLabel->setString(ss.str());
            }

            if (g_showDeaths) {
                m_overlay->m_deathsLabel->setVisible(true);
                m_overlay->m_deathsLabel->setString("Deaths: " + std::to_string(m_totalDeathsCounter));
            }
        } else {
            m_overlay->m_statusLabel->setString("Noclip: OFF");
            m_overlay->m_statusLabel->setColor({ 255, 100, 100 });
            m_overlay->m_accuracyLabel->setVisible(false);
            m_overlay->m_deathsLabel->setVisible(false);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (!g_noclipEnabled) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        double currentX = m_player1->m_position.x;
        double endX = m_levelEndAnimationXPos;
        double currentPercent = (endX > 0) ? (currentX / endX) * 100.0 : 0.0;
        double targetPercent = getTargetPercentForLevel(getCurrentLevelID(this));
        bool isPastTarget = (currentPercent >= targetPercent) || !g_delayEnabled;

        if (isPastTarget) {
            m_hasCheated = true;
            m_deathTicks++;
            static int lastObjectID = 0;
            if (object && object->m_objectID != lastObjectID) {
                m_totalDeathsCounter++;
                lastObjectID = object->m_objectID;

                // Enforce Death Threshold Limit Check
                if (g_deathLimit > 0 && m_totalDeathsCounter >= g_deathLimit) {
                    g_noclipEnabled = false; // System enforcement matrix block triggered
                }

                // Flash Rendering Action Execution Pipeline
                if (g_tintEnabled && m_flashLayer) {
                    m_flashLayer->setOpacity(80); // Translucent flash visibility allocation
                    m_flashLayer->stopAllActions();
                    m_flashLayer->runAction(CCFadeOut::create(g_tintDuration));
                }
            }
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        if (m_hasCheated && g_autoSafeMode) {
            FLAlertLayer::create("Safe Mode Active", "Level completed with Noclip Delay.\nProgress was not saved!", "OK")->show();
            this->onQuit();
            return;
        }
        PlayLayer::levelComplete();
    }
};
