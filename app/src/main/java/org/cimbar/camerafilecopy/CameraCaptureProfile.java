package org.cimbar.camerafilecopy;

final class CameraCaptureProfile {
    final String name;
    final int targetFps;
    final long frameDurationNs;
    final long exposureTimeNs;
    final int sensorSensitivity;
    final boolean useManualSensor;
    final boolean lockAuto3A;

    private CameraCaptureProfile(String name, int targetFps, long frameDurationNs,
                                 long exposureTimeNs, int sensorSensitivity,
                                 boolean useManualSensor, boolean lockAuto3A) {
        this.name = name;
        this.targetFps = targetFps;
        this.frameDurationNs = frameDurationNs;
        this.exposureTimeNs = exposureTimeNs;
        this.sensorSensitivity = sensorSensitivity;
        this.useManualSensor = useManualSensor;
        this.lockAuto3A = lockAuto3A;
    }

    static CameraCaptureProfile balanced() {
        return new CameraCaptureProfile(
                "balanced",
                30,
                33_333_333L,
                12_000_000L,
                400,
                false,
                true
        );
    }

    static CameraCaptureProfile throughput() {
        return new CameraCaptureProfile(
                "throughput",
                60,
                16_666_666L,
                8_000_000L,
                640,
                true,
                false
        );
    }

    static CameraCaptureProfile robust() {
        return new CameraCaptureProfile(
                "robust",
                24,
                41_666_666L,
                16_000_000L,
                800,
                false,
                true
        );
    }

    String describe() {
        return name +
                " targetFps=" + targetFps +
                " exposureNs=" + exposureTimeNs +
                " frameDurationNs=" + frameDurationNs +
                " iso=" + sensorSensitivity +
                " manualSensor=" + useManualSensor +
                " lockAuto3A=" + lockAuto3A;
    }
}
