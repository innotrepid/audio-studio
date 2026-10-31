package com.innotrepid.audiostudio

import android.content.Context
import android.util.AtomicFile
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.FileOutputStream
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Instant
import java.util.UUID

/** Stores named script snapshots separately from the active project so takes are never copied. */
internal class CreatorDraftStore(context: Context) {
    data class Draft(
        val draftId: String,
        val name: String,
        val createdUtc: String,
        val updatedUtc: String,
        val script: CreatorProjectStore.Script
    )
    data class Document(val drafts: List<Draft>, val activeDraftId: String)

    private val file = File(context.filesDir, "creator/drafts/drafts.json")
    private val atomic = AtomicFile(file)
    private val previousFile = File(file.parentFile, file.name + ".previous")
    private val maxBytes = 1_048_576
    private val maxDrafts = 100

    @Synchronized
    @Throws(IOException::class)
    fun load(): Document? {
        if (!file.exists() && !previousFile.exists() && !File(file.path + ".bak").exists()) return null
        var primaryError: Exception? = null
        if (file.exists() || File(file.path + ".bak").exists()) {
            try {
                val bytes = readPrimaryBytes()
                return decode(String(bytes, StandardCharsets.UTF_8))
            } catch (error: Exception) {
                primaryError = error
            }
        }
        if (previousFile.exists()) {
            try {
                val recovered = decode(previousFile.readText(StandardCharsets.UTF_8))
                writePrimary(encode(recovered).toByteArray(StandardCharsets.UTF_8))
                return recovered
            } catch (backupError: Exception) {
                throw IOException(
                    "The named drafts and their recovery copy are invalid. Both files were preserved. Primary: " + (primaryError?.message ?: "missing") + ". Recovery: " + backupError.message,
                    backupError
                )
            }
        }
        throw IOException(
            "The saved named drafts are invalid and were preserved: " + (primaryError?.message ?: "unknown error"),
            primaryError
        )
    }

    private fun readPrimaryBytes(): ByteArray {
        return atomic.openRead().use { input ->
            val out = ByteArrayOutputStream()
            val buffer = ByteArray(8192)
            var total = 0
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                total += count
                if (total > maxBytes) throw IOException("Draft data exceeds the 1 MB safety limit.")
                out.write(buffer, 0, count)
            }
            out.toByteArray()
        }
    }

    @Synchronized
    @Throws(IOException::class)
    fun save(document: Document): Document {
        file.parentFile?.mkdirs()
        validate(document)
        val bytes = encode(document).toByteArray(StandardCharsets.UTF_8)
        if (bytes.size > maxBytes) throw IOException("Draft data exceeds the 1 MB safety limit.")
        if (file.exists()) {
            try {
                val current = readPrimaryBytes()
                decode(String(current, StandardCharsets.UTF_8))
                writePrevious(current)
            } catch (_: Exception) {
                // Keep the last known-good previous generation if the primary is already damaged.
            }
        }
        writePrimary(bytes)
        return document
    }

    private fun writePrevious(bytes: ByteArray) {
        previousFile.parentFile?.mkdirs()
        val temporary = File(file.parentFile, file.name + ".previous.tmp")
        FileOutputStream(temporary).use { output ->
            output.write(bytes)
            output.fd.sync()
        }
        Files.move(
            temporary.toPath(), previousFile.toPath(),
            StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING
        )
    }

    private fun writePrimary(bytes: ByteArray) {
        var stream: java.io.FileOutputStream? = null
        try {
            stream = atomic.startWrite()
            stream.write(bytes)
            stream.fd.sync()
            atomic.finishWrite(stream)
        } catch (error: Exception) {
            if (stream != null) atomic.failWrite(stream)
            throw IOException("Could not safely save named drafts: \${error.message}", error)
        }
    }

    fun create(name: String, script: CreatorProjectStore.Script, current: Document?): Document {
        val cleanName = name.trim()
        require(cleanName.isNotEmpty() && cleanName.length <= 200) { "Draft name must contain 1–200 characters." }
        val now = Instant.now().toString()
        val draft = Draft(UUID.randomUUID().toString(), cleanName, now, now, script)
        val drafts = (current?.drafts.orEmpty() + draft)
        require(drafts.size <= maxDrafts) { "You can keep up to 100 drafts." }
        return Document(drafts, draft.draftId)
    }

    fun update(document: Document, draftId: String, name: String, script: CreatorProjectStore.Script): Document {
        val cleanName = name.trim()
        require(cleanName.isNotEmpty() && cleanName.length <= 200) { "Draft name must contain 1–200 characters." }
        require(document.drafts.any { it.draftId == draftId }) { "Selected draft no longer exists." }
        val now = Instant.now().toString()
        return document.copy(drafts = document.drafts.map {
            if (it.draftId == draftId) it.copy(name = cleanName, updatedUtc = now, script = script) else it
        })
    }

    fun delete(document: Document, draftId: String, nextActiveId: String?): Document {
        require(document.drafts.size > 1) { "The last draft cannot be deleted." }
        require(document.drafts.any { it.draftId == draftId }) { "Selected draft no longer exists." }
        val remaining = document.drafts.filterNot { it.draftId == draftId }
        val active = if (document.activeDraftId == draftId) nextActiveId else document.activeDraftId
        require(active != null && remaining.any { it.draftId == active }) { "Choose an existing draft before deleting the active draft." }
        return Document(remaining, active)
    }

    private fun validate(doc: Document) {
        require(doc.drafts.isNotEmpty() && doc.drafts.size <= maxDrafts) { "Draft count must be 1–100." }
        val ids = HashSet<String>()
        doc.drafts.forEach { draft ->
            require(draft.draftId.isNotBlank() && ids.add(draft.draftId)) { "Draft IDs must be unique." }
            require(draft.name.isNotBlank() && draft.name.length <= 200) { "Draft names must contain 1–200 characters." }
            require(draft.script.scriptId.isNotBlank() && draft.script.sections.isNotEmpty()) { "Draft script is invalid." }
        }
        require(doc.drafts.any { it.draftId == doc.activeDraftId }) { "Active draft must exist." }
    }

    private fun encode(doc: Document): String {
        val drafts = JSONArray()
        doc.drafts.forEach { d ->
            val sections = JSONArray()
            d.script.sections.sortedBy { it.order }.forEach { s ->
                sections.put(JSONObject().put("sectionId", s.sectionId).put("title", s.title)
                    .put("text", s.text).put("order", s.order).put("notes", s.notes)
                    .put("cues", JSONArray(s.cues)))
            }
            val script = JSONObject().put("scriptId", d.script.scriptId).put("title", d.script.title)
                .put("createdUtc", d.script.createdUtc).put("updatedUtc", d.script.updatedUtc)
                .put("sections", sections)
            drafts.put(JSONObject().put("draftId", d.draftId).put("name", d.name)
                .put("createdUtc", d.createdUtc).put("updatedUtc", d.updatedUtc).put("script", script))
        }
        return JSONObject().put("schemaVersion", 1).put("activeDraftId", doc.activeDraftId)
            .put("drafts", drafts).toString(2)
    }

    private fun decode(raw: String): Document {
        val root = JSONObject(raw)
        require(root.optInt("schemaVersion") == 1) { "Unsupported draft schema." }
        val drafts = mutableListOf<Draft>()
        val array = root.getJSONArray("drafts")
        require(array.length() in 1..maxDrafts) { "Draft count must be 1–100." }
        for (i in 0 until array.length()) {
            val item = array.getJSONObject(i)
            val obj = item.getJSONObject("script")
            val sectionsJson = obj.getJSONArray("sections")
            val sections = (0 until sectionsJson.length()).map { j ->
                val s = sectionsJson.getJSONObject(j)
                val cues = s.optJSONArray("cues") ?: JSONArray()
                CreatorProjectStore.Section(
                    s.getString("sectionId"), s.getString("title"), s.optString("text", ""),
                    s.optInt("order"), s.optString("notes", ""),
                    (0 until cues.length()).map { cues.getString(it) }
                )
            }
            val script = CreatorProjectStore.Script(obj.getString("scriptId"), obj.getString("title"),
                obj.optString("createdUtc").takeIf { it.isNotBlank() && it != "null" } ?: item.getString("createdUtc"), obj.optString("updatedUtc").takeIf { it.isNotBlank() && it != "null" } ?: item.getString("updatedUtc"), sections)
            drafts += Draft(item.getString("draftId"), item.getString("name"), item.getString("createdUtc"),
                item.getString("updatedUtc"), script)
        }
        return Document(drafts, root.getString("activeDraftId")).also(::validate)
    }
}
