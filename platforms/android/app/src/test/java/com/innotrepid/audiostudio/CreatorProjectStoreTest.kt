package com.innotrepid.audiostudio

import android.content.Context
import android.content.ContextWrapper
import androidx.test.core.app.ApplicationProvider
import java.io.File
import java.io.IOException
import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CreatorProjectStoreTest {
    private fun isolatedContext(): Pair<Context, File> {
        val root = Files.createTempDirectory("creator-project-test").toFile()
        val application = ApplicationProvider.getApplicationContext<Context>()
        return object : ContextWrapper(application) {
            override fun getFilesDir(): File = root
        } to root
    }

    @Test
    fun projectAndScriptTextRoundTrip() {
        val (context, root) = isolatedContext()
        try {
            val store = CreatorProjectStore(context)
            val created = store.loadOrCreate()
            val changed = created.copy(
                title = "Studio session",
                scripts = created.scripts.map { script ->
                    script.copy(sections = script.sections.map { it.copy(text = "A saved script line.") })
                }
            )
            store.save(changed)

            val loaded = CreatorProjectStore(context).loadOrCreate()
            assertEquals("Studio session", loaded.title)
            assertEquals("A saved script line.", loaded.scripts.first().sections.first().text)
            val draftCaptions = store.generateDraftCaptions("One two three four five six seven", 3)
            val captionProject = loaded.copy(transcript = "One two three four five six seven", captions = draftCaptions)
            store.save(captionProject)
            val captionReload = CreatorProjectStore(context).loadOrCreate()
            assertEquals("One two three four five six seven", captionReload.transcript)
            assertEquals(3, captionReload.captions.size)
            assertTrue(store.exportSrt(captionReload.captions).contains("00:00:00,000 -->"))
            assertTrue(store.exportWebVtt(captionReload.captions).startsWith("WEBVTT"))
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun malformedPrimaryRecoversLastValidGeneration() {
        val (context, root) = isolatedContext()
        try {
            val store = CreatorProjectStore(context)
            val initial = store.loadOrCreate()
            store.save(initial.copy(title = "Newer generation"))

            val primary = File(root, "creator/projects/default-project.json")
            primary.writeText("{ malformed")
            val recovered = CreatorProjectStore(context).loadOrCreate()

            assertEquals("My first project", recovered.title)
            assertTrue(primary.readText().contains("My first project"))
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun malformedPrimaryWithoutRecoveryIsPreserved() {
        val (context, root) = isolatedContext()
        try {
            val primary = File(root, "creator/projects/default-project.json")
            primary.parentFile?.mkdirs()
            val malformed = "{ definitely not valid JSON"
            primary.writeText(malformed)

            var failed = false
            try {
                CreatorProjectStore(context).loadOrCreate()
            } catch (_: IOException) {
                failed = true
            }

            assertTrue("Loading malformed data must fail safely.", failed)
            assertEquals(malformed, primary.readText())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun invalidTakeScriptReferenceIsRejected() {
        val (context, root) = isolatedContext()
        try {
            val store = CreatorProjectStore(context)
            val project = store.loadOrCreate()
            val invalidTake = org.json.JSONObject()
                .put("takeId", "test-take")
                .put("sourcePath", "/recordings/take.wav")
                .put("durationMs", 1200)
                .put("sampleRateHz", 48000)
                .put("channels", 1)
                .put("scriptId", "missing-script")
                .put("sectionId", "missing-section")

            var failed = false
            try {
                store.save(project.copy(takes = project.takes + invalidTake))
            } catch (_: IllegalArgumentException) {
                failed = true
            }

            assertTrue("A take referencing a missing script must be rejected.", failed)
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun invalidCaptionTimingIsRejected() {
        val (context, root) = isolatedContext()
        try {
            val store = CreatorProjectStore(context)
            val project = store.loadOrCreate()
            val invalid = CreatorProjectStore.Caption("caption-1", 2_000, 1_000, "Invalid timing")
            var failed = false
            try { store.save(project.copy(captions = listOf(invalid))) }
            catch (_: IllegalArgumentException) { failed = true }
            assertTrue("Caption end time must follow its start time.", failed)
        } finally { root.deleteRecursively() }
    }

    @Test
    fun unsupportedSchemaIsRejectedWithoutOverwritingFile() {
        val (context, root) = isolatedContext()
        try {
            val primary = File(root, "creator/projects/default-project.json")
            primary.parentFile?.mkdirs()
            val unsupported = """{"schemaVersion":99}"""
            primary.writeText(unsupported)

            var failed = false
            try {
                CreatorProjectStore(context).loadOrCreate()
            } catch (_: IOException) {
                failed = true
            }

            assertTrue("Unknown schema versions must be rejected.", failed)
            assertEquals(unsupported, primary.readText())
        } finally {
            root.deleteRecursively()
        }
    }
}
