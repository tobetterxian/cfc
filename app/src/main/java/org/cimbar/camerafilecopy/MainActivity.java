package org.cimbar.camerafilecopy;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;

import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.CompoundButton;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.view.GestureDetectorCompat;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewFrame;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewListener2;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.Mat;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity implements CvCameraViewListener2 {
    private static final String TAG = "MainActivity";
    private static final int CAMERA_PERMISSION_REQUEST = 1;
    private static final int CREATE_FILE = 11;
    private static final int MAX_DECODER_BACKLOG = 2;
    private static final String FORCE_LEGACY_CAMERA_FLAG = "force-legacy-camera";

    private GestureDetectorCompat mDetector;
    private Toast introToast;

    private CameraBridgeViewBase mOpenCvCameraView;
    private CameraBridgeViewBase mLegacyCameraView;
    private CameraBridgeViewBase mCamera2View;
    private ModeSelToggle mModeSwitch;
    private int modeVal = 0;
    private int detectedMode = 68;
    private String dataPath;
    private String activePath;
    private SessionLogWriter sessionLogger;

    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
        @Override
        public void onManagerConnected(int status) {
            if (status == LoaderCallbackInterface.SUCCESS) {
                Log.i(TAG, "OpenCV loaded successfully");

                // Load native library after(!) OpenCV initialization
                System.loadLibrary("cfc-cpp");

                if (mOpenCvCameraView != null) {
                    mOpenCvCameraView.enableView();
                }
            } else {
                super.onManagerConnected(status);
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "called onCreate");
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // Permissions for Android 6+
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.CAMERA},
                CAMERA_PERMISSION_REQUEST
        );

        this.dataPath = this.getFilesDir().getPath();
        //this.dataPath = this.getExternalFilesDir(null).getPath(); // for manual testing

        setContentView(R.layout.activity_main);
        ensureSessionLogger();
        mLegacyCameraView = findViewById(R.id.main_surface_legacy);
        mCamera2View = findViewById(R.id.main_surface_camera2);
        configureCameraView(mLegacyCameraView);
        configureCameraView(mCamera2View);
        selectCameraView(shouldUseCamera2());
        grantCameraPermissionIfAvailable();

        mModeSwitch = (ModeSelToggle) findViewById(R.id.mode_switch);
        mModeSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    modeVal = detectedMode;
                } else {
                    modeVal = 0;
                }
            }
        });

        // Set up the swipe gestures
        mDetector = new GestureDetectorCompat(this, new FlingGestureListener());

        // and the hint toast
        introToast = Toast.makeText(this, "↕ Swipe to encode data! Or use cimbar.org :)",  Toast.LENGTH_LONG);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == CAMERA_PERMISSION_REQUEST) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                grantCameraPermissionIfAvailable();
            } else {
                String message = "Camera permission was not granted";
                Log.e(TAG, message);
                Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            }
        } else {
            Log.e(TAG, "Unexpected permission request");
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        introToast.show();
        // reset autodetect
        mModeSwitch.setChecked(false);
        modeVal = 0;
    }

    @Override
    public void onPause() {
        closeSessionLogger();
        shutdownJNI();
        super.onPause();
        disableAllCameraViews();
    }

    @Override
    public void onResume() {
        super.onResume();
        ensureSessionLogger();
        if (!OpenCVLoader.initDebug()) {
            Log.d(TAG, "Internal OpenCV library not found. Using OpenCV Manager for initialization");
            OpenCVLoader.initAsync(OpenCVLoader.OPENCV_VERSION, this, mLoaderCallback);
        } else {
            Log.d(TAG, "OpenCV library found inside package. Using it!");
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }

    @Override
    public void onDestroy() {
        closeSessionLogger();
        shutdownJNI();
        super.onDestroy();
        disableAllCameraViews();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
    }

    @Override
    public void onCameraViewStopped() {
    }

    @Override
    public Mat onCameraFrame(CvCameraViewFrame frame) {
        long frameStartedNs = SystemClock.elapsedRealtimeNanos();
        // get current camera frame as OpenCV Mat object
        Mat mat = frame.rgba();
        NativeTelemetrySnapshot beforeTelemetry = NativeTelemetrySnapshot.fromWireString(getDecoderTelemetryJNI());

        if (beforeTelemetry.backlog >= MAX_DECODER_BACKLOG) {
            long frameFinishedNs = SystemClock.elapsedRealtimeNanos();
            if (sessionLogger != null) {
                sessionLogger.logFrame(frameStartedNs, frameFinishedNs, modeVal, detectedMode, "#drop_backlog", beforeTelemetry);
            }
            return mat;
        }

        // native call to process current camera frame
        String res = processImageJNI(mat.getNativeObjAddr(), this.dataPath, this.modeVal);
        NativeTelemetrySnapshot telemetry = NativeTelemetrySnapshot.fromWireString(getDecoderTelemetryJNI());
        long frameFinishedNs = SystemClock.elapsedRealtimeNanos();
        if (sessionLogger != null) {
            sessionLogger.logFrame(frameStartedNs, frameFinishedNs, modeVal, detectedMode, res, telemetry);
        }

        // res will contain a file path if we completed a transfer. Ask the user where to save it
        if (res.startsWith("/")) {
            detectedMode = parseDetectedModeResult(res);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mModeSwitch.setChecked(true);
                    mModeSwitch.setModeVal(detectedMode);
                }
            });

        }
        else if (!res.isEmpty()) {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            intent.putExtra(Intent.EXTRA_TITLE, res);
            // can't get putExtra to work for extra values, so we'll save it in the class
            this.activePath = this.dataPath + "/" + res;
            startActivityForResult(intent, CREATE_FILE);
        }

        // return processed frame for live preview
        return mat;
    }

    private int parseDetectedModeResult(String res) {
        if (res == null || res.length() < 2 || res.charAt(0) != '/') {
            return 68;
        }
        try {
            return Integer.parseInt(res.substring(1));
        } catch (NumberFormatException e) {
            Log.w(TAG, "Unexpected detected mode payload: " + res, e);
            return 68;
        }
    }

    private void configureCameraView(CameraBridgeViewBase cameraView) {
        if (cameraView == null) {
            return;
        }
        cameraView.setVisibility(View.GONE);
        cameraView.setCvCameraViewListener(this);
    }

    private void disableAllCameraViews() {
        if (mLegacyCameraView != null) {
            mLegacyCameraView.disableView();
        }
        if (mCamera2View != null) {
            mCamera2View.disableView();
        }
    }

    private void selectCameraView(boolean preferCamera2) {
        CameraBridgeViewBase preferred = preferCamera2 ? mCamera2View : mLegacyCameraView;
        CameraBridgeViewBase fallback = preferCamera2 ? mLegacyCameraView : mCamera2View;

        if (preferred == null) {
            preferred = fallback;
            fallback = null;
        }

        if (preferred == null) {
            throw new IllegalStateException("No camera views available");
        }

        if (fallback != null) {
            fallback.disableView();
            fallback.setVisibility(View.GONE);
        }

        preferred.setVisibility(SurfaceView.VISIBLE);
        mOpenCvCameraView = preferred;
        Log.i(TAG, "Selected camera path: " + (mOpenCvCameraView == mCamera2View ? "camera2" : "legacy"));
    }

    private boolean shouldUseCamera2() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return false;
        }
        return !(new File(getFilesDir(), FORCE_LEGACY_CAMERA_FLAG).exists());
    }

    private void grantCameraPermissionIfAvailable() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        if (mLegacyCameraView != null) {
            mLegacyCameraView.setCameraPermissionGranted();
        }
        if (mCamera2View != null) {
            mCamera2View.setCameraPermissionGranted();
        }
    }

    private void ensureSessionLogger() {
        if (sessionLogger != null) {
            return;
        }
        try {
            sessionLogger = SessionLogWriter.open(this);
        } catch (IOException e) {
            Log.e(TAG, "Failed to create session logger", e);
        }
    }

    private void closeSessionLogger() {
        if (sessionLogger == null) {
            return;
        }
        try {
            sessionLogger.updateLastSnapshot(NativeTelemetrySnapshot.fromWireString(getDecoderTelemetryJNI()));
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Native telemetry unavailable while closing session", e);
        }
        sessionLogger.closeQuietly();
        sessionLogger = null;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (resultCode == RESULT_OK && requestCode == CREATE_FILE) {
            if (this.activePath == null)
                return;

            // copy this.activePath (tempfile) to the user-specified location
            try (
                    InputStream istream = new FileInputStream(this.activePath);
                    OutputStream ostream = getContentResolver().openOutputStream(data.getData())
            ) {
                byte[] buf = new byte[8192];
                int length;
                while ((length = istream.read(buf)) > 0) {
                    ostream.write(buf, 0, length);
                }
                ostream.flush();
            } catch (Exception e) {
                Log.e(TAG, "failed to write file " + e.toString());
            } finally {
                try {
                    new File(this.activePath).delete();
                } catch (Exception e) {}
                this.activePath = null;
            }
        }
    }

    private native String processImageJNI(long mat, String path, int modeInt);
    private native String getDecoderTelemetryJNI();
    private native void shutdownJNI();

    @Override
    public boolean onTouchEvent(MotionEvent event){
        if (this.mDetector.onTouchEvent(event)) {
            return true;
        }
        return super.onTouchEvent(event);
    }
    @SuppressLint("ClickableViewAccessibility")
    class FlingGestureListener extends GestureDetector.SimpleOnGestureListener {
        private static final String TAG = "Gestures";

        // We only want fling gestures to trigger the view transitions, not scrolling.
        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2,
                               float velocityX, float velocityY) {
            final int THRESHOLD = 100;
            final int VEL_THRESHOLD = 100;

            if (Math.abs(velocityY) < VEL_THRESHOLD)
                return false;
            if (Math.abs(event1.getY() - event2.getY()) < THRESHOLD)
                return false;
            if (mOpenCvCameraView != null)
                mOpenCvCameraView.disableView();
            if (introToast != null)
                introToast.cancel();

            Intent intent = new Intent(MainActivity.this, WebViewActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            return true;
        }
    }

}

