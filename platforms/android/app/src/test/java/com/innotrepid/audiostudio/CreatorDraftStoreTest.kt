package com.innotrepid.audiostudio

import android.content.Context
import android.content.ContextWrapper
import androidx.test.core.app.ApplicationProvider
import java.io.File
import java.io.IOException
import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CreatorDraftStoreTest {
    private fun isolatedContext(): Pair<Context, File> {
        val root = Files.createTempDirectory("creator-draft-test").toFile()
        val application = ApplicationProvider.getApplicationContext<Context>()
        return object : ContextWrapper(application) {
            override fun getFilesDir(): File = root
        } to root
    }

    @Test
    fun namedDraftsRoundTripAndRecoverPreviousValidGeneration() {
        val (context, root) = isolatedContext()
        try {
            val script = CreatorProjectStore(context).loadOrCreate().scripts.first()
            val store = CreatorDraftStore(context)
            val first = store.create("First draft", script, null)
            val second = store.create("Second draft", script, first)
            store.save(second)
            val renamed = store.update(second, second.activeDraftId, "Renamed second", script)
            store.save(renamed)

            val primary = File(root, "creator/drafts/drafts.json")
            primary.writeText("{ malformed")
            val recovered = CreatorDraftStore(context).load()

            assertEquals(2, recovered?.drafts?.size)
            assertTrue(recovered!!.drafts.any { it.name == "Second draft" })
            assertTrue(primary.readText().contains("Second draft"))
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun malformedDraftWithoutRecoveryIsPreserved() {
        val (context, root) = isolatedContext()
        try {
            val primary = File(root, "creator/drafts/drafts.json")
            primary.parentFile?.mkdirs()
            val malformed = "{ definitely not valid JSON"
            primary.writeText(malformed)

            var failed = false
            try { CreatorDraftStore(context).load() }
            catch (_: IOException) { failed = true }

            assertTrue("Malformed named drafts must fail safely.", failed)
            assertEquals(malformed, primary.readText())
        } finally {
            root.deleteRecursively()
        }
    }
}
