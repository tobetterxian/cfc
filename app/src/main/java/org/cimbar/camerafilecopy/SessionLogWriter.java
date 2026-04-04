package org.cimbar.camerafilecopy;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;

import androidx.annotation.Nullable;

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
    private static final String EVENT_HEADER = "wall_ms,realtime_ns,tag,message";
    private static final Object EVENT_LOCK = new Object();
    private static volatile File activeEventLogFile;

    private final File sessionDir;
    private final BufferedWriter frameWriter;
    private final BufferedWriter eventWriter;
    private final long startedRealtimeNs;
    private final long startedWallMs;

    private NativeTelemetrySnapshot lastSnapshot = NativeTelemetrySnapshot.empty();
    private int framesSeen = 0;
    private int transferCompletions = 0;
    private boolean closed = false;

    private SessionLogWriter(File sessionDir, BufferedWriter frameWriter, BufferedWriter eventWriter,
                             long startedRealtimeNs, long startedWallMs) {
        this.sessionDir = sessionDir;
        this.frameWriter = frameWriter;
        this.eventWriter = eventWriter;
        this.startedRealtimeNs = startedRealtimeNs;
        this.startedWallMs = startedWallMs;
    }

    static SessionLogWriter open(Context context) throws IOException {
        long wallNow = System.currentTimeMillis();
        File root = benchmarkRoot(context);
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

        File eventFile = new File(sessionDir, "events.log");
        BufferedWriter eventWriter = new BufferedWriter(new FileWriter(eventFile, true));
        eventWriter.write(EVENT_HEADER);
        eventWriter.newLine();
        eventWriter.flush();

        activeEventLogFile = eventFile;
        SessionLogWriter writer = new SessionLogWriter(
                sessionDir,
                frameWriter,
                eventWriter,
                SystemClock.elapsedRealtimeNanos(),
                wallNow
        );
        writer.logEvent(TAG, "session-open root=" + root.getAbsolutePath());
        return writer;
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

    synchronized void logEvent(String tag, String message) {
        if (closed) {
            return;
        }
        appendEventLine(eventWriter, tag, message);
    }

    static void logGlobalEvent(@Nullable Context context, String tag, String message) {
        Log.i(tag, message);
        synchronized (EVENT_LOCK) {
            File target = activeEventLogFile;
            if (target == null && context != null) {
                File root = benchmarkRoot(context);
                if (!root.exists()) {
                    root.mkdirs();
                }
                target = new File(root, "adhoc-events.log");
                if (!target.exists()) {
                    try (BufferedWriter writer = new BufferedWriter(new FileWriter(target, true))) {
                        writer.write(EVENT_HEADER);
                        writer.newLine();
                    } catch (IOException e) {
                        Log.e(TAG, "Failed to initialize adhoc event log", e);
                    }
                }
            }
            if (target == null) {
                return;
            }
            try (BufferedWriter writer = new BufferedWriter(new FileWriter(target, true))) {
                appendEventLine(writer, tag, message);
            } catch (IOException e) {
                Log.e(TAG, "Failed to append global event", e);
            }
        }
    }

    @Override
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }
        logEvent(TAG, "session-close frames=" + framesSeen + " completed=" + transferCompletions);
        closed = true;
        writeSummary();
        frameWriter.close();
        eventWriter.close();
        if (activeEventLogFile != null && activeEventLogFile.equals(new File(sessionDir, "events.log"))) {
            activeEventLogFile = null;
        }
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

        try (BufferedWriter summaryWriter = new BufferedWriter(new FileWriter(new File(sessionDir, "summary.json")))) {
            try {
                summaryWriter.write(summary.toString(2));
            } catch (JSONException e) {
                throw new IOException("Failed to serialize JSON summary", e);
            }
            summaryWriter.newLine();
        }
    }

    File getSessionDir() {
        return sessionDir;
    }

    private static File benchmarkRoot(Context context) {
        File externalRoot = context.getExternalFilesDir(null);
        if (externalRoot != null) {
            return new File(externalRoot, "benchmarks/sessions");
        }
        return new File(context.getFilesDir(), "benchmarks/sessions");
    }

    private static void appendEventLine(BufferedWriter writer, String tag, String message) {
        try {
            writer.write(System.currentTimeMillis() + "," +
                    SystemClock.elapsedRealtimeNanos() + "," +
                    sanitizeCsv(tag) + "," +
                    sanitizeCsv(message));
            writer.newLine();
            writer.flush();
        } catch (IOException e) {
            Log.e(TAG, "Failed to append event line", e);
        }
    }

    private static String sanitizeCsv(String value) {
        if (value == null || value.isEmpty()) {
            return "";
        }
        return value.replace(",", "_").replace('\n', '_').replace('\r', '_');
    }
}
