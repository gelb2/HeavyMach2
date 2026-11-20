package com.gelb2.heavymach2

import android.util.Log
import org.json.JSONObject
import java.io.File

internal object TestControl {
    private external fun execute(handle: Long, command: String): String

    fun run(handle: Long, command: String, request: String, directory: File) {
        val response = JSONObject().put("command", command).put("request", request)
        try {
            response.put("result", JSONObject(execute(handle, command)))
        } catch (error: RuntimeException) {
            response.put("error", error.message)
            Log.e("HM2Validation", "Validation command failed: $command", error)
        }
        val pending = File(directory, "validation.json.tmp")
        pending.writeText(response.toString())
        check(pending.renameTo(File(directory, "validation.json")))
        Log.i("HM2Validation", command + " " + response.toString().take(1800))
    }
}
