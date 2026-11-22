import java.awt.Color
import java.awt.Font
import java.awt.RenderingHints
import java.awt.image.BufferedImage
import java.util.Properties
import javax.imageio.ImageIO

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val repo = rootProject.projectDir.parentFile
val assetsRoot = repo.resolve(".local/prepared/Campaign/HD")
val privateSigning = Properties().apply {
    val path = repo.resolve(".local/android-signing/release.properties")
    if (path.isFile) path.inputStream().use(::load)
}
val iconFactory = tasks.register<Exec>("compileLauncherDecoder") {
    val source = rootProject.file("tools/launcher.cpp")
    val decoder = repo.resolve("Sources/Assets/Image.cpp")
    val executable = layout.buildDirectory.file("launcher/decoder").get().asFile
    inputs.files(source, decoder, repo.resolve("Sources/Assets/Image.hpp"), repo.resolve("ThirdParty/stb/stb_image.h"))
    outputs.file(executable)
    doFirst { executable.parentFile.mkdirs() }
    commandLine("c++", "-std=c++20", "-O2", "-I${repo}/Sources", "-I${repo}/ThirdParty/stb", source, decoder, "-o", executable)
}

android {
    namespace = "com.gelb2.heavymach2"
    compileSdk = 36
    ndkVersion = "28.2.13676358"
    defaultConfig {
        applicationId = "com.gelb2.heavymach2.restored"
        minSdk = 26
        targetSdk = 36
        versionCode = 2
        versionName = "0.4"
        ndk { abiFilters += providers.gradleProperty("hm2Abis").orElse("arm64-v8a,x86_64").get().split(",") }
        externalNativeBuild { cmake {
            arguments += "-DANDROID_STL=c++_static"
            targets += "heavymach2"
        } }
    }
    flavorDimensions += "edition"
    productFlavors {
        for (edition in listOf("normal", "cheat")) {
            create(edition) {
                val cheat = edition == "cheat"
                dimension = "edition"
                if (cheat) applicationId = "com.gelb2.heavymach2.validation"
                resValue("string", "app_name", if (cheat) "HeavyMach 2 Test" else "HeavyMach 2")
                buildConfigField("boolean", "CHEAT", cheat.toString())
                externalNativeBuild { cmake { arguments += "-DHM2_CHEAT=" + if (cheat) "ON" else "OFF" } }
            }
        }
    }
    if (!privateSigning.isEmpty) {
        signingConfigs.create("installedRelease") {
            storeFile = repo.resolve(".local/android-signing/release.p12")
            storePassword = privateSigning.getProperty("storePassword")
            keyAlias = privateSigning.getProperty("keyAlias")
            keyPassword = storePassword
        }
        buildTypes.getByName("release").signingConfig = signingConfigs.getByName("installedRelease")
    }
    buildFeatures { buildConfig = true }
    externalNativeBuild { cmake { path = rootProject.file("cmake/CMakeLists.txt"); version = "3.22.1" } }
    sourceSets.getByName("main").assets.srcDir(assetsRoot)
    androidResources {
        ignoreAssetsPatterns += "manifest.json"
        noCompress += listOf("wav", "mp3")
    }
    packaging { jniLibs { useLegacyPackaging = false } }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
}

kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }

for (edition in listOf("normal", "cheat")) {
    val directory = layout.buildDirectory.dir("generated/launcher/$edition")
    val icon = tasks.register("generate${edition.replaceFirstChar(Char::uppercase)}Launcher") {
        dependsOn(iconFactory)
        val source = assetsRoot.resolve("Main_BG.png")
        val decoder = layout.buildDirectory.file("launcher/decoder")
        val png = directory.map { it.file("drawable-nodpi/launcher.png") }
        inputs.files(source, decoder)
        outputs.file(png)
        doLast {
            val process = ProcessBuilder(decoder.get().asFile.path, source.path).redirectError(ProcessBuilder.Redirect.INHERIT).start()
            val rgba = process.inputStream.use { it.readBytes() }
            check(process.waitFor() == 0 && rgba.size == 768 * 1024 * 4) { "Could not decode shared title artwork" }
            val crop = BufferedImage(768, 768, BufferedImage.TYPE_INT_RGB)
            for (y in 0 until 768) for (x in 0 until 768) {
                val offset = ((y + 128) * 768 + x) * 4
                val red = rgba[offset].toInt() and 255
                val green = rgba[offset + 1].toInt() and 255
                val blue = rgba[offset + 2].toInt() and 255
                crop.setRGB(x, y, (red shl 16) or (green shl 8) or blue)
            }
            val image = BufferedImage(192, 192, BufferedImage.TYPE_INT_RGB)
            val graphics = image.createGraphics()
            graphics.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC)
            graphics.drawImage(crop, 0, 0, 192, 192, null)
            if (edition == "cheat") {
                graphics.color = Color(24, 26, 31)
                graphics.fillRect(0, 160, 192, 32)
                graphics.color = Color.WHITE
                graphics.font = Font(Font.SANS_SERIF, Font.BOLD, 22)
                graphics.drawString("TEST", (192 - graphics.fontMetrics.stringWidth("TEST")) / 2, 184)
            }
            graphics.dispose()
            val output = png.get().asFile
            output.parentFile.mkdirs()
            ImageIO.write(image, "png", output)
        }
    }
    android.sourceSets.getByName(edition).res.srcDir(directory)
    tasks.configureEach {
        if (name.startsWith("pre${edition.replaceFirstChar(Char::uppercase)}") && name.endsWith("Build")) dependsOn(icon)
    }
}

tasks.named("preBuild") {
    doFirst { check(assetsRoot.resolve("manifest.json").isFile) { "Missing prepared campaign at $assetsRoot" } }
}

dependencies {
    testImplementation("junit:junit:4.13.2")
}
