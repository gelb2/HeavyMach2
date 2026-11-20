#include "Runtime.hpp"
#include "Renderer.hpp"
#include "Media.hpp"

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <codecvt>
#include <locale>
#include <memory>
#include <stdexcept>

namespace hm2::android {
namespace {
using Utf16 = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>;

std::string fromJava(JNIEnv* env, jstring value) {
    if (!value) return {};
    std::u16string chars(env->GetStringLength(value), 0);
    env->GetStringRegion(value, 0, static_cast<jsize>(chars.size()), reinterpret_cast<jchar*>(chars.data()));
    return Utf16{}.to_bytes(chars);
}
jstring toJava(JNIEnv* env, std::string_view value) {
    auto chars = Utf16{}.from_bytes(value.data(), value.data() + value.size());
    return env->NewString(reinterpret_cast<const jchar*>(chars.data()), static_cast<jsize>(chars.size()));
}
void checkJava(JNIEnv* env) {
    if (env->ExceptionCheck()) throw std::runtime_error("Android host callback failed");
}
class LocalFrame {
    JNIEnv* env_;
public:
    explicit LocalFrame(JNIEnv* env): env_(env) {
        if (env_->PushLocalFrame(10) < 0) throw std::runtime_error("Cannot allocate JNI local frame");
    }
    ~LocalFrame() { env_->PopLocalFrame(nullptr); }
};

// AAssetManager_fromJava borrows its native pointer. Keep the exact Java
// AssetManager alive across configuration changes and GL context replacement.
class AssetOwner {
    JavaVM* vm_;
    jobject manager_;
public:
    AssetOwner(JNIEnv* env, jobject manager) {
        env->GetJavaVM(&vm_);
        manager_ = env->NewGlobalRef(manager);
        checkJava(env);
        if (!manager_) throw std::runtime_error("Missing Java AssetManager");
    }
    ~AssetOwner() {
        JNIEnv* env = nullptr;
        if (vm_->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6) == JNI_OK)
            env->DeleteGlobalRef(manager_);
    }
    AAssetManager* native(JNIEnv* env) const { return AAssetManager_fromJava(env, manager_); }
};

class Host final : public ui::TextRasterizer {
    JavaVM* vm_;
    jobject owner_;
    jmethodID raster_, music_, audio_, dialog_;

    JNIEnv* env() const {
        JNIEnv* result = nullptr;
        if (vm_->GetEnv(reinterpret_cast<void**>(&result), JNI_VERSION_1_6) != JNI_OK)
            throw std::runtime_error("Android host called outside game thread");
        return result;
    }
public:
    Host(JNIEnv* jni, jobject activity) {
        jni->GetJavaVM(&vm_);
        const auto type = jni->GetObjectClass(activity);
        raster_ = jni->GetMethodID(type,"raster","(Ljava/lang/String;Ljava/lang/String;FIIFFI)[B");
        music_ = jni->GetMethodID(type,"playMusic","(Ljava/lang/String;)V");
        audio_ = jni->GetMethodID(type,"audioActive","(Z)V");
        dialog_ = jni->GetMethodID(type,"showDialog","(IJLjava/lang/String;Ljava/lang/String;)V");
        checkJava(jni);
        owner_ = jni->NewGlobalRef(activity);
        checkJava(jni);
    }
    ~Host() { env()->DeleteGlobalRef(owner_); }
    ui::TextBitmap rasterize(const ui::TextRasterRequest& request) override {
        auto* jni = env();
        LocalFrame locals(jni);
        ui::TextBitmap result;
        result.request = request;
        auto& layout = result.layout = ui::textRasterLayout(request);
        auto pixels = static_cast<jbyteArray>(jni->CallObjectMethod(owner_, raster_,
            toJava(jni, request.text), toJava(jni, request.fontName), layout.fontSize,
            static_cast<jint>(layout.width), static_cast<jint>(layout.height),
            layout.content.width, layout.content.height, static_cast<jint>(request.alignment)));
        checkJava(jni);
        const auto count = static_cast<jsize>(layout.width * layout.height);
        if (!pixels || jni->GetArrayLength(pixels) != count) throw std::runtime_error("Invalid Android text dimensions");
        result.alpha.resize(count);
        jni->GetByteArrayRegion(pixels,0,count,reinterpret_cast<jbyte*>(result.alpha.data()));
        checkJava(jni);
        result.metrics.resolvedFontName = "Android sans-serif";
        return result;
    }
    void music(std::string_view track) {
        auto* jni = env(); LocalFrame locals(jni);
        jni->CallVoidMethod(owner_, music_, toJava(jni,track));
        checkJava(jni);
    }
    void audio(bool active) {
        auto* jni = env();
        jni->CallVoidMethod(owner_, audio_, static_cast<jboolean>(active));
        checkJava(jni);
    }
    void dialog(const Dialog& request) {
        auto* jni = env(); LocalFrame locals(jni);
        jni->CallVoidMethod(owner_,dialog_,static_cast<jint>(request.kind),
            static_cast<jlong>(request.argument),toJava(jni,request.title),toJava(jni,request.message));
        checkJava(jni);
    }
};
struct Game {
    Host host;
    AssetOwner assetOwner;
    Assets assets;
    Effects effects;
    Renderer renderer;
    int drawableWidth = 0, drawableHeight = 0;
    Runtime runtime;

    Game(JNIEnv* env,jobject owner,jobject manager,const std::string& directory):
        host(env,owner),assetOwner(env,manager),assets(assetOwner.native(env)),effects(assets),renderer(assets),
        runtime({assets,host,
            [this](const Frame& f){ renderer.prepareFrame(f); },
            [this](const Frame& f){ renderer.retain(f); },
            [this](std::string_view key,const ui::TextBitmap& b){ renderer.prepareText(b,std::string(key)); },
            [this](const Frame& f){ renderer.draw(f,drawableWidth,drawableHeight); },
            [this](std::string_view n){ effects.prepare(n); },
            [this](std::string_view n,Vec2 p){ effects.play(n,p); },
            [this](std::string_view n){ host.music(n); },
            [this](bool on){ effects.setActive(on); host.audio(on); },
            [this](const Dialog& d){ host.dialog(d); }}, directory) {}
};
void fail(JNIEnv* env, const std::exception& error) {
    __android_log_print(ANDROID_LOG_ERROR,"HeavyMach2","%s",error.what());
    if (!env->ExceptionCheck()) env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),error.what());
}
template<class F> void apply(JNIEnv* env,jlong handle,F action) {
    try {
        if (!handle) throw std::runtime_error("HeavyMach 2 session is unavailable");
        action(*reinterpret_cast<Game*>(handle));
    } catch (const std::exception& error) { fail(env,error); }
}
}
}
using namespace hm2;
using namespace hm2::android;

#define GAME_JNI(name) JNIEXPORT JNICALL Java_com_gelb2_heavymach2_NativeGame_##name
extern "C" {
jlong GAME_JNI(create)(JNIEnv* env,jobject,jobject activity,jobject assets,jstring directory) {
    try { return reinterpret_cast<jlong>(new Game(env,activity,assets,fromJava(env,directory))); }
    catch (const std::exception& error) { fail(env,error); return 0; }
}
void GAME_JNI(restore)(JNIEnv* env,jobject,jlong handle) {
    apply(env,handle,[](Game& g){ g.renderer.contextLost(); g.runtime.restoreGraphics(); });
}
void GAME_JNI(display)(JNIEnv* env,jobject,jlong handle,jint width,jint height,jfloat density,
                       jint left,jint bottom,jint right,jint top,jboolean tablet) {
    apply(env,handle,[&](Game& g){
        g.drawableWidth = width;
        g.drawableHeight = height;
        auto config = DisplayConfiguration::surface({width/density,height/density},
            {left/density,bottom/density,right/density,top/density});
        config.device = tablet ? DisplayDevice::tablet : DisplayDevice::phone;
        g.runtime.setDisplay(config);
    });
}
void GAME_JNI(frame)(JNIEnv* env,jobject,jlong handle,jint ticks) {
    apply(env,handle,[&](Game& g){ for (int i=0;i<ticks;++i) g.runtime.tick(); g.runtime.draw(); });
}
void GAME_JNI(active)(JNIEnv* env,jobject,jlong handle,jboolean active) {
    apply(env,handle,[&](Game& g){g.runtime.setActive(active);});
}
void GAME_JNI(focus)(JNIEnv* env,jobject,jlong handle,jboolean focus) {
    apply(env,handle,[&](Game& g){g.runtime.setAudioFocused(focus);});
}
void GAME_JNI(touch)(JNIEnv* env,jobject,jlong handle,jint phase,jint pointer,jfloat x,jfloat y) {
    apply(env,handle,[&](Game& g){g.runtime.input(static_cast<PointerPhase>(phase),{x,y},pointer);});
}
void GAME_JNI(cancel)(JNIEnv* env,jobject,jlong handle) {
    apply(env,handle,[](Game& g){g.runtime.cancelInputs();});
}
void GAME_JNI(confirm)(JNIEnv* env,jobject,jlong handle,jint kind,jlong arg,jstring name,jstring country) {
    apply(env,handle,[&](Game& g){g.runtime.confirm(static_cast<DialogKind>(kind),arg,fromJava(env,name),fromJava(env,country));});
}
void GAME_JNI(destroy)(JNIEnv*,jobject,jlong handle) {
    auto* game = reinterpret_cast<Game*>(handle);
    if (game) {
        // GLSurfaceView has joined its render thread and destroyed its EGL context.
        game->renderer.contextLost();
        delete game;
    }
}
#if HM2_CHEAT
JNIEXPORT jstring JNICALL Java_com_gelb2_heavymach2_TestControl_execute(JNIEnv* env,jobject,jlong handle,jstring command) {
    jstring result = nullptr;
    apply(env,handle,[&](Game& g){ result = toJava(env,g.runtime.validate(fromJava(env,command))); });
    return result;
}
#endif
}
