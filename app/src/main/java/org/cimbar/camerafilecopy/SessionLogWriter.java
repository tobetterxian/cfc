package org.cimbar.camerafilecopy;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedWriter;
import java.io.Closeable;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

final class SessionLogWriter implements Closeable {
    private static final String TAG = "SessionLogWriter";
    private static final String CSV_HEADER =
            "frame_started_ns,frame_finished_ns,frame_duration_us,requested_mode,detected_mode,result," +
            "calls,scanned,decoded,perfect,bytes,scan_ms,extract_ms,decode_ms,backlog,files_in_flight,files_decoded";

    private final File sessionDir;
    private final BufferedWriter frameWriter;
    private final long startedRealtimeNs;
    private final long startedWallMs;

    private NativeTelemetrySnapshot lastSnapshot = NativeTelemetrySnapshot.empty();
    private int framesSeen = 0;
    private int transferCompletions = 0;
    private boolean closed = false;

    private SessionLogWriter(File sessionDir, BufferedWriter frameWriter, long startedRealtimeNs, long startedWallMs) {
        this.sessionDir = sessionDir;
        this.frameWriter = frameWriter;
        this.startedRealtimeNs = startedRealtimeNs;
        this.startedWallMs = startedWallMs;
    }

    static SessionLogWriter open(Context context) throws IOException {
        long wallNow = System.currentTimeMillis();
        File root = new File(context.getFilesDir(), "benchmarks/sessions");
        if (!root.exists() && !root.mkdirs()) {
            throw new IOException("Failed to create benchmark session root: " + root);
        }

        File sessionDir = new File(root, Long.toString(wallNow));
        int suffix = 0;
        while (sessionDir.exists()) {
            suffix++;
            sessionDir = new File(root, wallNow + "-" + suffix);
        }
        if (!sessionDir.mkdirs()) {
            throw new IOException("Failed to create benchmark session directory: " + sessionDir);
        }

        BufferedWriter frameWriter = new BufferedWriter(new FileWriter(new File(sessionDir, "frames.csv")));
        frameWriter.write(CSV_HEADER);
        frameWriter.newLine();
        frameWriter.flush();
        return new SessionLogWriter(sessionDir, frameWriter, SystemClock.elapsedRealtimeNanos(), wallNow);
    }

    synchronized void logFrame(long frameStartedNs, long frameFinishedNs, int requestedMode,
                               int detectedMode, String result, NativeTelemetrySnapshot snapshot) {
        if (closed) {
            return;
        }

        lastSnapshot = snapshot;
        framesSeen++;
        if (result != null && !result.isEmpty() && !result.startsWith("/")) {
            transferCompletions++;
        }

        long frameDurationUs = (frameFinishedNs - frameStartedNs) / 1000L;
        try {
            frameWriter.write(frameStartedNs + "," +
                    frameFinishedNs + "," +
                    frameDurationUs + "," +
                    requestedMode + "," +
                    detectedMode + "," +
                    sanitizeCsv(result) + "," +
                    snapshot.calls + "," +
                    snapshot.scanned + "," +
                    snapshot.decoded + "," +
                    snapshot.perfect + "," +
                    snapshot.bytes + "," +
                    snapshot.scanMillis + "," +
                    snapshot.extractMillis + "," +
                    snapshot.decodeMillis + "," +
                    snapshot.backlog + "," +
                    snapshot.filesInFlight + "," +
                    snapshot.filesDecoded);
            frameWriter.newLine();
            frameWriter.flush();
        } catch (IOException e) {
            Log.e(TAG, "Failed to append frame telemetry", e);
        }
    }

    synchronized void updateLastSnapshot(NativeTelemetrySnapshot snapshot) {
        if (snapshot != null) {
            lastSnapshot = snapshot;
        }
    }

    @Override
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }
        closed = true;
        writeSummary();
        frameWriter.close();
    }

    void closeQuietly() {
        try {
            close();
        } catch (IOException e) {
            Log.e(TAG, "Failed to close session logger", e);
        }
    }

    private void writeSummary() throws IOException {
        long durationMs = (SystemClock.elapsedRealtimeNanos() - startedRealtimeNs) / 1_000_000L;
        JSONObject summary = new JSONObject();
        JSONObject decoder = new JSONObject();
        try {
            summary.put("started_wall_ms", startedWallMs);
            summary.put("duration_ms", durationMs);
            summary.put("frames_seen", framesSeen);
            summary.put("transfers_completed", transferCompletions);
            summary.put("session_dir", sessionDir.getAbsolutePath());

            decoder.put("calls", lastSnapshot.calls);
            decoder.put("scanned", lastSnapshot.scanned);
            decoder.put("decoded", lastSnapshot.decoded);
            decoder.put("perfect", lastSnapshot.perfect);
            decoder.put("bytes", lastSnapshot.bytes);
            decoder.put("scan_ms", lastSnapshot.scanMillis);
            decoder.put("extract_ms", lastSnapshot.extractMillis);
            decoder.put("decode_ms", lastSnapshot.decodeMillis);
            decoder.put("backlog", lastSnapshot.backlog);
            decoder.put("mode", lastSnapshot.mode);
            decoder.put("detected_mode", lastSnapshot.detectedMode);
            decoder.put("files_in_flight", lastSnapshot.filesInFlight);
            decoder.put("files_decoded", lastSnapshot.filesDecoded);
            summary.put("decoder", decoder);
        } catch (JSONException e) {
            throw new IOException("Failed to build JSON summary", e);
        }

        BufferedWriter summaryWriter = new BufferedWriter(new FileWriter(new File(sessionDir, "summary.json")));
        summaryWriter.write(summary.toString(2));
        summaryWriter.newLine();
        summaryWriter.close();
    }

    private static String sanitizeCsv(String value) {
        if (value == null || value.isEmpty()) {
            return "";
        }
        return value.replace(",", "_").replace('\n', '_').replace('\r', '_');
    }
}
