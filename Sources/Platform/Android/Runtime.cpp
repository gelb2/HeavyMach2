#include "Runtime.hpp"
#include "Storage.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#if HM2_CHEAT
#include "Validation.hpp"
#include <set>
#endif
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <limits>
#include <map>
#include <sstream>

namespace hm2::android {
namespace {
std::atomic<std::uint64_t> nextRuntimeIdentity{0};

struct Touch {
    Vec2 previous;
    bool capturedByWorld = false;
};

std::string jsonString(std::string_view value) {
    std::string result(1, '"');
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') result += '\\';
        if (c < 32) {
            constexpr char hex[] = "0123456789abcdef";
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else result += char(c);
    }
    result += '"';
    return result;
}
}

struct Runtime::Impl final : SessionHost, ui::ScreensHost {
    RuntimeServices services;
    const std::filesystem::path directory;
    DarwinWorldRandom random{1};
    PreparedTiles tiles{"screens"};
    ui::TextPreparationCache text;
    std::map<const ui::TextBitmap*,std::string> textNames;
    // Several isolated validation saves share one renderer. Texture names
    // must stay distinct while their text caches have overlapping lifetimes.
    const std::string textPrefix="@text/"+std::to_string(nextRuntimeIdentity.fetch_add(1))+"/";
    std::unique_ptr<GameSession> game;
    std::unique_ptr<ui::Screens> screens;
    std::map<std::uintptr_t,Touch> touches;
    std::uint64_t revision = 0, resourceRevision = 0, uiGeneration = 0, nextText = 0, ticks = 0;
    CombatScene* lastScene = nullptr;
#ifdef __ANDROID__
    int loggedScreen = -1;
    std::uint64_t loggedSaves = 0;
#endif
    bool active = true, audioFocused = true;
#if HM2_CHEAT
    std::uint32_t testClock = 731;
    std::string playableMusic;
    std::unique_ptr<Validation> validation;
    std::unique_ptr<Campaign> campaign;
#endif

    Impl(RuntimeServices value, std::filesystem::path files)
        : services(std::move(value)), directory(std::move(files)) {
        std::optional<SaveData> data;
        if (auto bytes = readFile(directory / "HeavyMach2.sav", SaveData::byteCount))
            data = SaveData::decode(*bytes, random);
#if HM2_CHEAT
        else data = playableFixture();
#endif
        game = std::make_unique<GameSession>(services.resources, *this, random, std::move(data));
        for (const auto& name : game->orderedStartupSounds()) prepareSound(name);
        screens = std::make_unique<ui::Screens>(services.resources, tiles, *this);
        synchronize(true);
    }

    void sound(std::string_view name, Vec2 point) override {
        if (active && audioFocused && !name.empty()) services.playSound(name,point);
    }
    void soundID(unsigned id, Vec2 point) override { sound(audio::soundFilename(id),point); }
    void sound(std::string_view name) override { sound(name,{}); }
    void sfx(unsigned id) override { soundID(id,{}); }
    void prepareSound(std::string_view name) override { services.prepareSound(name); }
    void bgm(std::string_view name) override {
#if HM2_CHEAT
        playableMusic = name;
#endif
        services.playMusic(name);
    }
    void notice(std::string_view title, std::string_view message) override {
        services.showDialog({DialogKind::notice,0,std::string(title),std::string(message)});
    }
    void alert(std::string_view key) override { alert(key, key == "IncorrectName" ? "Incorrect Name" : "Alert!"); }
    void alert(std::string_view key, std::string_view title) override {
        const auto kind = key == "VisitIndiean" ? DialogKind::visitWebsite : DialogKind::notice;
        services.showDialog({kind,0,std::string(title),game->originalText(key)});
    }
    void requestName(unsigned slot) override { services.showDialog({DialogKind::name,slot,"New User",""}); }
    void requestDelete(unsigned slot) override {
        services.showDialog({DialogKind::deleteSlot,slot,"Delete Slot",game->originalText("DeleteSlot")});
    }
    void requestGameOver(std::uint64_t token) override {
        services.showDialog({DialogKind::gameOver,token,"Destroy Your Mach.",game->originalText("DestroyMach")});
    }
    bool writeSave(std::span<const std::uint8_t> bytes, SaveReason) override {
        if (writeFile(directory / "HeavyMach2.sav", bytes)) return true;
        notice("HeavyMach 2", "진행 데이터를 저장하지 못했습니다.");
        return false;
    }
    std::optional<Bytes> loadHUDLayout(std::string_view profile) override {
        return readFile(hudPath(directory, profile), 65536);
    }
    bool saveHUDLayout(std::string_view profile, std::span<const std::uint8_t> bytes) override {
        return !bytes.empty() && bytes.size() <= 65536 && writeFile(hudPath(directory, profile), bytes);
    }
    std::uint32_t absoluteTimeLow() override {
#if HM2_CHEAT
        return testClock;
#else
        return static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }
    std::uint32_t unixTime() override {
#if HM2_CHEAT
        return 1794268800;
#else
        return static_cast<std::uint32_t>(std::time(nullptr));
#endif
    }

    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& request) override {
        auto bitmap = text.prepare(services.rasterizer,request);
        auto found = textNames.find(bitmap.get());
        if (found == textNames.end()) {
            const auto name = textPrefix + std::to_string(nextText++);
            services.uploadText(name,*bitmap);
            found = textNames.emplace(bitmap.get(),name).first;
        }
        return {found->second, bitmap};
    }
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& request) override {
        return prepareText(request);
    }
    void action(const ui::ScreenAction& action) override {
        game->action(action);
        synchronize();
    }
    void synchronize(bool force = false) {
        #ifdef __ANDROID__
        const auto screen = static_cast<int>(game->state().screen);
        const auto saves = game->metrics().saveCompletions;
        if (screen != loggedScreen || saves != loggedSaves) {
            __android_log_print(ANDROID_LOG_INFO, "HM2Runtime", "screen=%d saves=%llu",screen,static_cast<unsigned long long>(saves));
            loggedScreen=screen; loggedSaves=saves;
        }
#endif
        const bool rebuilt = revision != game->presentationRevision();
        if (rebuilt) {
            screens->refresh(game->state(),game->renderTime());
            revision = game->presentationRevision();
        } else screens->updateHUD(game->state());
        if (!force && !rebuilt && lastScene == game->battle() &&
            resourceRevision == game->resourceRevision() && uiGeneration == screens->resourceGeneration()) return;
        lastScene = game->battle();
        resourceRevision = game->resourceRevision();
        uiGeneration = screens->resourceGeneration();
        Frame owners = game->preparationFrame();
        screens->appendPreparation(owners);
        services.prepareFrame(owners);
        for (const auto* expired : text.discardUnused()) textNames.erase(expired);
        for (const auto& [bitmap,name] : textNames) {
            Sprite sprite;
            sprite.image = name;
            owners.sprites.push_back(std::move(sprite));
        }
        services.retainTextures(owners);
    }
};

Runtime::Runtime(RuntimeServices services,std::filesystem::path files)
    : impl_(std::make_unique<Impl>(std::move(services),std::move(files))) {}
Runtime::~Runtime() = default;

void Runtime::tick() {
    auto& p = *impl_;
    if (!p.active) return;
#if HM2_CHEAT
    if (p.campaign) { p.campaign->tick(); return; }
    if (p.validation) { p.validation->tick(); return; }
#endif
    p.game->tick([&p]{
        p.synchronize();
        p.screens->tick(p.game->renderTime(),p.random);
    });
    p.synchronize();
    ++p.ticks;
}

void Runtime::draw() {
    auto& p = *impl_;
#if HM2_CHEAT
    if (p.campaign) { p.campaign->draw(); return; }
    if (p.validation) { p.validation->draw(); return; }
#endif
    Frame frame = p.game->drawWorld();
    p.screens->append(frame,p.game->renderTime());
    p.game->appendOverlays(frame);
    p.services.drawFrame(frame);
}

void Runtime::setDisplay(DisplayConfiguration display) {
    auto& p = *impl_;
    cancelInputs();
    p.game->setDisplay(display);
    p.screens->setDisplay(display);
#if HM2_CHEAT
    if (p.campaign) { p.campaign->setDisplay(display); return; }
    if (p.validation) { p.validation->setDisplay(display); return; }
#endif
    p.synchronize(true);
}

void Runtime::restoreGraphics() {
    auto& p = *impl_;
#if HM2_CHEAT
    if (p.campaign) { p.campaign->restoreGraphics(); return; }
    if (p.validation) { p.validation->restoreGraphics(); return; }
#endif
    for (const auto& [bitmap,name] : p.textNames) p.services.uploadText(name,*bitmap);
    p.synchronize(true);
}

void Runtime::cancelInputs() {
    auto& p = *impl_;
    for (const auto& [id,touch] : p.touches)
        if (touch.capturedByWorld) p.game->inputWorld(PointerPhase::cancel,touch.previous,id);
    p.touches.clear();
    p.screens->discardInputs();
}

void Runtime::setActive(bool active) {
    auto& p = *impl_;
    if (!active) cancelInputs();
    p.active = active;
    p.game->setActive(active);
#if HM2_CHEAT
    if (p.campaign) p.campaign->setActive(active);
    if (p.validation) p.validation->setActive(active);
#endif
    p.services.setAudioActive(active && p.audioFocused);
}

void Runtime::setAudioFocused(bool focused) {
    auto& p = *impl_;
    #if HM2_CHEAT
    if (p.campaign) p.campaign->setAudioFocused(focused);
#endif
    if (p.audioFocused == focused) return;
    p.audioFocused = focused;
    if (!focused) {
        cancelInputs();
        p.game->setActive(false);
        p.game->setActive(p.active);
    }
    p.services.setAudioActive(focused && p.active);
}

void Runtime::input(PointerPhase phase, Vec2 point, std::uintptr_t id) {
    auto& p = *impl_;
    if (!p.active || !id) return;
#if HM2_CHEAT
    if (p.validation || p.campaign) return;
#endif
    if (phase == PointerPhase::begin) p.touches.insert_or_assign(id,Touch{point});
    auto found = p.touches.find(id);
    if (found == p.touches.end()) return;
    auto& touch = found->second;
    const bool sameMove = phase == PointerPhase::move && point.x == touch.previous.x && point.y == touch.previous.y;
    touch.previous = point;
    const bool capturedByUI = p.game->inputOverlay(phase,point,id) ||
        p.screens->input(phase,point,id,p.game->renderTime());
    if (!capturedByUI && !sameMove) {
        const bool used = p.game->inputWorld(phase,point,id);
        if (phase == PointerPhase::begin) touch.capturedByWorld = used;
    }
    if (phase == PointerPhase::end || phase == PointerPhase::cancel) {
        if (capturedByUI && touch.capturedByWorld) p.game->inputWorld(PointerPhase::cancel,point,id);
        p.touches.erase(found);
    }
    p.synchronize();
}

void Runtime::confirm(DialogKind kind,std::uint64_t argument,std::string_view name,std::string_view country) {
    auto& p = *impl_;
    if (kind == DialogKind::name && argument < SaveData::slotCount) {
        // Android supplies UTF-8; count UTF-16 code units like the platform
        // name field and keep the original 31-byte save payload bound.
        unsigned units=0;
        for (unsigned char c:name) if ((c&0xc0)!=0x80) units+=c>=0xf0?2:1;
        if (!units || units>10 || name.size()>31 || name.find('\0')!=name.npos)
            p.alert("IncorrectName");
        else p.game->createSlot(static_cast<unsigned>(argument),name,country);
    } else if (kind == DialogKind::deleteSlot && argument < SaveData::slotCount)
        p.game->deleteSlot(static_cast<unsigned>(argument));
    else if (kind == DialogKind::gameOver) p.game->confirmGameOver(argument);
    p.synchronize();
}

std::string Runtime::status() {
    auto& p = *impl_;
    auto& game = *p.game;
    const auto& state = game.state();
    std::ostringstream out;
    out << "{\"screen\":" << unsigned(state.screen) << ",\"slot\":" << state.selectedSlot
        << ",\"base\":" << jsonString(state.baseId) << ",\"place\":" << jsonString(state.fieldPlaceName)
        << ",\"ticks\":" << p.ticks << ",\"saves\":" << game.metrics().saveCompletions
        << ",\"saveRequests\":" << game.metrics().saveRequests << ",\"credits\":" << state.credits
        << ",\"level\":" << state.level << ",\"canvas\":[" << game.display().canvas.width << "," << game.display().canvas.height << "]";
    if (auto* battle = game.battle()) {
        const auto pos = battle->player().position;
        out << ",\"player\":[" << pos.x << "," << pos.y << "],\"enemies\":" << battle->enemies().size()
            << ",\"dashStarts\":" << battle->metrics().dashStarts << ",\"sp\":" << battle->dashState().special
            << ",\"paused\":" << (battle->isPaused() ? "true" : "false");
    }
    out << ",\"buttons\":[";
    bool first = true;
    for (const auto& key : p.screens->buttons()) {
        auto button = p.screens->button(key);
        if (!button || !button->active || !button->visible) continue;
        const auto pos = p.screens->buttonScreenPosition(key);
        if (!first) out << ',';
        first = false;
        out << '[' << key.id << ',' << key.intData << ',' << pos.x << ',' << pos.y << ',' << jsonString(key.dataId) << ']';
    }
    out << "]}";
    return out.str();
}

#if HM2_CHEAT
void Runtime::setTestClock(std::uint32_t value){impl_->testClock=value;}
GameSession& Runtime::testSession(){return *impl_->game;}
ui::Screens& Runtime::testScreens(){return *impl_->screens;}
void Runtime::refreshTestScene(){impl_->synchronize(true);}
std::string Runtime::validate(std::string_view command) {
    auto& p = *impl_;
    if (command == "campaign:start" && !p.validation && !p.campaign) {
        cancelInputs();
        p.campaign=std::make_unique<Campaign>(p.services,p.directory/"campaign",p.game->display());
    }
    if (p.campaign) {
        auto result=p.campaign->status();
        if (command=="campaign:stop") {
            p.campaign.reset();
            restoreGraphics();
            if (!p.playableMusic.empty()) p.services.playMusic(p.playableMusic);
        }
        return result;
    }
    if (command == "concurrent:start") {
        if (!p.validation) {
            cancelInputs();
            p.validation = std::make_unique<Validation>(p.services,p.directory / "concurrent",p.game->display());
        }
        return p.validation->status();
    }
    if (command == "concurrent:status" && p.validation) return p.validation->status();
    if (command == "concurrent:stop" && p.validation) {
        const auto result = p.validation->status();
        p.validation.reset();
        restoreGraphics();
        if (!p.playableMusic.empty()) p.services.playMusic(p.playableMusic);
        return result;
    }
    if (command == "status") return status();
    if (command == "artwork") {
        Frame frame;
        p.screens->append(frame, p.game->renderTime());
        p.game->appendOverlays(frame);
        std::set<std::string> artwork;
        for (const auto& sprite : frame.sprites) {
            const auto& file = sprite.textureSource.image.empty() ? sprite.image : sprite.textureSource.image;
            if (!file.empty() && file.front() != '@') artwork.insert(file);
        }
        std::ostringstream out;
        out << "{\"screen\":" << unsigned(p.game->state().screen) << ",\"images\":[";
        bool first = true;
        for (const auto& name : artwork) {
            if (!first) out << ',';
            first = false;
            out << jsonString(name);
        }
        out << "]}";
        return out.str();
    }
    if (p.validation) throw std::invalid_argument("Stop concurrent validation before operating the playable slot");
    if (command == "base" || command.starts_with("field:")) {
        if (p.game->state().selectedSlot < 0 && !p.game->selectSlot(0)) throw std::runtime_error("Missing validation slot");
        if (command == "base") p.game->enterBase();
        else if (!p.game->enterField(command.substr(6),0)) throw std::invalid_argument("Unknown field");
        p.synchronize();
    } else if (command.starts_with("press:")) {
        const auto args = std::string(command.substr(6));
        const auto split = args.find(':');
        const auto id = std::stoul(args.substr(0,split));
        const auto data = split == args.npos ? 0 : std::stoi(args.substr(split+1));
        for (const auto& key : p.screens->buttons()) {
            if (key.id != id || key.intData != data) continue;
            p.screens->revealButton(key);
            auto button = p.screens->button(key);
            if (!button || !button->active || !button->visible) continue;
            const auto pos = p.screens->buttonScreenPosition(key);
            input(PointerPhase::begin,pos,0xABCD);
            input(PointerPhase::end,pos,0xABCD);
            return status();
        }
        throw std::invalid_argument("Button unavailable");
    } else throw std::invalid_argument("Unknown validation command");
    return status();
}
#endif
}
