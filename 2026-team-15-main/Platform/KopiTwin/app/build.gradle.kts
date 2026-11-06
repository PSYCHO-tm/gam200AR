plugins {
    alias(libs.plugins.android.application)
}

val arcoreVersion = "1.56.0"
val arcoreLibPath = file("../../../Extern/ARCore-natives/$arcoreVersion")

// A separate list of dependencies, used only to download the ARCore AAR for extraction.
// It is empty until natives("com.google.ar:core:...") adds ARCore below.
val natives by configurations.creating

// Unzips the AAR (a zip file) and copies everything under its jni/ folder
// (the native .so libraries) into arcoreLibPath, where CMake looks for them.
val extractNativeLibraries by tasks.registering(Copy::class) {
    from({ natives.files.map { zipTree(it) } })
    include("jni/**/*")
    into(arcoreLibPath)
}

// Makes every CMake build step wait for extractNativeLibraries, so libarcore_sdk_c.so
// exists before linking. Matches tasks by name prefix; skips Clean tasks.
tasks.matching {
    (it.name.startsWith("configureCMake") || it.name.startsWith("buildCMake") ||
            it.name.startsWith("generateJsonModel") || it.name.startsWith("externalNativeBuild")) &&
            !it.name.contains("Clean")
}.configureEach { dependsOn(extractNativeLibraries) }


android {
    namespace = "com.example.kopitwin"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.example.kopitwin"
        minSdk = 24
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk { abiFilters += listOf("arm64-v8a") }
    }

    buildTypes {
        release {
            optimization {
                enable = true
                packageScope = setOf("androidx.**", "kotlin.**", "kotlinx.**")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        viewBinding = true
    }

    sourceSets.named("main") {
        // extra folder of Java files for Gradle to compile (path is relative to the app folder)
        java.directories.add("../../../Extern/SDL3/android-project/app/src/main/java")
	// Goal 5: the repo's Assets/ folder goes into the APK
	assets.directories.add("../../../Assets")
    }
}

dependencies {
    natives("com.google.ar:core:$arcoreVersion") // Pulls ARCore .so file from Google Maven
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    implementation("com.google.ar:core:$arcoreVersion") // Specify the utilisation of the ARCore Library
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}