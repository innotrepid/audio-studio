plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.innotrepid.audiostudio"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.innotrepid.Synvidra"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        // DeepFilterNet3's ONNX Runtime binaries are available for 64-bit Android.
        // RNNoise and the app share this ABI set so the native library is always present.
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-Wall", "-Wextra")
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    sourceSets {
        getByName("main") {
            // The model files are bundled from the pinned noise-cancellation-model checkout.
            assets.srcDir("../../../vendor/noise-cancellation-model/models")
            // Whisper + Silero offline models (copied by copyTranscriptionModels from build/).
            assets.srcDir("src/main/assetsGenerated")
        }
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
    }
}

// App module is platforms/android/app → repo root is ../../..
val transcriptionModelsDir = projectDir.resolve("../../../build/transcription-models")
val generatedAssetsDir = layout.projectDirectory.dir("src/main/assetsGenerated")

val copyTranscriptionModels by tasks.registering(Copy::class) {
    description = "Copy verified Whisper and Silero model files into APK assets/models"
    val whisper = transcriptionModelsDir.resolve("ggml-tiny.bin")
    val silero = transcriptionModelsDir.resolve("silero_vad.onnx")
    doFirst {
        if (!whisper.isFile || !silero.isFile) {
            throw GradleException(
                "Missing offline models under ${transcriptionModelsDir.canonicalFile}. " +
                    "Run: python scripts/fetch_transcription_models.py --output build/transcription-models"
            )
        }
    }
    from(transcriptionModelsDir) {
        include("ggml-tiny.bin", "silero_vad.onnx")
        into("models")
    }
    into(generatedAssetsDir)
}

tasks.named("preBuild").configure {
    dependsOn(copyTranscriptionModels)
}

dependencies {
    implementation("com.google.oboe:oboe:1.11.0")
    // Pinned CPU ONNX Runtime; its native Android libraries are packaged in the APK.
    implementation("com.microsoft.onnxruntime:onnxruntime-android:1.23.2")

    testImplementation("junit:junit:4.13.2")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("org.robolectric:robolectric:4.14.1")
}
