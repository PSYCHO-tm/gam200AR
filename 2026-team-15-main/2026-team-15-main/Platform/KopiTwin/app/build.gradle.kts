import java.io.File
import java.util.zip.ZipFile

plugins {
    alias(libs.plugins.android.application)
}

val arcoreVersion = "1.56.0"
val arcoreLibPath = file("../../../Extern/ARCore/$arcoreVersion")

val natives by configurations.creating

val extractNativeLibraries by tasks.registering(Copy::class) {
    from({ natives.files.map { zipTree(it) } })
    include("jni/**/*")
    into(arcoreLibPath)
}

// make sure the unpacking happens before CMake runs
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
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    implementation("com.google.ar:core:$arcoreVersion")
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
    natives("com.google.ar:core:$arcoreVersion")
}

natives.files.forEach { aar ->
    ZipFile(aar).use { zip ->
        for (entry in zip.entries().asSequence()) {
            if (entry.isDirectory || !entry.name.startsWith("jni/")) continue
            val target = File(arcoreLibPath, entry.name)
            if (target.exists() && target.length() == entry.size) continue   // already unpacked
            target.parentFile.mkdirs()
            zip.getInputStream(entry).use { input ->
                target.outputStream().use { output -> input.copyTo(output) }
            }
        }
    }
}