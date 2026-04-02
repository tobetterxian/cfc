package org.cimbar.camerafilecopy;

final class NativeTelemetrySnapshot {
    private static final int FIELD_COUNT = 13;

    final long calls;
    final long scanned;
    final long decoded;
    final long perfect;
    final long bytes;
    final long scanMillis;
    final long extractMillis;
    final long decodeMillis;
    final int backlog;
    final int mode;
    final int detectedMode;
    final int filesInFlight;
    final int filesDecoded;

    NativeTelemetrySnapshot(long calls, long scanned, long decoded, long perfect, long bytes,
                            long scanMillis, long extractMillis, long decodeMillis,
                            int backlog, int mode, int detectedMode,
                            int filesInFlight, int filesDecoded) {
        this.calls = calls;
        this.scanned = scanned;
        this.decoded = decoded;
        this.perfect = perfect;
        this.bytes = bytes;
        this.scanMillis = scanMillis;
        this.extractMillis = extractMillis;
        this.decodeMillis = decodeMillis;
        this.backlog = backlog;
        this.mode = mode;
        this.detectedMode = detectedMode;
        this.filesInFlight = filesInFlight;
        this.filesDecoded = filesDecoded;
    }

    static NativeTelemetrySnapshot empty() {
        return new NativeTelemetrySnapshot(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    static NativeTelemetrySnapshot fromWireString(String wire) {
        if (wire == null || wire.isEmpty()) {
            return empty();
        }

        String[] fields = wire.split("\\|", -1);
        if (fields.length != FIELD_COUNT) {
            return empty();
        }

        return new NativeTelemetrySnapshot(
                parseLong(fields[0]),
                parseLong(fields[1]),
                parseLong(fields[2]),
                parseLong(fields[3]),
                parseLong(fields[4]),
                parseLong(fields[5]),
                parseLong(fields[6]),
                parseLong(fields[7]),
                parseInt(fields[8]),
                parseInt(fields[9]),
                parseInt(fields[10]),
                parseInt(fields[11]),
                parseInt(fields[12])
        );
    }

    private static long parseLong(String value) {
        try {
            return Long.parseLong(value);
        } catch (NumberFormatException e) {
            return 0L;
        }
    }

    private static int parseInt(String value) {
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return 0;
        }
    }
}
